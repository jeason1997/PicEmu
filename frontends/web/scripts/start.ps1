param(
    [string]$Example = "button",
    [int]$Port = 4173
)

$ErrorActionPreference = "Stop"
$WebRoot = Split-Path -Parent $PSScriptRoot
$RepoRoot = Split-Path -Parent (Split-Path -Parent $WebRoot)

# 所有构建和启动逻辑都在 start.sh；这里仅负责把 Windows 参数交给 WSL。
& wsl.exe --cd $RepoRoot bash -ic 'exec "$@"' bash `
    ./frontends/web/scripts/start.sh --example $Example --port "$Port"
if ($LASTEXITCODE -ne 0) {
    throw "Web server exited with code $LASTEXITCODE."
}
