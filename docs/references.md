# References

## Hardware & vendor docs

- [PineBuds Pro — PINE64 wiki](https://wiki.pine64.org/wiki/PineBuds_Pro)
- [BES2300-YP datasheet (PDF)](https://files.pine64.org/doc/datasheet/PineBudsPro/BES2300-YP_Datasheet_v1.0.pdf)
- [Earbuds schematic 20220817 (PDF)](https://files.pine64.org/doc/PineBudsPro/PineBuds%20Pro%20mainboard%20schematic-20220817.pdf)
- [Charging case schematic 20220915](https://wiki.pine64.org/wiki/PineBuds_Pro#Information,_Schematics_and_Certifications)
- Factory programmer + stock images: wiki “Firmware images” section

## Firmware trees

- [pine64/OpenPineBuds](https://github.com/pine64/OpenPineBuds)
- [ANC support discussion (#41)](https://github.com/pine64/OpenPineBuds/issues/41)
- [hall/little-buddy](https://github.com/hall/little-buddy)
- [Ralim/bestool](https://github.com/Ralim/bestool)
- Windows flash workflow for this project: [docs/windows-flash.md](windows-flash.md)
- [PINE64 Windows programmer manual (PDF)](https://files.pine64.org/os/PineBudsPro/PineBuds%20Pro%20programmer%20user%20manual.pdf)

## Community BES / PineBuds forks (surveyed 2026-09-24)

### [erik-smit/EriksPineBuds](https://github.com/erik-smit/EriksPineBuds)

OpenPineBuds fork: **BLE GATT companion** (touch + parametric EQ) + Android app.
Same closed `ibrt_libbt_profiles_*.a` as us (MD5 match) — **does not unlock ACL buffers**.

**Useful to us:**
- Working **BLE=1** + custom GATT (`docs/GATT_SPEC.md`, `services/ble_profiles/opb_*`, `android/`) — template if we ever move Capture logs off classic SPP to cut ACL contention with extra CROS (see [latency-and-next.md](latency-and-next.md) §L′).
- TWS config sync via `app_tws_if_register_sync_user` (small blobs on IBRT sync path — not a media pipe).
- Stock `plc_utils` / `MSBC_PLC_ENABLE` already in upstream; SCO-oriented, still a reference for option **A**.

**Not useful for CROS latency:** EQ, gesture remapping, QR sharing.

### [nnonickreal/openqore](https://github.com/nnonickreal/openqore) (+ [openqore-sdk](https://github.com/nnonickreal/openqore-sdk), [besota](https://github.com/nnonickreal/besota))

BES2300\* Soundcore (Q35 etc.) toolkit: binary **sound-prompt patcher**, UART flash docs, OTA flasher.
SDK is OpenPineBuds retargeted to over-ear Q35 — **same `HCI_NUM_ACL_BUFFERS=6` + closed profiles `.a`**.

**Useful to us:**
- [besota](https://github.com/nnonickreal/besota): reverse-engineered **BES OTA over RFCOMM** (`UUID 66666666-…` = `BES_OTA_UUID_128`). Confirms §J — that channel is real when OTA is enabled; still classic ACL, not a CROS media path. Handy if we ever enable `OTA_ENABLE` / wireless flash.
- UART / OTA_BOOT / CRC notes for unbrick workflows (we already use bestool).

**Not useful for CROS:** alert-sound injection, Q35 ANC/button maps, over-ear (no TWS bud↔bud mic).

## CROS / SSHL background

- [CROS hearing aid — Wikipedia](https://en.wikipedia.org/wiki/CROS_hearing_aid)
- Commercial reference behavior: Phonak CROS product line (mic on unaidable ear → aid on better ear; BiCROS when better ear also aided)

## Related hobby audio routing

- Phone-side TWS mic relay experiments exist (high latency / SCO call mode); prefer on-bud IBRT path for this project.

## Logging / vendor SPP

- In-tree TOTA/SPP investigation for this project: [bt-log-sink.md](bt-log-sink.md)
- BES earbuds often expose vendor RFCOMM (commonly channel 12 / SPP 0x1101); Android HCI snoop + Wireshark SPP filter is the usual RE path
- Example community SPP companions (not PineBuds-specific): Motobuds RFCOMM notes, Soundcore SPP RE writeups
- **Later alternative:** Erik-style BLE GATT log notify (see community forks above) if classic SPP quiet mode is still too heavy under Capture
