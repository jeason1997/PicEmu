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

# OSS-CAD-Suite executables load DLLs and Python support files from these
# directories. Updating PATH in this process is equivalent to environment.bat.
$env:PATH = "$Bin;$Lib;$PyBin;$env:PATH"

$Yosys = Join-Path $Bin "yosys.exe"
$NextPnr = Join-Path $Bin "nextpnr-gowin.exe"
$GowinPack = Join-Path $Bin "gowin_pack.exe"

foreach ($Tool in @($Yosys, $NextPnr, $GowinPack)) {
    if (-not (Test-Path -LiteralPath $Tool)) {
        throw "OSS-CAD-Suite tool not found: $Tool`nSet OSS_CAD_SUITE or pass -OssCadSuite."
    }
}

$FpgaRoot = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $FpgaRoot "build\button_toggle"
$TopFile = Join-Path $FpgaRoot "examples\button_toggle\rtl\top.v"
$ButtonFile = Join-Path $FpgaRoot "rtl\common\button_conditioner.v"
$ConstraintFile = Join-Path $FpgaRoot "boards\tang_nano_1k\tang_nano_1k.cst"
$SynthJson = Join-Path $BuildDir "button_toggle.json"
$PnrJson = Join-Path $BuildDir "button_toggle_pnr.json"
$Bitstream = Join-Path $BuildDir "button_toggle.fs"

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

Push-Location $BuildDir
try {
    Write-Host "[1/3] Synthesizing with Yosys..."
    & $Yosys -p "read_verilog `"$ButtonFile`" `"$TopFile`"; synth_gowin -top top -json `"$SynthJson`""
    if ($LASTEXITCODE -ne 0) { throw "Yosys failed (exit code $LASTEXITCODE)." }

    Write-Host "[2/3] Placing and routing with nextpnr-gowin..."
    & $NextPnr `
        --json $SynthJson `
        --write $PnrJson `
        --freq 27 `
        --device "GW1NZ-LV1QN48C6/I5" `
        --family "GW1NZ-1" `
        --cst $ConstraintFile
    if ($LASTEXITCODE -ne 0) { throw "nextpnr-gowin failed (exit code $LASTEXITCODE)." }

    Write-Host "[3/3] Generating bitstream with Apicula..."
    & $GowinPack -d "GW1NZ-1" -o $Bitstream $PnrJson
    if ($LASTEXITCODE -ne 0) { throw "gowin_pack failed (exit code $LASTEXITCODE)." }
}
finally {
    Pop-Location
}

Write-Host "Build succeeded: $Bitstream"
