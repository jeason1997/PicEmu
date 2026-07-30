param(
    [string]$Firmware = "examples\blink\build\firmware.hex",
    [switch]$SkipBuild,
    [string]$OssCadSuite = $env:OSS_CAD_SUITE
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($OssCadSuite)) {
    $OssCadSuite = "E:\oss-cad-suite"
}

$Bin = Join-Path $OssCadSuite "bin"
$Lib = Join-Path $OssCadSuite "lib"
$PyBin = Join-Path $OssCadSuite "py3bin"
$env:PATH = "$Bin;$Lib;$PyBin;$env:PATH"

$FpgaRoot = Split-Path -Parent $PSScriptRoot
$Loader = Join-Path $OssCadSuite "bin\openFPGALoader.exe"
$Bitstream = Join-Path $FpgaRoot "build\pic10f200\pic10f200.fs"

if (-not (Test-Path -LiteralPath $Loader)) {
    throw "openFPGALoader not found: $Loader"
}
if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot "build.ps1") -Firmware $Firmware -OssCadSuite $OssCadSuite
    if ($LASTEXITCODE -ne 0) {
        throw "FPGA build failed (exit code $LASTEXITCODE)."
    }
}
if (-not (Test-Path -LiteralPath $Bitstream)) {
    throw "Bitstream not found: $Bitstream"
}

Write-Host "Programming Tang Nano 1K SRAM (volatile)..."
& $Loader -b tangnano1k $Bitstream
if ($LASTEXITCODE -ne 0) { throw "Programming failed (exit code $LASTEXITCODE)." }

Write-Host "Done. PIC GP0/GP1/GP2 drive RGB LEDs; BTN1 drives GP3; BTN2 resets."
