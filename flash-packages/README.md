# Flash packages

> **Work in progress — not a functional CROS product.** Experimental bins only. Not a hearing aid or PPE.

**Current version: v0.1.5** (Windows-safe `backup.ps1`)

## Download (pick one)

### A) Ready zip (preferred when present)
- [pinebuds-cros-LATEST.zip](./pinebuds-cros-LATEST.zip)
- [pinebuds-cros-v0.1.5.zip](./pinebuds-cros-v0.1.5.zip)

### B) Rebuild from parts (if the zip on GitHub is still the old broken one)

```powershell
cd flash-packages\v015-parts
.\EXPAND.ps1
```

That writes `pinebuds-cros-v0.1.5.zip` and updates `pinebuds-cros-LATEST.zip` one folder up.

Then:

```powershell
# unzip the package, ensure bestool.exe is on PATH or in the folder
.\backup.ps1 -Port0 COM3 -Port1 COM4
.\flash.ps1 -Port0 COM3 -Port1 COM4
```

See [CHANGELOG.md](../CHANGELOG.md), [bestool guide](../docs/bestool-windows.md), [Windows flashing](../docs/windows-flash.md).
