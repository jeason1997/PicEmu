param(
    [string]$OssCadSuite = $env:OSS_CAD_SUITE,
    [string]$Firmware = "examples\blink\build\firmware.hex",
    [switch]$NoWave
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
$ProgramMemoryFile = Join-Path $FpgaRoot "rtl\memory\pic10f200_program_memory.v"
$CoreTestbench = Join-Path $FpgaRoot "tb\pic10f200_core_tb.v"
$MemoryTestbench = Join-Path $FpgaRoot "tb\pic10f200_program_memory_tb.v"
$FirmwareTestbench = Join-Path $FpgaRoot "tb\pic10f200_firmware_tb.v"
$GowinRamModel = Join-Path $FpgaRoot "tb\gowin_ram16sdp4_model.v"
$CoreSimulation = Join-Path $BuildDir "pic10f200_core_tb.vvp"
$MemorySimulation = Join-Path $BuildDir "pic10f200_program_memory_tb.vvp"
$FirmwareSimulation = Join-Path $BuildDir "pic10f200_firmware_tb.vvp"
$FirmwareMemory = Join-Path $BuildDir "firmware_sim.mem"
$FirmwareVcd = Join-Path $BuildDir "firmware_sim.vcd"
$Converter = Join-Path $FpgaRoot "tools\hex_to_mem.py"
$RepoRoot = Split-Path -Parent $FpgaRoot
$Python = Join-Path $PyBin "python3.exe"
if (-not (Test-Path -LiteralPath $Python)) {
    $Python = Join-Path $Lib "python3.exe"
}

if (-not [System.IO.Path]::IsPathRooted($Firmware)) {
    $Firmware = Join-Path $RepoRoot $Firmware
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

& $Iverilog -g2012 -s pic10f200_core_tb -o $CoreSimulation `
    $GowinRamModel $CoreFile $CoreTestbench
if ($LASTEXITCODE -ne 0) { throw "Icarus Verilog compile failed (exit code $LASTEXITCODE)." }

& $Vvp $CoreSimulation
if ($LASTEXITCODE -ne 0) { throw "Simulation failed (exit code $LASTEXITCODE)." }

& $Iverilog -g2012 -s pic10f200_program_memory_tb `
    -o $MemorySimulation $ProgramMemoryFile $MemoryTestbench
if ($LASTEXITCODE -ne 0) { throw "Program-memory test compile failed (exit code $LASTEXITCODE)." }

& $Vvp $MemorySimulation
if ($LASTEXITCODE -ne 0) { throw "Program-memory test failed (exit code $LASTEXITCODE)." }

if (-not (Test-Path -LiteralPath $Firmware)) {
    throw "PIC firmware not found: $Firmware`nBuild it with XC8 before running RTL firmware simulation."
}
if (-not (Test-Path -LiteralPath $Python)) {
    throw "Python from OSS-CAD-Suite was not found: $Python"
}

Write-Host "Converting real PIC firmware for RTL simulation: $Firmware"
& $Python $Converter $Firmware $FirmwareMemory
if ($LASTEXITCODE -ne 0) { throw "HEX conversion failed (exit code $LASTEXITCODE)." }

& $Iverilog -g2012 -s pic10f200_firmware_tb `
    -o $FirmwareSimulation $GowinRamModel $CoreFile $ProgramMemoryFile $FirmwareTestbench
if ($LASTEXITCODE -ne 0) { throw "Firmware test compile failed (exit code $LASTEXITCODE)." }

Push-Location $BuildDir
try {
    if ($NoWave) {
        & $Vvp $FirmwareSimulation +NO_WAVE
    }
    else {
        & $Vvp $FirmwareSimulation
    }
    if ($LASTEXITCODE -ne 0) { throw "Firmware simulation failed (exit code $LASTEXITCODE)." }
}
finally {
    Pop-Location
}

if ($NoWave) {
    Remove-Item -LiteralPath $FirmwareVcd -ErrorAction SilentlyContinue
}
else {
    Write-Host "GTKWave waveform: $FirmwareVcd"
}
