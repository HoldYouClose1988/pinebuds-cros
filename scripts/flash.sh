#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "$ROOT/.tools/env.sh"

TARGET="${TARGET:-open_source}"
BIN="${BIN:-$OPENPINEBUDS_ROOT/out/$TARGET/$TARGET.bin}"
PORT0="${PORT0:-/dev/ttyACM0}"
PORT1="${PORT1:-/dev/ttyACM1}"

if [[ ! -f "$BIN" ]]; then
  echo "Missing firmware image: $BIN (run ./scripts/build.sh first)" >&2
  exit 1
fi

if [[ ! -x "$(command -v bestool)" ]]; then
  echo "bestool not on PATH (run ./scripts/bootstrap-sdk.sh)" >&2
  exit 1
fi

echo "Flashing $BIN"
echo "Seat both buds, wake them (remove 3s / reseat or long-hold rear button), then continue."
read -r -p "Press Enter to flash $PORT0 and $PORT1..." _

bestool write-image "$BIN" --port "$PORT0"
bestool write-image "$BIN" --port "$PORT1"
echo "Done."
