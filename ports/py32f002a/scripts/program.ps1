[CmdletBinding(PositionalBinding = $false)]
param(
    [ValidatePattern('^[A-Za-z0-9_-]+$')]
    [string]$Example = "blink",

    [ValidateSet("pic10f200", "pic10f202")]
    [string]$Device = "pic10f200",

    [ValidateSet("Flash", "Reset")]
    [string]$Action = "Flash",

    [string]$OpenOcd,

    [string]$OpenOcdScripts,

    [string]$Interface = "interface/cmsis-dap.cfg",

    [switch]$NoBuild,

    [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
    [string[]]$MakeArgs
)

$ErrorActionPreference = "Stop"

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

if (-not $NoBuild -and $Action -eq "Flash") {
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

if ($Action -eq "Flash" -and -not (Test-Path -LiteralPath $Firmware)) {
    throw "Firmware was not generated: $Firmware"
}

if ([string]::IsNullOrWhiteSpace($OpenOcd)) {
    $LocalOpenOcd = Join-Path $PortRoot "tools\openocd\bin\openocd.exe"
    if (Test-Path -LiteralPath $LocalOpenOcd) {
        $OpenOcd = $LocalOpenOcd
    } else {
        $Command = Get-Command "openocd.exe" -ErrorAction SilentlyContinue
        if ($null -ne $Command) {
            $OpenOcd = $Command.Source
        }
    }
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
} else {
    Write-Host "Resetting PY32F002A through OpenOCD..."
    $OpenOcdArguments += @("-c", "init; reset run; shutdown")
}

& $OpenOcd @OpenOcdArguments
if ($LASTEXITCODE -ne 0) {
    throw "OpenOCD failed (exit code $LASTEXITCODE)."
}

Write-Host "Done: action=$Action example=$Example device=$Device"
