# BMO PLAYER - portable MP3 player  (@frandorturtles)

Is a pocket MP3 player in a BMO-style 3D-printed case. ESP32 + 2.0" ST7789 screen + PCM5102A DAC (3.5 mm jack) + microSD + LiPo with USB-C charging. **No custom PCB**: commercial modules joined with wires.

Front layout follows the reference photo: screen on top, yellow cross (D-pad), blue triangle, green and red buttons, key-ring eyelet, speaker dimples. `@frandorturtles` is engraved on the back.

```
bmo-player/
  3d-models/  source/ (bmo_case.py = source of the STLs, bmo_player.scad = OpenSCAD mirror)  STL/  renders/
  electronics/ BOM.csv, pinout.csv      wiring-diagrams/ wiring.md + overview PNG
  firmware/   PlatformIO project (src/, lib/bmo_core/, test/host/)
  docs/       assembly/  components/  manufacturing/  validation.md
  tests/      test_pinmap.py  test_stl.py  fit_report.json
```

## Quick start
1. Print the parts (docs/manufacturing/printing.md). 2. Measure your modules and compare with `docs/components/dimensions.md`. 3. Wire (wiring-diagrams/wiring.md). 4. `cd firmware && pio run -t upload`. 5. Put MP3 files in `/music` on a FAT32 microSD.

## Controls
| Button | Now Playing | Menus / library |
|---|---|---|
| Cross up/down | volume | move |
| Cross left/right | previous / next | page / enter |
| Green (OK) | play / pause | select |
| Blue triangle | shuffle (hold: repeat mode) | - |
| Red (BACK) | open menu | back |

## Status - read before building
Verified here: STL geometry (watertight, no collisions between nominal module boxes and the case), pin map (rules + consistency), firmware core logic (ID3, playlist, battery) with host unit tests.
**NOT verified here (no hardware, no PlatformIO, no OpenSCAD in the build environment):** firmware compile on ESP32 and on-device behaviour, bmo_player.scad compile, real module dimensions, headphone loudness from the PCM5102A, charge-while-playing behaviour. See `docs/validation.md` for the exact list.

License: MIT.
