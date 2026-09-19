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

## CROS / SSHL background

- [CROS hearing aid — Wikipedia](https://en.wikipedia.org/wiki/CROS_hearing_aid)
- Commercial reference behavior: Phonak CROS product line (mic on unaidable ear → aid on better ear; BiCROS when better ear also aided)

## Related hobby audio routing

- Phone-side TWS mic relay experiments exist (high latency / SCO call mode); prefer on-bud IBRT path for this project.
