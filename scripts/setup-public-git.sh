#!/usr/bin/env bash
# Configure anonymous git identity + public-push hooks for this workspace.
# Safe to re-run. Does not embed denylist contents in tracked files.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

NAME="${PUBLIC_GIT_NAME:-PineBuds CROS contributors}"
EMAIL="${PUBLIC_GIT_EMAIL:-noreply@users.noreply.github.com}"

git config user.name "$NAME"
git config user.email "$EMAIL"

chmod +x "$ROOT/.githooks/"* 2>/dev/null || true

# Local-only denylist (never committed). Populate with personal handles/emails
# for this machine — do not put those strings in tracked files.
mkdir -p "$ROOT/.git/info"
DENY="$ROOT/.git/info/public-denylist"
if [[ ! -f "$DENY" ]]; then
  cat >"$DENY" <<'EOF'
# Local-only patterns blocked from commits/pushes (not tracked by git).
# One substring per line. Example: personal email local-part, account path segment.
EOF
fi

# Optional: seed from comma-separated PUBLIC_PII_DENYLIST env (not stored in repo).
if [[ -n "${PUBLIC_PII_DENYLIST:-}" ]]; then
  IFS=',' read -ra _pii_items <<<"$PUBLIC_PII_DENYLIST"
  for item in "${_pii_items[@]}"; do
    item="$(echo "$item" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
    [[ -z "$item" ]] && continue
    grep -qiFx -- "$item" "$DENY" 2>/dev/null || echo "$item" >>"$DENY"
  done
fi

# Point "original" hooks at repo .githooks so Cursor's dispatcher runs them first.
HOOKS_DIR="$(git config --get core.hooksPath || true)"
if [[ -n "$HOOKS_DIR" && -d "$HOOKS_DIR" ]]; then
  echo "$ROOT/.githooks" >"$HOOKS_DIR/.cursor-original-hooks-path"

  # Neutralize Cursor co-author injector if present (may be restored; scrub hook below covers that).
  if [[ -f "$HOOKS_DIR/commit-msg.cursor.co-author" ]]; then
    cat >"$HOOKS_DIR/commit-msg.cursor.co-author" <<'EOF'
#!/bin/bash
# Disabled: this repository is public — do not append Co-authored-by identifiers.
exit 0
EOF
    chmod +x "$HOOKS_DIR/commit-msg.cursor.co-author"
  fi

  # Runs after commit-msg.cursor* alphabetically; strips any identity trailers.
  cat >"$HOOKS_DIR/commit-msg.cursor.zz-public-scrub" <<'EOF'
#!/bin/bash
# Project addon: strip identity trailers for public pushes (runs after co-author hook)
set -euo pipefail
MSG_FILE="${1:-}"
[[ -n "$MSG_FILE" && -f "$MSG_FILE" ]] || exit 0
tmp="$(mktemp)"
awk '
  BEGIN { IGNORECASE=1 }
  /^(Co-authored-by|Signed-off-by|Reviewed-by|Acked-by):/ { next }
  { lines[++n] = $0 }
  END {
    while (n > 0 && lines[n] ~ /^[ \t]*$/) n--
    for (i = 1; i <= n; i++) print lines[i]
  }
' "$MSG_FILE" >"$tmp"
mv "$tmp" "$MSG_FILE"
exit 0
EOF
  chmod +x "$HOOKS_DIR/commit-msg.cursor.zz-public-scrub"
else
  # No Cursor hooksPath — use repo hooks directly
  git config core.hooksPath "$ROOT/.githooks"
fi

echo "==> Public git identity: $NAME <$EMAIL>"
echo "==> Hooks ready (strip Co-authored-by; block denylist on push)"
echo "==> Denylist: $DENY"
