param(
    [string]$Example = "button",
    [int]$Port = 4173
)

$ErrorActionPreference = "Stop"
$WebRoot = Split-Path -Parent $PSScriptRoot
$RepoRoot = Split-Path -Parent (Split-Path -Parent $WebRoot)

# start.sh is the single source of build and launch behavior.
# Interactive bash loads the XC8 PATH configured in .bashrc.
& wsl.exe "--cd" "$RepoRoot" "bash" "-i" `
    "./frontends/web/scripts/start.sh" "--example" "$Example" `
    "--port" "$Port"
$ExitCode = $LASTEXITCODE

# Ctrl+C / terminal shutdown through WSL may be reported as -1, 130 or 255.
if ($ExitCode -notin @(0, -1, 130, 255)) {
    throw "Web server exited with code $ExitCode."
}
