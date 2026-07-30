#!/bin/sh
set -eu

# Linux 是 Web 仿真器唯一的启动实现。Windows 的 PowerShell 脚本只把参数
# 转交给本脚本，避免两边分别维护构建步骤和默认值。
EXAMPLE=button
PORT=4173

while [ "$#" -gt 0 ]; do
    case "$1" in
        --example)
            EXAMPLE=${2:?--example requires a value}
            shift 2
            ;;
        --port)
            PORT=${2:?--port requires a value}
            shift 2
            ;;
        *)
            echo "Unknown argument: $1" >&2
            exit 2
            ;;
    esac
done

case "$EXAMPLE" in
    *[!A-Za-z0-9_-]*|'')
        echo "Invalid example: $EXAMPLE" >&2
        exit 2
        ;;
esac
case "$PORT" in
    *[!0-9]*|'')
        echo "Invalid port: $PORT" >&2
        exit 2
        ;;
esac

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/../../.." && pwd)
cd "$REPO_ROOT"

echo "Building C web backend and all reusable PIC examples..."
make web-core firmware

if command -v node >/dev/null 2>&1; then
    exec node frontends/web/server.mjs --port "$PORT" --example "$EXAMPLE"
fi

# WSL 中未安装 Linux Node.js 时，直接使用 Windows 已安装的 node.exe。
if command -v node.exe >/dev/null 2>&1; then
    exec node.exe frontends/web/server.mjs --port "$PORT" --example "$EXAMPLE"
fi

echo "Node.js was not found. Install node or make node.exe available in PATH." >&2
exit 127
