param(
    [string]$Firmware,
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

$Yosys = Join-Path $Bin "yosys.exe"
$NextPnrHimbaechel = Join-Path $Bin "nextpnr-himbaechel.exe"
$GowinPack = Join-Path $Bin "gowin_pack.exe"
$Python = Join-Path $PyBin "python3.exe"
if (-not (Test-Path -LiteralPath $Python)) {
    $Python = Join-Path $Lib "python3.exe"
}

foreach ($Tool in @($Yosys, $GowinPack, $Python)) {
    if (-not (Test-Path -LiteralPath $Tool)) {
        throw "OSS-CAD-Suite tool not found: $Tool`nSet OSS_CAD_SUITE or pass -OssCadSuite."
    }
}
if (-not (Test-Path -LiteralPath $NextPnrHimbaechel)) {
    throw "nextpnr-himbaechel was not found in $Bin.`nUpgrade OSS-CAD-Suite to 2026-07-29 or newer; the older nextpnr-gowin database cannot place GW1NZ-1 BSRAM."
}
$NextPnr = $NextPnrHimbaechel

$BuildDir = Join-Path $FpgaRoot $FpgaConfig.BuildRelativePath
$TopFile = Join-Path $FpgaRoot "boards\tang_nano_1k\pic10f200_top.v"
$ClockEnableFile = Join-Path $FpgaRoot "rtl\common\clock_enable.v"
$SynchronizerFile = Join-Path $FpgaRoot "rtl\common\input_synchronizer.v"
$CoreFile = Join-Path $FpgaRoot "rtl\core\pic10f200_core.v"
$ProgramMemoryFile = Join-Path $FpgaRoot "rtl\memory\pic10f200_program_memory.v"
$ConstraintFile = Join-Path $FpgaRoot "boards\tang_nano_1k\tang_nano_1k.cst"
$Converter = Join-Path $FpgaRoot "tools\hex_to_mem.py"
$SynthJson = Join-Path $BuildDir "pic10f200.json"
$PnrJson = Join-Path $BuildDir "pic10f200_pnr.json"
$Bitstream = Join-Path $BuildDir $FpgaConfig.BitstreamName
$MemoryFile = Join-Path $BuildDir "firmware.mem"

if (-not [System.IO.Path]::IsPathRooted($Firmware)) {
    $Firmware = Join-Path $RepoRoot $Firmware
}
if (-not (Test-Path -LiteralPath $Firmware)) {
    throw "PIC firmware not found: $Firmware"
}

$FirmwareBuildDir = Split-Path -Parent $Firmware
$FirmwareProjectDir = Split-Path -Parent $FirmwareBuildDir
$FirmwareSource = Join-Path $FirmwareProjectDir "main.c"
if ((Test-Path -LiteralPath $FirmwareSource) -and
    (Get-Item -LiteralPath $FirmwareSource).LastWriteTime -gt
    (Get-Item -LiteralPath $Firmware).LastWriteTime) {
    throw "PIC firmware is stale: $Firmware`nSource is newer: $FirmwareSource`nRebuild the XC8 firmware before building the FPGA bitstream."
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

Push-Location $BuildDir
try {
    Write-Host "[1/4] Converting PIC Intel HEX to 12-bit program-memory image..."
    & $Python $Converter $Firmware $MemoryFile
    if ($LASTEXITCODE -ne 0) {
        throw "HEX conversion failed (exit code $LASTEXITCODE)."
    }

    Write-Host "[2/4] Synthesizing PIC10F200 and block program RAM..."
    $VerilogSources = @(
        $ClockEnableFile,
        $SynchronizerFile,
        $ProgramMemoryFile,
        $CoreFile,
        $TopFile
    )
    $YosysSources = @()
    foreach ($Src in $VerilogSources) {
        if (-not (Test-Path -LiteralPath $Src)) {
            throw "Verilog source not found: $Src"
        }
        $YosysSources += $Src -replace '\\', '/'
    }
    $YosysSynthJson = $SynthJson -replace '\\', '/'
    $YosysSourcesJoined = $YosysSources -join ' '
    $YosysCommand = "read_verilog $YosysSourcesJoined; synth_gowin -nowidelut -top top -json $YosysSynthJson"
    & $Yosys -p $YosysCommand
    if ($LASTEXITCODE -ne 0) {
        throw "Yosys failed (exit code $LASTEXITCODE)."
    }

    $FirmwareHasInstruction = Get-Content -LiteralPath $MemoryFile |
        Where-Object { $_ -notmatch '^\s*0+\s*$' } |
        Select-Object -First 1
    if ($null -ne $FirmwareHasInstruction) {
        $SynthesizedNetlist = Get-Content -LiteralPath $SynthJson -Raw
        if ($SynthesizedNetlist -notmatch 'INIT_RAM_[0-9A-F]+"\s*:\s*"(?=[01]*1)') {
            throw "Synthesized program BSRAM is all zero although firmware contains instructions. Refusing to generate or program a stale/empty bitstream."
        }
        Write-Host "Verified: synthesized BSRAM contains non-zero firmware initialization."
    }

    Write-Host "[3/4] Placing and routing with nextpnr-himbaechel..."
    & $NextPnr `
        --json $SynthJson `
        --write $PnrJson `
        --freq $FpgaConfig.TargetFrequencyMHz `
        --device $FpgaConfig.NextPnrDevice `
        --vopt "cst=$ConstraintFile"
    if ($LASTEXITCODE -ne 0) {
        throw "nextpnr-himbaechel failed (exit code $LASTEXITCODE)."
    }

    Write-Host "[4/4] Generating bitstream with Apicula..."
    & $GowinPack -d $FpgaConfig.GowinPackDevice -o $Bitstream $PnrJson
    if ($LASTEXITCODE -ne 0) {
        throw "gowin_pack failed (exit code $LASTEXITCODE)."
    }
}
finally {
    Pop-Location
}

Write-Host "Firmware embedded in bitstream: $Firmware"
Write-Host "Program memory initialized during FPGA configuration."
Write-Host "Build succeeded: $Bitstream"
