[CmdletBinding(PositionalBinding = $false)]
param(
    [ValidatePattern('^[A-Za-z0-9_-]+$')]
    [string]$Example = "blink",

    [ValidateSet("pic10f200", "pic10f202")]
    [string]$Device = "pic10f200",

    [ValidateSet("Flash", "Reset", "Debug")]
    [string]$Action = "Flash",

    [string]$OpenOcd,

    [string]$OpenOcdScripts,

    [string]$Interface = "interface/cmsis-dap.cfg",

    [switch]$NoBuild,

    [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
    [string[]]$MakeArgs
)

$ErrorActionPreference = "Stop"

$OpenOcdUrl = "https://gitee.com/puya-semiconductor/tools-and-software/raw/" +
    "d503385bb4f9f5a51d7d5a5913d566fde9b66652/" +
    "PY32_GCC/openocd-0.12.0.zip"
$OpenOcdSha256 = "F2CB432E5C6AC65FA3B26F6A8441F3F54E53242335FBE5DA3B075CDF06152058"

$PortRoot = Split-Path -Parent $PSScriptRoot
$RepoRoot = Split-Path -Parent (Split-Path -Parent $PortRoot)
$ExampleSource = Join-Path $RepoRoot "examples\$Example\main.c"
$TargetConfig = Join-Path $PortRoot `
    "vendor\Core\OpenOCD\py32f002a_target.cfg"
$Firmware = Join-Path $PortRoot `
    "build\$Example\picemu-py32f002a.elf"

if (-not (Test-Path -LiteralPath $ExampleSource)) {
    throw "PIC example not found: $ExampleSource"
}

if (-not $NoBuild -and $Action -in @("Flash", "Debug")) {
    $Wsl = Get-Command "wsl.exe" -ErrorAction SilentlyContinue
    if ($null -eq $Wsl) {
        throw "wsl.exe was not found. Install WSL or build elsewhere and use -NoBuild."
    }

    $BuildArguments = @("EXAMPLE=$Example", "DEVICE=$Device")
    foreach ($Argument in $MakeArgs) {
        if ($Argument -notmatch '^[A-Za-z_][A-Za-z0-9_]*=.*$') {
            throw "Invalid Make argument '$Argument'. Expected NAME=value."
        }
        $BuildArguments += $Argument
    }

    Write-Host "[1/2] Building PY32F002A firmware in WSL..."
    $BuildCommand = "make py32 " + ($BuildArguments -join " ")
    & $Wsl.Source --cd $RepoRoot bash -ic $BuildCommand
    if ($LASTEXITCODE -ne 0) {
        throw "WSL build failed (exit code $LASTEXITCODE)."
    }
}

if ($Action -in @("Flash", "Debug") -and
    -not (Test-Path -LiteralPath $Firmware)) {
    throw "Firmware was not generated: $Firmware"
}

if ([string]::IsNullOrWhiteSpace($OpenOcd)) {
    $LocalOpenOcd = Join-Path $PortRoot "tools\openocd\bin\openocd.exe"
    if (-not (Test-Path -LiteralPath $LocalOpenOcd)) {
        $ToolsRoot = Join-Path $PortRoot "tools"
        $InstallRoot = Join-Path $ToolsRoot ".openocd-install"
        $Archive = Join-Path $InstallRoot "openocd-0.12.0.zip"
        $Expanded = Join-Path $InstallRoot "openocd-0.12.0"

        if (Test-Path -LiteralPath $InstallRoot) {
            Remove-Item -Recurse -Force -LiteralPath $InstallRoot
        }
        New-Item -ItemType Directory -Force -Path $InstallRoot | Out-Null

        Write-Host "OpenOCD for PY32F002A was not found. Downloading the official package..."
        try {
            Invoke-WebRequest -Uri $OpenOcdUrl -OutFile $Archive -UseBasicParsing
            $ActualHash = (Get-FileHash -Algorithm SHA256 $Archive).Hash
            if ($ActualHash -ne $OpenOcdSha256) {
                throw "OpenOCD SHA-256 mismatch: expected $OpenOcdSha256, got $ActualHash"
            }
            Expand-Archive -LiteralPath $Archive -DestinationPath $InstallRoot
            if (-not (Test-Path -LiteralPath (Join-Path $Expanded "bin\openocd.exe"))) {
                throw "Downloaded OpenOCD package has an unexpected directory layout."
            }
            Move-Item -LiteralPath $Expanded -Destination (Join-Path $ToolsRoot "openocd")
        } finally {
            if (Test-Path -LiteralPath $InstallRoot) {
                Remove-Item -Recurse -Force -LiteralPath $InstallRoot
            }
        }
    }
    $OpenOcd = $LocalOpenOcd
}

if ([string]::IsNullOrWhiteSpace($OpenOcd) -or
    -not (Test-Path -LiteralPath $OpenOcd)) {
    throw "openocd.exe was not found. Pass a PY32F002A-capable build with -OpenOcd."
}

$OpenOcdArguments = @()
if ([string]::IsNullOrWhiteSpace($OpenOcdScripts)) {
    # xPack uses bin/ for the executable and openocd/scripts/ for data files.
    # Traditional Windows archives commonly use bin/../scripts/ instead.
    $OpenOcdBin = Split-Path -Parent $OpenOcd
    $OpenOcdRoot = Split-Path -Parent $OpenOcdBin
    $ScriptCandidates = @(
        (Join-Path $OpenOcdRoot "openocd\scripts"),
        (Join-Path $OpenOcdRoot "scripts"),
        (Join-Path $OpenOcdBin "scripts")
    )
    $OpenOcdScripts = $ScriptCandidates |
        Where-Object {
            Test-Path -LiteralPath (Join-Path $_ "interface\cmsis-dap.cfg")
        } |
        Select-Object -First 1
}

if (-not [string]::IsNullOrWhiteSpace($OpenOcdScripts)) {
    if (-not (Test-Path -LiteralPath $OpenOcdScripts)) {
        throw "OpenOCD scripts directory not found: $OpenOcdScripts"
    }
    $OpenOcdArguments += @("-s", $OpenOcdScripts)
} else {
    throw "OpenOCD scripts directory was not found. Pass it with -OpenOcdScripts."
}
$OpenOcdArguments += @(
    "-f", $Interface,
    "-c", "set CHIPNAME py32f002a; set FLASH_SIZE 0x8000; set WORKAREASIZE 0x1000",
    "-f", $TargetConfig
)

if ($Action -eq "Flash") {
    Write-Host "[2/2] Programming and resetting PY32F002A through OpenOCD..."
    # Tcl treats Windows backslashes as escapes (for example, \b is backspace).
    # Forward slashes and braces preserve both drive letters and paths with spaces.
    $FirmwareOpenOcd = $Firmware.Replace('\', '/')
    $OpenOcdArguments += @(
        "-c", "program {$FirmwareOpenOcd} verify reset exit"
    )
} elseif ($Action -eq "Reset") {
    Write-Host "Resetting PY32F002A through OpenOCD..."
    $OpenOcdArguments += @("-c", "init; reset run; shutdown")
} else {
    Write-Host "Starting the PY32F002A debug server on localhost:3333..."
    Write-Host "Connect GDB with: target extended-remote localhost:3333"
    Write-Host "Press Ctrl+C to stop the debug server."
    $OpenOcdArguments += @("-c", "init; reset halt")
}

& $OpenOcd @OpenOcdArguments
if ($LASTEXITCODE -ne 0) {
    throw "OpenOCD failed (exit code $LASTEXITCODE)."
}

if ($Action -ne "Debug") {
    Write-Host "Done: action=$Action example=$Example device=$Device"
}
