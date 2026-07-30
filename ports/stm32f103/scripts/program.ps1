[CmdletBinding(PositionalBinding = $false)]
param(
    [ValidatePattern('^[A-Za-z0-9_-]+$')]
    [string]$Example = "blink",

    [string]$Programmer,

    [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
    [string[]]$MakeArgs
)

$ErrorActionPreference = "Stop"

$PortRoot = Split-Path -Parent $PSScriptRoot
$RepoRoot = Split-Path -Parent (Split-Path -Parent $PortRoot)
$ExampleSource = Join-Path $RepoRoot "examples\$Example\main.c"

if (-not (Test-Path -LiteralPath $ExampleSource)) {
    throw "PIC example not found: $ExampleSource"
}

$Wsl = Get-Command "wsl.exe" -ErrorAction SilentlyContinue
if ($null -eq $Wsl) {
    throw "wsl.exe was not found. The STM32 build currently uses the ARM toolchain installed in WSL."
}

Write-Host "[1/2] Building STM32 firmware in WSL..."
$BuildArguments = @("EXAMPLE=$Example")
foreach ($Argument in $MakeArgs) {
    if ($Argument -notmatch '^[A-Za-z_][A-Za-z0-9_]*=.*$') {
        throw "Invalid Make argument '$Argument'. Expected NAME=value."
    }
    $BuildArguments += $Argument
}
$BuildCommand = "make stm32 " + ($BuildArguments -join " ")
& $Wsl.Source --cd $RepoRoot bash -ic $BuildCommand
if ($LASTEXITCODE -ne 0) {
    throw "WSL build failed (exit code $LASTEXITCODE)."
}

$Firmware = Join-Path $PortRoot `
    "build\$Example\picemu-stm32f103.hex"
if (-not (Test-Path -LiteralPath $Firmware)) {
    throw "STM32 firmware was not generated: $Firmware"
}

if ([string]::IsNullOrWhiteSpace($Programmer)) {
    $Command = Get-Command "STM32_Programmer_CLI.exe" `
        -ErrorAction SilentlyContinue
    if ($null -ne $Command) {
        $Programmer = $Command.Source
    }
}

if ([string]::IsNullOrWhiteSpace($Programmer)) {
    $Candidates = @(
        "E:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
        "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe",
        "C:\Program Files (x86)\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
    )
    $Programmer = $Candidates |
        Where-Object { Test-Path -LiteralPath $_ } |
        Select-Object -First 1
}

if ([string]::IsNullOrWhiteSpace($Programmer) -or
    -not (Test-Path -LiteralPath $Programmer)) {
    throw "STM32_Programmer_CLI.exe was not found. Pass its path with -Programmer."
}

Write-Host "[2/2] Programming and resetting STM32 through ST-Link..."
& $Programmer -c "port=SWD" -w $Firmware -v -rst
if ($LASTEXITCODE -ne 0) {
    throw "STM32 programming failed (exit code $LASTEXITCODE)."
}

Write-Host "Done: example=$Example"
