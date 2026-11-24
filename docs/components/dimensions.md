# Module dimensions used by the case (NOMINAL - verify with calipers)

Edit the `MOD` block in `3d-models/source/bmo_case.py`, run `python3 bmo_case.py`, then `python3 ../../tests/test_stl.py`.

| Module | Assumed L x W x H (mm) | What depends on it |
|---|---|---|
| 2.0" ST7789 display | 52 x 36 x 4 | pocket, window 44x33 (active area ~40.8x30.6; position of the active area on the PCB is NOT verified), header side = left |
| ESP32 DevKit (no header pins) | 51.5 x 28.3 x 4.8 | cradle, micro-USB opening (y=76.5, z=10.8). 30/38-pin boards differ |
| TP4056 USB-C | 26 x 17 x 4.6 | USB-C opening (x=21.6, z=12.2) |
| Pololu U1V10F5 | 17.8 x 12.7 x 3.6 | free space only |
| PCM5102A + jack | 31.5 x 17.5 x 6.6 | jack opening dia 8.2 at x=54.05, z=14 (jack axis height above PCB NOT verified) |
| microSD module | 25 x 19 x 4 | slot opening 12.8 x 2.8 at y=45.5 |
| Slide switch SS12D00 | 8.5 x 3.6 x 3.5 | slot 7.2 x 2.6 on left wall |
| Tact switch 6x6 | 6 x 6 x 4.3, legs 4.5 x 6.5 | tray pockets; if height differs change TRAY_Z0 |
| LiPo 603450 | 50 x 34 x 6 | cradle (cells vary +-1 mm) |
