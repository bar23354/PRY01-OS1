#!/usr/bin/env bash
set -euo pipefail

PORT="${1:-5050}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SERVER_LOG="${TMPDIR:-/tmp}/pry01_server_${PORT}.log"
CLIENT_LOG="${TMPDIR:-/tmp}/pry01_client_${PORT}.log"

cleanup() {
    if [[ -n "${SERVER_PID:-}" ]]; then
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
}

trap cleanup EXIT

cd "$ROOT_DIR"
make

CHAT_ALLOW_SAME_IP=1 stdbuf -oL ./bin/chat_server "$PORT" >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 1

printf "/list\n/quit\n" | ./bin/chat_client prueba 127.0.0.1 "$PORT" >"$CLIENT_LOG" 2>&1

echo "CLIENT_LOG=$CLIENT_LOG"
cat "$CLIENT_LOG"
echo "SERVER_LOG=$SERVER_LOG"
cat "$SERVER_LOG"
