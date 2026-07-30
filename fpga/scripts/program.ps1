param(
    [string]$Firmware,
    [string]$Example,
    [switch]$SkipBuild,
    [switch]$Volatile,
    [string]$OssCadSuite
)

$ErrorActionPreference = "Stop"

$FpgaRoot = Split-Path -Parent $PSScriptRoot
$RepoRoot = Split-Path -Parent $FpgaRoot
. (Join-Path $PSScriptRoot "config.ps1")

if ([string]::IsNullOrWhiteSpace($Firmware)) {
    $Firmware = $FpgaConfig.DefaultFirmware
}
if ([string]::IsNullOrWhiteSpace($OssCadSuite)) {
    $OssCadSuite = $env:OSS_CAD_SUITE
}
if ([string]::IsNullOrWhiteSpace($OssCadSuite)) {
    $OssCadSuite = $FpgaConfig.DefaultOssCadSuite
}

$Bin = Join-Path $OssCadSuite "bin"
$Lib = Join-Path $OssCadSuite "lib"
$PyBin = Join-Path $OssCadSuite "py3bin"
$env:PATH = "$Bin;$Lib;$PyBin;$env:PATH"

$Loader = Join-Path $OssCadSuite "bin\openFPGALoader.exe"
$BuildDir = Join-Path $FpgaRoot $FpgaConfig.BuildRelativePath
$Bitstream = Join-Path $BuildDir $FpgaConfig.BitstreamName

if (-not (Test-Path -LiteralPath $Loader)) {
    throw "openFPGALoader not found: $Loader"
}

# With -Example, one command compiles main.c in WSL, builds the FPGA
# bitstream, and programs the board. Keep -Firmware for custom HEX files.
if (-not [string]::IsNullOrWhiteSpace($Example)) {
    if ($PSBoundParameters.ContainsKey("Firmware")) {
        throw "-Example and -Firmware cannot be used together."
    }
    if ($SkipBuild) {
        throw "-SkipBuild cannot be used with -Example because the new firmware must be embedded into a new bitstream."
    }
    if ($Example -notmatch '^[A-Za-z0-9_-]+$') {
        throw "Invalid example name: $Example"
    }

    $ExampleDir = Join-Path $RepoRoot "examples\$Example"
    $ExampleSource = Join-Path $ExampleDir "main.c"
    if (-not (Test-Path -LiteralPath $ExampleSource)) {
        $AvailableExamples = (
            Get-ChildItem -LiteralPath (Join-Path $RepoRoot "examples") -Directory |
                Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName "main.c") } |
                Select-Object -ExpandProperty Name
        ) -join ", "
        throw "Example not found: $Example`nAvailable examples: $AvailableExamples"
    }

    $Wsl = Get-Command "wsl.exe" -ErrorAction SilentlyContinue
    if ($null -eq $Wsl) {
        throw "wsl.exe was not found. Install/enable WSL or compile the HEX manually and use -Firmware."
    }

    Write-Host "Compiling PIC example '$Example' in WSL with XC8..."
    # wsl.exe --cd accepts a Windows path directly. This avoids the quoting
    # differences of wslpath across PowerShell/WSL versions.
    $WslBuildCommand = "make -C 'examples/$Example' firmware"
    & $Wsl.Source --cd $RepoRoot bash -ic $WslBuildCommand
    if ($LASTEXITCODE -ne 0) {
        throw "PIC firmware build failed in WSL (exit code $LASTEXITCODE)."
    }

    $Firmware = Join-Path $ExampleDir "build\firmware.hex"
    if (-not (Test-Path -LiteralPath $Firmware)) {
        throw "XC8 completed but firmware was not generated: $Firmware"
    }
}

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot "build.ps1") `
        -Firmware $Firmware `
        -OssCadSuite $OssCadSuite
    if ($LASTEXITCODE -ne 0) {
        throw "FPGA build failed (exit code $LASTEXITCODE)."
    }
}
if (-not (Test-Path -LiteralPath $Bitstream)) {
    throw "Bitstream not found: $Bitstream"
}

if ($Volatile) {
    Write-Host "Programming Tang Nano 1K SRAM (volatile test mode)..."
    & $Loader -b $FpgaConfig.OpenFpgaLoaderBoard $Bitstream
}
else {
    Write-Host "Programming Tang Nano 1K internal configuration Flash..."
    & $Loader -b $FpgaConfig.OpenFpgaLoaderBoard -f $Bitstream
    if ($LASTEXITCODE -ne 0) {
        throw "Persistent Flash programming failed (exit code $LASTEXITCODE)."
    }

    <#
    写配置 Flash 时下载器会擦除当前 SRAM 配置，但并非所有版本的
    openFPGALoader 都会在写完后自动触发重新配置。再加载一次 SRAM，
    让刚烧录的程序立即运行；Flash 中的持久化副本保持不变，下次上电
    仍会自动加载。
    #>
    Write-Host "Loading the same bitstream into FPGA SRAM for immediate start..."
    & $Loader -b $FpgaConfig.OpenFpgaLoaderBoard $Bitstream
}
if ($LASTEXITCODE -ne 0) {
    throw "Programming failed (exit code $LASTEXITCODE)."
}

Write-Host "Done. The bitstream contains both the PIC core and selected firmware."
if ($Volatile) {
    Write-Host "The image is volatile and will be lost after power-off."
}
else {
    Write-Host "The image is running now and is also persistent across power cycles."
}
Write-Host "PIC GP0/GP1/GP2 drive RGB LEDs; BTN1 drives GP3; BTN2 resets."
