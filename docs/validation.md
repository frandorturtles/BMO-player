# Validation report

## Done and passing (run `tests/`)
* STL: all 11 files watertight, positive volume, expected bounding boxes (case 76 x 110 x 26 + 7.5 mm eyelet).
* Fit: nominal module boxes vs. every printed part and module-vs-module: **0 interferences** (tests/fit_report.json). All modules lie inside the cavity.
* Pin map: 19 signals unique, no flash pins, no strapping pins 0/12, no button on input-only pins, pins.h == pinout.csv.
* Firmware core (ID3 v2.2/2.3/2.4, UTF-8/UTF-16/Latin-1 folding, shuffle/repeat, battery curve): host unit tests with ASan/UBSan pass.

## Electrical reasoning (not measured)
* ESP32 3V3 load: ESP32 ~100-240 mA + TFT ~40 mA + SD ~50-100 mA peak + DAC ~20 mA < 800 mA of the onboard LDO.
* 5 V boost from 3.0-4.2 V: ESP32 draws < 300 mA at 5 V -> < 0.5 A from the cell. Check the boost's output current at 3.3 V input in its datasheet.
* Battery: single cell with DW01 protection (TP4056 board WITH protection) -> 4.2 V charge limit, over-discharge cut. Divider 2 x 100k -> 2.1 V max at GPIO35.
* Charging while playing: the TP4056 has no load sharing, so the charge termination can be disturbed; switch OFF while charging for a full charge.

## PENDING - must be verified on the real build
1. Exact module dimensions (docs/components/dimensions.md), especially display active-area offset, ESP32 board length, jack height.
2. Firmware compile on ESP32 with `esphome/ESP32-audioI2S@2.0.7` and `espressif32@6.5.0` (versions must be confirmed to resolve; API differs in 3.x).
3. Headphone loudness: PCM5102A is a line-level DAC (2.1 Vrms); low-impedance headphones may be quiet. If so add a headphone-amp module.
4. TFT orientation / SPI mode (`TFT_ROTATION`, `TFT_SPI_MODE` in pins.h) and whether BL pin needs a resistor.
5. Boost output current at low battery, TP4056 behaviour, battery runtime.
6. bmo_player.scad compile (the STLs do not depend on it).
7. Buy links are search/catalog links; confirm each listing matches the specs.
