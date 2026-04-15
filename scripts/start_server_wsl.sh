#!/usr/bin/env bash
set -euo pipefail

PORT="${1:-8080}"
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "$ROOT_DIR"
make
exec ./bin/chat_server "$PORT"
