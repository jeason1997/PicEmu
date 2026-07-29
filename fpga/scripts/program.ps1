param(
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
$Bitstream = Join-Path $FpgaRoot "build\button_toggle\button_toggle.fs"

if (-not (Test-Path -LiteralPath $Loader)) {
    throw "openFPGALoader not found: $Loader"
}
if (-not (Test-Path -LiteralPath $Bitstream)) {
    throw "Bitstream not found: $Bitstream`nRun fpga\scripts\build.ps1 first."
}

Write-Host "Programming Tang Nano 1K SRAM (volatile)..."
& $Loader -b tangnano1k $Bitstream
if ($LASTEXITCODE -ne 0) { throw "Programming failed (exit code $LASTEXITCODE)." }

Write-Host "Done. BTN1 toggles LED1; BTN2 resets LED1 to off."
