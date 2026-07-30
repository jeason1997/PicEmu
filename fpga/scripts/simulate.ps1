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

$Iverilog = Join-Path $OssCadSuite "bin\iverilog.exe"
$Vvp = Join-Path $OssCadSuite "bin\vvp.exe"

foreach ($Tool in @($Iverilog, $Vvp)) {
    if (-not (Test-Path -LiteralPath $Tool)) {
        throw "Simulation tool not found: $Tool"
    }
}

$FpgaRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $FpgaRoot "build\pic10f200"
$CoreFile = Join-Path $FpgaRoot "rtl\core\pic10f200_core.v"
$Testbench = Join-Path $FpgaRoot "tb\pic10f200_core_tb.v"
$Simulation = Join-Path $BuildDir "pic10f200_core_tb.vvp"

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

& $Iverilog -g2012 -s pic10f200_core_tb -o $Simulation $CoreFile $Testbench
if ($LASTEXITCODE -ne 0) { throw "Icarus Verilog compile failed (exit code $LASTEXITCODE)." }

& $Vvp $Simulation
if ($LASTEXITCODE -ne 0) { throw "Simulation failed (exit code $LASTEXITCODE)." }
