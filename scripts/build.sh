#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "$ROOT/.tools/env.sh"

cd "$OPENPINEBUDS_ROOT"
JOBS="${JOBS:-$(nproc)}"
TARGET="${TARGET:-open_source}"
STAGE_A="${STAGE_A:-1}"
STAGE_B="${STAGE_B:-1}"

EXTRA=()
if [[ "$STAGE_B" == "1" ]]; then
  EXTRA+=(CROS_STAGE_B=1 CROS_STAGE_A=1)
  echo "==> Experimental cross-bud CROS enabled (CROS_STAGE_B=1, poor=RIGHT)"
elif [[ "$STAGE_A" == "1" ]]; then
  EXTRA+=(CROS_STAGE_A=1)
  echo "==> Experimental FF mic loopback enabled (CROS_STAGE_A=1)"
fi

echo "==> Building T=$TARGET (jobs=$JOBS) ${EXTRA[*]:-}"
if make -j"$JOBS" "T=$TARGET" DEBUG=1 "${EXTRA[@]}" >"$ROOT/build.log" 2>&1; then
  echo "build success"
  ls -lh "out/$TARGET/"*.bin 2>/dev/null || ls -lh out/"$TARGET"/
else
  echo "build failed — see build.log" >&2
  grep -E "error:|Error" "$ROOT/build.log" | tail -40 >&2 || true
  exit 1
fi
