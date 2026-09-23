#!/usr/bin/env bash
# Build a Windows-ready flash ZIP and stage it under flash-packages/ for GitHub download.
#
# Versioning: reads VERSION at repo root (semver). Bump VERSION + CHANGELOG.md
# before packaging a new iteration, or set BUMP=patch|minor|major.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# shellcheck disable=SC1091
source "$ROOT/.tools/env.sh"

TARGET="${TARGET:-open_source}"
STAGE="${STAGE:-stage-b}"
STAGE_A="${STAGE_A:-1}"
# Logging flash packages enable TOTA SPP by default (phone log sink).
TOTA="${TOTA:-1}"
BIN_SRC="${BIN_SRC:-$OPENPINEBUDS_ROOT/out/$TARGET/$TARGET.bin}"
OUT_DIR="${OUT_DIR:-$ROOT/flash-packages}"
DO_BUILD="${DO_BUILD:-1}"
VERSION_FILE="$ROOT/VERSION"
CHANGELOG_FILE="$ROOT/CHANGELOG.md"
BESTOOL_DOC="$ROOT/docs/bestool-windows.md"
BESTOOL_EXE="${BESTOOL_EXE:-$ROOT/tools/windows/bestool.exe}"
NOTICE_FILE="$ROOT/NOTICE"

bump_semver() {
  local ver="$1" part="$2"
  local major minor patch
  IFS=. read -r major minor patch <<<"$ver"
  major="${major:-0}"
  minor="${minor:-0}"
  patch="${patch:-0}"
  case "$part" in
    major) major=$((major + 1)); minor=0; patch=0 ;;
    minor) minor=$((minor + 1)); patch=0 ;;
    patch) patch=$((patch + 1)) ;;
    *)
      echo "BUMP must be patch, minor, or major (got: $part)" >&2
      exit 1
      ;;
  esac
  echo "${major}.${minor}.${patch}"
}

if [[ ! -f "$VERSION_FILE" ]]; then
  echo "0.1.0" >"$VERSION_FILE"
fi
VERSION="$(tr -d '[:space:]' <"$VERSION_FILE")"
if [[ -n "${BUMP:-}" ]]; then
  VERSION="$(bump_semver "$VERSION" "$BUMP")"
  echo "$VERSION" >"$VERSION_FILE"
  echo "==> Bumped VERSION -> $VERSION"
fi

if [[ ! -f "$CHANGELOG_FILE" ]]; then
  echo "Missing $CHANGELOG_FILE — add a changelog entry for v$VERSION" >&2
  exit 1
fi
if ! grep -qE "^## \[${VERSION//./\.}\]" "$CHANGELOG_FILE"; then
  echo "CHANGELOG.md has no section for [$VERSION] — add one before packaging." >&2
  exit 1
fi
if [[ ! -f "$BESTOOL_DOC" ]]; then
  echo "Missing bestool instructions: $BESTOOL_DOC" >&2
  exit 1
fi
if [[ ! -f "$BESTOOL_EXE" ]]; then
  echo "Missing Windows bestool.exe: $BESTOOL_EXE" >&2
  echo "Place a built binary at tools/windows/bestool.exe (see tools/windows/README.md)." >&2
  exit 1
fi

if [[ "$DO_BUILD" == "1" ]]; then
  echo "==> Building firmware for package v$VERSION (TOTA=$TOTA)"
  STAGE_A="$STAGE_A" TOTA="$TOTA" bash "$ROOT/scripts/build.sh"
fi

if [[ ! -f "$BIN_SRC" ]]; then
  echo "Firmware image not found: $BIN_SRC" >&2
  echo "Run ./scripts/build.sh first, or set DO_BUILD=1." >&2
  exit 1
fi

GIT_SHA="$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
STAMP="$(date -u +%Y%m%d)"
ITER_NAME="pinebuds-cros-v${VERSION}"
ZIP_NAME="${ITER_NAME}.zip"
# Keep a dated+sha alias so history stays unique if VERSION is reused by mistake
ALIAS_NAME="pinebuds-cros-v${VERSION}-${STAMP}-${GIT_SHA}.zip"
STAGE_DIR="$(mktemp -d)"
PKG="$STAGE_DIR/$ITER_NAME"
mkdir -p "$PKG" "$OUT_DIR"

cp -f "$BIN_SRC" "$PKG/open_source.bin"
cp -f "$ROOT/scripts/flash.ps1" "$PKG/flash.ps1"
cp -f "$ROOT/scripts/backup.ps1" "$PKG/backup.ps1"
cp -f "$VERSION_FILE" "$PKG/VERSION"
cp -f "$CHANGELOG_FILE" "$PKG/CHANGELOG.md"
cp -f "$BESTOOL_DOC" "$PKG/BESTOOL.md"
cp -f "$BESTOOL_EXE" "$PKG/bestool.exe"
if [[ -f "$NOTICE_FILE" ]]; then
  cp -f "$NOTICE_FILE" "$PKG/NOTICE"
fi

SIZE="$(wc -c <"$PKG/open_source.bin" | tr -d ' ')"
SHA256="$(sha256sum "$PKG/open_source.bin" | awk '{print $1}')"
BESTOOL_SHA256="$(sha256sum "$PKG/bestool.exe" | awk '{print $1}')"
BESTOOL_BYTES="$(wc -c <"$PKG/bestool.exe" | tr -d ' ')"
BUILT_UTC="$(date -u +%Y-%m-%dT%H:%M:%SZ)"

# Extract this version's changelog section for a short RELEASE_NOTES.txt
python3 - "$CHANGELOG_FILE" "$VERSION" "$PKG/RELEASE_NOTES.txt" <<'PY'
import re, sys
path, ver, out = sys.argv[1], sys.argv[2], sys.argv[3]
text = open(path, encoding="utf-8").read()
pat = rf"(## \[{re.escape(ver)}\][^\n]*\n)(.*?)(?=\n## \[|\Z)"
m = re.search(pat, text, re.S)
body = (m.group(1) + m.group(2)).strip() + "\n" if m else f"## [{ver}]\n(no changelog section found)\n"
open(out, "w", encoding="utf-8").write(body)
PY

cat >"$PKG/MANIFEST.txt" <<EOF
version:     $VERSION
name:        $ITER_NAME
stage:       $STAGE
git_sha:     $GIT_SHA
built_utc:   $BUILT_UTC
target:      $TARGET
stage_a:     $STAGE_A
tota:        $TOTA
bin:         open_source.bin
bin_bytes:   $SIZE
bin_sha256:  $SHA256
bestool:     bestool.exe
bestool_bytes: $BESTOOL_BYTES
bestool_sha256: $BESTOOL_SHA256
repo:        (clone of this project; no absolute owner URL embedded)
files:       open_source.bin bestool.exe flash.ps1 backup.ps1 FLASH.md BESTOOL.md CHANGELOG.md RELEASE_NOTES.txt VERSION MANIFEST.txt SHA256SUMS NOTICE
EOF

cat >"$PKG/SHA256SUMS" <<EOF
$SHA256  open_source.bin
$BESTOOL_SHA256  bestool.exe
EOF

cat >"$PKG/FLASH.md" <<EOF
# PineBuds Pro flash package **v$VERSION** ($STAGE)

DIY / own-risk. **Experimental CROS — not a hearing aid or PPE.** Keep a stock backup.

Start here → **\`BESTOOL.md\`** (flasher) and **\`RELEASE_NOTES.txt\`** (this version).

## What’s in this zip

| File | Purpose |
|------|---------|
| \`VERSION\` | Package version (\`$VERSION\`) |
| \`CHANGELOG.md\` | Full project changelog |
| \`RELEASE_NOTES.txt\` | Notes for **this** version only |
| \`BESTOOL.md\` | How to use the bundled \`bestool.exe\` |
| \`bestool.exe\` | Windows flasher ([Ralim/bestool](https://github.com/Ralim/bestool), MIT + BES programmer blob) |
| \`NOTICE\` | Third-party / SDK notices |
| \`open_source.bin\` | Firmware image (flash to **both** buds) |
| \`flash.ps1\` | Write image via \`bestool\` |
| \`backup.ps1\` | Read stock images before first custom flash |
| \`MANIFEST.txt\` | Build id, git sha, checksum |
| \`SHA256SUMS\` | SHA-256 of bin + bestool |

## One-time Windows setup

1. Install WCH **CH342** driver → Device Manager shows **two** COM ports.
2. Unzip this folder — \`bestool.exe\` is already included (no Rust build needed).
3. Optional restore tool: PINE64 \`dld_main\` + [factory images](https://wiki.pine64.org/wiki/PineBuds_Pro#Firmware_images).

If Windows Defender quarantines \`bestool.exe\`, restore it or allow the folder (unsigned Rust binary).

## Backup once (before any custom flash)

\`\`\`powershell
# Replace COM5 / COM6 with your ports
.\\backup.ps1 -Port0 COM5 -Port1 COM6
\`\`\`

(\`flash.ps1\` / \`backup.ps1\` auto-find \`.\\bestool.exe\`.)

Keep \`backups\\*.bin\` somewhere safe.

## Flash this version (v$VERSION)

1. Unzip this folder.
2. Seat both buds; plug the case in USB.
3. Wake: remove buds ~3s and reseat, **or** long-hold rear button in-case (~5s).
4. Run:

\`\`\`powershell
.\\flash.ps1 -Port0 COM5 -Port1 COM6
\`\`\`

(\`BinPath\` defaults to \`.\\open_source.bin\`.)

5. Leave buds in case ~30–60s for TWS re-pair.

## Lost TWS link / no quad-tap

Quad-tap needs the **bud↔bud** link. If one LED stays in pairing flash:

1. Forget PineBuds Pro on the phone.
2. Both in case + USB: hold **case RESET ~5s**, wait 30–60s (purple LED not required).
3. If still unpaired: power each off (hold ~5s to red), both red+blue → tap 5×, seat 30s+.
4. Last resort: factory restore via \`dld_main\`, confirm stock TWS, re-flash this zip.

## Experimental CROS test (this build)

After both buds re-pair (~30s):

- Wear **both** buds. **Quad-tap** either bud to toggle CROS (needs TWS link).
- Default: **RIGHT = mic (poor / TX)**, **LEFT = speaker (good / RX)**.
- Speak / scratch near the **right** outer face — hear it in the **left** ear.
- Path: 50 ms ADPCM on BESAUD **extra L2CAP** after peer READY; cmd fallback until then.
- Avoid phone music while testing (A2DP fights the CROS stream).

**Phone logs (TOTA=1):** open \`android/cros-log\` → Connect → **Capture logs** on.
Expect handshake lines then \`[cros_log] quiet=1\` — periodic stats are suppressed on
purpose so SPP does not kill extra audio. Transitions (READY / DISABLE) still show.
Share log from the app when done.

DIY / own-risk — **not** a hearing aid.

## Flash budget

On-chip flash has limited erase cycles (~500). Flash only when you mean to.

## Feedback

If you test a build, note: left/right master, worn vs desk, chop y/n, delay feel, Capture on/off, version from \`init v0.x.x\` log line.
EOF

(
  cd "$STAGE_DIR"
  zip -qr "$OUT_DIR/$ZIP_NAME" "$ITER_NAME"
)

cp -f "$OUT_DIR/$ZIP_NAME" "$OUT_DIR/$ALIAS_NAME"
cp -f "$OUT_DIR/$ZIP_NAME" "$OUT_DIR/pinebuds-cros-LATEST.zip"

cat >"$OUT_DIR/LATEST.txt" <<EOF
version=$VERSION
zip=$ZIP_NAME
alias=$ALIAS_NAME
stage=$STAGE
git_sha=$GIT_SHA
built_utc=$BUILT_UTC
bin_sha256=$SHA256
download=flash-packages/pinebuds-cros-LATEST.zip
versioned=flash-packages/$ZIP_NAME
EOF

{
  echo "# Flash packages"
  echo
  echo "> **DIY / own-risk — not a hearing aid or PPE.** Experimental CROS bins (extra-path from v0.3.16+). Not a clinical product."
  echo
  echo "**Current version: v$VERSION**"
  echo
  echo "Download **[pinebuds-cros-LATEST.zip](./pinebuds-cros-LATEST.zip)** or **[$ZIP_NAME](./$ZIP_NAME)**."
  echo
  echo "Each zip includes \`bestool.exe\`, \`BESTOOL.md\`, \`CHANGELOG.md\`, \`RELEASE_NOTES.txt\`, firmware, and PowerShell helpers. See the root [README](../README.md) for test steps."
  echo
  echo "| Package | Version |"
  echo "|---------|---------|"
  # Only list clean semver zips (not dated aliases)
  find "$OUT_DIR" -maxdepth 1 -type f -name 'pinebuds-cros-v*.zip' -printf '%f\n' \
    | grep -E '^pinebuds-cros-v[0-9]+\.[0-9]+\.[0-9]+\.zip$' \
    | sort -V -r \
    | while read -r base; do
        ver="${base#pinebuds-cros-v}"
        ver="${ver%.zip}"
        echo "| [$base](./$base) | v$ver |"
      done
  echo
  echo "See [CHANGELOG.md](../CHANGELOG.md), [bestool guide](../docs/bestool-windows.md), [Windows flashing](../docs/windows-flash.md)."
} >"$OUT_DIR/README.md"

rm -rf "$STAGE_DIR"

echo "==> Packaged v$VERSION -> $OUT_DIR/$ZIP_NAME"
echo "    + $OUT_DIR/pinebuds-cros-LATEST.zip"
echo "    + $OUT_DIR/$ALIAS_NAME"
ls -lh "$OUT_DIR/$ZIP_NAME" "$OUT_DIR/pinebuds-cros-LATEST.zip"
cat "$OUT_DIR/LATEST.txt"
