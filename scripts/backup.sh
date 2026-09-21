#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "$ROOT/.tools/env.sh"

OUT="${OUT:-$ROOT/backups}"
mkdir -p "$OUT"
STAMP="$(date -u +%Y%m%dT%H%M%SZ)"
PORT0="${PORT0:-/dev/ttyACM0}"
PORT1="${PORT1:-/dev/ttyACM1}"

echo "Backing up buds to $OUT ($STAMP)"
echo "Sync order per bud: OUT (LED on) -> start bestool below -> IMMEDIATELY reseat (reset during Sync)."
echo "If hung on 'Sent message type Sync', Ctrl+C and retry with reseat-after-start."
echo "Press Enter when LEFT bud is OUT and ready ($PORT0)..."
read -r _
bestool read-image "$OUT/left-$STAMP.bin" --port "$PORT0" || bestool --help | head -20
echo "Press Enter when RIGHT bud is OUT and ready ($PORT1)..."
read -r _
bestool read-image "$OUT/right-$STAMP.bin" --port "$PORT1" || true
echo "If read-image is unsupported by this bestool build, use OpenPineBuds ./backup.sh inside vendor/OpenPineBuds."
