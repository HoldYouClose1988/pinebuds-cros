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
APK_SRC="${APK_SRC:-$ROOT/android/CROScontrol.apk}"
INSTALL_PS1="$ROOT/scripts/Install.ps1"

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
cp -f "$INSTALL_PS1" "$PKG/Install.ps1"
cp -f "$VERSION_FILE" "$PKG/VERSION"
cp -f "$CHANGELOG_FILE" "$PKG/CHANGELOG.md"
cp -f "$BESTOOL_DOC" "$PKG/BESTOOL.md"
cp -f "$BESTOOL_EXE" "$PKG/bestool.exe"
if [[ -f "$NOTICE_FILE" ]]; then
  cp -f "$NOTICE_FILE" "$PKG/NOTICE"
fi

APK_INCLUDED=0
APK_SHA256=""
APK_BYTES=0
if [[ -f "$APK_SRC" ]]; then
  cp -f "$APK_SRC" "$PKG/CROScontrol.apk"
  APK_INCLUDED=1
  APK_SHA256="$(sha256sum "$PKG/CROScontrol.apk" | awk '{print $1}')"
  APK_BYTES="$(wc -c <"$PKG/CROScontrol.apk" | tr -d ' ')"
else
  echo "WARN: APK not found at $APK_SRC — packaging without CROScontrol.apk" >&2
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

MANIFEST_FILES="open_source.bin bestool.exe Install.ps1 flash.ps1 backup.ps1 FLASH.md BESTOOL.md CHANGELOG.md RELEASE_NOTES.txt VERSION MANIFEST.txt SHA256SUMS NOTICE"
if [[ "$APK_INCLUDED" == "1" ]]; then
  MANIFEST_FILES="$MANIFEST_FILES CROScontrol.apk"
fi

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
apk:         $([[ "$APK_INCLUDED" == "1" ]] && echo CROScontrol.apk || echo "(not packaged)")
apk_bytes:   $APK_BYTES
apk_sha256:  $APK_SHA256
repo:        (clone of this project; no absolute owner URL embedded)
files:       $MANIFEST_FILES
EOF

{
  echo "$SHA256  open_source.bin"
  echo "$BESTOOL_SHA256  bestool.exe"
  if [[ "$APK_INCLUDED" == "1" ]]; then
    echo "$APK_SHA256  CROScontrol.apk"
  fi
} >"$PKG/SHA256SUMS"

APK_ROW=""
APK_SECTION=""
if [[ "$APK_INCLUDED" == "1" ]]; then
  APK_ROW="| \`CROScontrol.apk\` | Android **CROS Control** app (sideload — not on Play Store) |"
  APK_SECTION="$(cat <<'APKEOF'
## Install the Android app (sideload)

1. Copy `CROScontrol.apk` from this folder to your **Android** phone.
2. On the phone, allow *Install unknown apps* for Files / Chrome / your file manager.
3. Open the APK → Install.
4. Pair **PineBuds Pro** in Bluetooth settings, then open **CROS Control** → Connect → **Apply** knobs once.
5. After Apply, settings live on the buds — quad-tap works without the app open.

App source (build yourself if you prefer): `android/cros-log/` on GitHub.

**iPhone is not supported** for day-to-day BiCROS.
APKEOF
)"
fi

cat >"$PKG/FLASH.md" <<EOF
# PineBuds Pro BiCROS flash package **v$VERSION** ($STAGE)

DIY / own-risk. **Experimental BiCROS — not a hearing aid or PPE.** Keep a stock backup.

## Start here (easiest)

1. Unzip this **entire** folder.
2. Plug the charging case into USB (need **two** COM ports — install the [WCH CH342 driver](http://www.wch-ic.com/downloads/CH343SER_EXE.html) if missing).
3. Right-click \`Install.ps1\` → **Run with PowerShell**  
   (or: \`powershell -ExecutionPolicy Bypass -File .\\Install.ps1\`).
4. Follow the on-screen menu: **Backup THEN flash** the first time.
5. Leave both buds in the case **30–60 s** for TWS re-pair.
6. Pair an **Android** phone. **Quad-tap** toggles BiCROS.
7. Sideload \`CROScontrol.apk\` for knobs / Help (optional but recommended).

If Windows blocks the script: \`Set-ExecutionPolicy -Scope Process Bypass\` then re-run \`Install.ps1\`.  
If Defender quarantines \`bestool.exe\`, restore/allow it (unsigned Rust binary).

Also see **\`BESTOOL.md\`** (Sync detail) and **\`RELEASE_NOTES.txt\`** (this version).

## What’s in this zip

| File | Purpose |
|------|---------|
| \`Install.ps1\` | **Guided installer** — instructions + backup/flash menu + COM detect |
| \`VERSION\` | Package version (\`$VERSION\`) |
| \`CHANGELOG.md\` | Full project changelog |
| \`RELEASE_NOTES.txt\` | Notes for **this** version only |
| \`BESTOOL.md\` | How to use the bundled \`bestool.exe\` |
| \`bestool.exe\` | Windows flasher ([Ralim/bestool](https://github.com/Ralim/bestool), MIT + BES programmer blob) |
| \`NOTICE\` | Third-party / SDK notices |
| \`open_source.bin\` | Firmware image (flash to **both** buds) |
| \`flash.ps1\` | Write image via \`bestool\` (auto-detects COM if omitted) |
| \`backup.ps1\` | Read stock images before first custom flash |
$APK_ROW
| \`MANIFEST.txt\` | Build id, git sha, checksum |
| \`SHA256SUMS\` | SHA-256 of bin + bestool$([[ "$APK_INCLUDED" == "1" ]] && echo " + APK") |

## Critical: Sync order (every bud)

BES2300 only enters the programmer if Sync is ACKed **during reset**:

1. Bud **OUT** (LED awake).
2. Press Enter in the script (Sync starts).
3. **Immediately reseat** that bud.

Do **one** bud at a time. Hang on \`Sent message type Sync\` → Ctrl+C and retry.

## Manual backup / flash (advanced)

\`\`\`powershell
# Ports optional if exactly two COM devices are present
.\\backup.ps1 -Port0 COM5 -Port1 COM6
.\\flash.ps1  -Port0 COM5 -Port1 COM6
\`\`\`

(\`BinPath\` defaults to \`.\\open_source.bin\`. Scripts auto-find \`.\\bestool.exe\`.)

Keep \`backups\\*.bin\` somewhere safe.

$APK_SECTION

## Lost TWS link / no quad-tap

Quad-tap needs the **bud↔bud** link. If one LED stays in pairing flash:

1. Forget PineBuds Pro on the phone.
2. Both in case + USB: hold **case RESET ~5s**, wait 30–60s (purple LED not required).
3. If still unpaired: power each off (hold ~5s to red), both red+blue → tap 5×, seat 30s+.
4. Last resort: factory restore via PINE64 \`dld_main\`, confirm stock TWS, re-flash this zip.

## Experimental BiCROS test (this build)

After both buds re-pair (~30s):

- Wear **both** buds. **Quad-tap** either bud to toggle BiCROS (needs TWS link).
- Default: **RIGHT = mic (poor / TX)**, **LEFT = speaker (good / RX)** — change poor side in the app.
- Speak / scratch near the poor outer face — hear it in the good ear.
- Path: peer SCO mSBC BiCROS (~140 ms) + local good-ear sidetone mix.
- **Turning BiCROS off/on can take ~15–40 s.** That wait is intentional — do not
  case-reset mid-wait unless it truly wedges.
- Avoid phone music while testing (A2DP fights the BiCROS stream).
- **Disable BiCROS before taking a phone call.**

DIY / own-risk — **not** a hearing aid.

## Flash budget

On-chip flash has limited erase cycles (~500). Flash only when you mean to.
EOF

(
  cd "$STAGE_DIR"
  zip -qr "$OUT_DIR/$ZIP_NAME" "$ITER_NAME"
)

cp -f "$OUT_DIR/$ZIP_NAME" "$OUT_DIR/$ALIAS_NAME"

cat >"$OUT_DIR/CURRENT.txt" <<EOF
version=$VERSION
zip=$ZIP_NAME
alias=$ALIAS_NAME
stage=$STAGE
git_sha=$GIT_SHA
built_utc=$BUILT_UTC
bin_sha256=$SHA256
download=flash-packages/$ZIP_NAME
EOF

# Keep a tiny pointer for old scripts that still open LATEST.txt
cp -f "$OUT_DIR/CURRENT.txt" "$OUT_DIR/LATEST.txt"

{
  echo "# Flash packages"
  echo
  echo "> **DIY / own-risk — not a hearing aid or PPE.** Experimental CROS firmware."
  echo
  echo "**Current:** [$ZIP_NAME](./$ZIP_NAME)"
  echo
  echo "| Package | Role |"
  echo "|---------|------|"
  echo "| [$ZIP_NAME](./$ZIP_NAME) | **Current** (v$VERSION) |"
  if [[ -f "$OUT_DIR/pinebuds-cros-v0.3.61.zip" && "$ZIP_NAME" != "pinebuds-cros-v0.3.61.zip" ]]; then
    echo "| [pinebuds-cros-v0.3.61.zip](./pinebuds-cros-v0.3.61.zip) | **Audio baseline** |"
  fi
  echo
  echo "Each zip includes \`Install.ps1\` (guided flasher), \`bestool.exe\`, \`CROScontrol.apk\`,"
  echo "flash scripts, \`CHANGELOG.md\`, and \`RELEASE_NOTES.txt\`."
  echo
  echo "**Older builds:** [GitHub Releases](https://github.com/HoldYouClose1988/pinebuds-cros/releases) (not kept in this folder)."
  echo
  echo "Flash guide: [Windows flashing](../docs/windows-flash.md)."
} >"$OUT_DIR/README.md"

rm -rf "$STAGE_DIR"

echo "==> Packaged v$VERSION -> $OUT_DIR/$ZIP_NAME"
echo "    + $OUT_DIR/$ALIAS_NAME (local alias; not committed — see .gitignore)"
ls -lh "$OUT_DIR/$ZIP_NAME"
cat "$OUT_DIR/CURRENT.txt"
