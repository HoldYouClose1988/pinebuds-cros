#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "$ROOT/.tools/env.sh"

# Keep vendored SDK apps in sync with firmware/ (bootstrap does this once;
# rebuilds must re-copy or stale .o / old strings ship).
if [[ -d "$ROOT/firmware/stage_a" ]]; then
  mkdir -p "$OPENPINEBUDS_ROOT/apps/cros_loopback"
  cp -f "$ROOT/firmware/stage_a/cros_loopback.c" \
        "$ROOT/firmware/stage_a/cros_loopback.h" \
        "$ROOT/firmware/stage_a/Makefile" \
        "$OPENPINEBUDS_ROOT/apps/cros_loopback/"
fi
if [[ -d "$ROOT/firmware/stage_b" ]]; then
  mkdir -p "$OPENPINEBUDS_ROOT/apps/cros_tws"
  cp -f "$ROOT/firmware/stage_b/"*.c \
        "$ROOT/firmware/stage_b/"*.h \
        "$ROOT/firmware/stage_b/Makefile" \
        "$OPENPINEBUDS_ROOT/apps/cros_tws/"
  if compgen -G "$ROOT/firmware/stage_b/"*.cpp >/dev/null; then
    cp -f "$ROOT/firmware/stage_b/"*.cpp "$OPENPINEBUDS_ROOT/apps/cros_tws/"
  fi
fi

cd "$OPENPINEBUDS_ROOT"
JOBS="${JOBS:-$(nproc)}"
TARGET="${TARGET:-open_source}"
STAGE_A="${STAGE_A:-1}"
STAGE_B="${STAGE_B:-1}"
TOTA="${TOTA:-0}"

EXTRA=()
if [[ "$STAGE_B" == "1" ]]; then
  EXTRA+=(CROS_STAGE_B=1 CROS_STAGE_A=1)
  echo "==> Experimental cross-bud CROS enabled (CROS_STAGE_B=1, poor=RIGHT)"
elif [[ "$STAGE_A" == "1" ]]; then
  EXTRA+=(CROS_STAGE_A=1)
  echo "==> Experimental FF mic loopback enabled (CROS_STAGE_A=1)"
fi

if [[ "$TOTA" == "1" ]]; then
  # TEST_OVER_THE_AIR must be 1 so libtota + -Iservices/tota are linked;
  # TOTA=1 alone only adds -DTEST_OVER_THE_AIR_ENANBLED (include path stays off).
  EXTRA+=(TOTA=1 TEST_OVER_THE_AIR=1)
  echo "==> TOTA SPP log sink enabled (TOTA=1) — phone can capture [cros_*] over RFCOMM 12"
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
