# Monday checklist — Stage A (FF mic loopback)

Hardware arrives. Goal: **one careful flash**, then clap-test that the outward FF mic plays through the speaker on the **same** bud.

## Before you leave home / first plug-in

1. Install **CH342** driver; confirm **two COM ports** in Device Manager.
2. Download the flash package from GitHub (see below) and follow **`BESTOOL.md`** inside the zip (or [docs/bestool-windows.md](bestool-windows.md)).
3. Optional restore: PINE64 `dld_main` + factory images.

## Backup (do this once before any custom flash)

From the unzipped package folder:

```powershell
.\backup.ps1 -Port0 COMx -Port1 COMy
```

Save `backups\*.bin` somewhere safe (cloud drive / USB). Also bookmark [factory images](https://wiki.pine64.org/wiki/PineBuds_Pro#Firmware_images).

## Get Stage A firmware (preferred)

Download the ready-to-flash zip (includes bin + scripts + FLASH.md):

- **Always newest:** [pinebuds-cros-LATEST.zip](../flash-packages/pinebuds-cros-LATEST.zip)
- Folder / older iterations: [flash-packages/](../flash-packages/)

Unzip, then flash from that folder. (WSL/cloud build still works if you prefer: `./scripts/build.sh`.)

## Flash both buds

```powershell
.\flash.ps1 -Port0 COMx -Port1 COMy
```

Wake tips: remove buds ~3s and reseat, or long-hold rear button in case (~5s).

Leave in case ~30s after flash for TWS re-pair.

## Clap / talk test

1. Take **one** bud out; wear it (or seal the tip) — open-air on a desk can howl.
2. **Do not play phone music** during this test (A2DP will fight the loopback streams).
3. Clap / talk near the **outer** face (logo / FF mic). You should hear it in that same ear with a short delay.
4. **Quad-tap** the touch pad: toggles loopback off/on.
5. Repeat on the other bud.

### Pass / fail

| Result | Meaning | Next |
|--------|---------|------|
| Hear ambient clearly, no wild howl when worn | Stage A **pass** | Stage B: TWS mode sync; Stage C: cross-bud audio |
| Silence | Wrong mic path, stream failed, or muted | UART log @ 2 Mbaud; tell agent what you hear/see |
| Loud howl | Gain too high / open-air feedback | Quad-tap off; report; we lower `CROS_LB_GAIN_Q15` |
| Only one bud works | Flash/port issue | Re-flash the quiet COM port |

## What this build does / does not

- **Does:** FF (MIC1) → local speaker on each bud at boot.
- **Does not:** CROS to the other ear yet; SITE mode; Android app.

## Flash budget reminder

Treat this as **one** of your scarce erase cycles. Only re-flash when we change the bin on purpose.

## Report back

After work testing, note: left/right, worn vs desk, clap vs speech, howl y/n, quad-tap worked y/n, phone paired y/n. That drives the next patch.
