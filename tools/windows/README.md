# Windows host tools (redistributed)

| File | Source | License |
|------|--------|---------|
| `bestool.exe` | [Ralim/bestool](https://github.com/Ralim/bestool) (user-built release binary) | MIT (tool); embedded `programmer.bin` is BES copyright per upstream README |

Flash packages copy `bestool.exe` into each zip so Windows users can run `flash.ps1` / `backup.ps1` without a separate Rust build.

Replace this binary by dropping a newer `bestool.exe` here and re-running `./scripts/package-flash.sh`.
