# Public git / no-identifiers policy

This repository is **public**. Do not put personal identifiers in commits, messages, docs, or flash packages.

## Rules

- Author/committer: `PineBuds CROS contributors <noreply@users.noreply.github.com>` only
- No `Co-authored-by`, `Signed-off-by`, or similar identity trailers
- No personal emails, handles, or `github.com/<account>/...` owner URLs in tracked files (use relative links)
- Flash package manifests must not embed absolute owner URLs

## Setup (Cloud Agent / local)

```bash
./scripts/setup-public-git.sh
```

This configures the anonymous identity, installs scrub/block hooks (including neutralizing Cursor’s co-author injector), and creates a **local-only** denylist at `.git/info/public-denylist` (not pushed).

`environment.json` install runs this automatically.
