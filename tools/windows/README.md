# Windows host tools

| File | Source | License |
|------|--------|---------|
| `bestool.exe` | [Ralim/bestool](https://github.com/Ralim/bestool) (user-built release binary) | MIT (tool); embedded `programmer.bin` is BES copyright per upstream README |
| `Dump.ps1` | This repo | Diagnostics: full-image dump + BiCROS NV knob scan |

Flash packages copy `bestool.exe` into each zip so Windows users can run `Install.ps1` / `flash.ps1` / `backup.ps1` without a separate Rust build. **`Dump.ps1` stays here** (developer / support tool) and is not bundled in the flash zip.

```powershell
# From an unzipped flash folder that already has bestool.exe:
powershell -ExecutionPolicy Bypass -File ..\..\tools\windows\Dump.ps1
# Or copy Dump.ps1 next to bestool.exe in that folder.
```

Replace `bestool.exe` by dropping a newer build here and re-running `./scripts/package-flash.sh`.
