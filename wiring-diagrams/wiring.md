# BMO PLAYER - complete wiring (module to module, no custom PCB)

All grounds are common. Use 26-30 AWG silicone wire; solder directly to the module pads (no header pins) to save thickness.

## Power path
| From | To | Notes |
|---|---|---|
| LiPo + (red) | TP4056 `B+` | cell must have its own protection or use the TP4056 board WITH DW01+FS8205 |
| LiPo - | TP4056 `B-` | |
| TP4056 `OUT+` | slide switch pin 1 | switch interrupts the positive line only |
| slide switch pin 2 | Pololu U1V10F5 `VIN` | and to the 100k divider top (see below) |
| TP4056 `OUT-` | boost `GND` | common ground |
| Boost `VOUT` (5V) | ESP32 `VIN`/`5V` pin | the ESP32 onboard LDO makes 3V3 |
| ESP32 `3V3` | display VCC + BL, microSD VCC, DAC VCC | total 3V3 load est. < 300 mA (verify) |
| Switch pin 2 -> 100k -> GPIO35 -> 100k -> GND | battery sense | 2.1 V max at the pin |

## Signals
See `electronics/pinout.csv` (single source, tested against `firmware/src/pins.h`).

## Module settings
* PCM5102A: solder pad `SCK` -> GND (internal PLL). `FLT`=L, `DEMP`=L, `FMT`=L (I2S), `XSMT`=H (un-mute). Check pad labels on YOUR board.
* ST7789: `RES` -> ESP32 `EN` pin. `BL` -> 3V3.
* microSD: use a module that is 3.3 V logic (no 5 V level-shifter that needs 5 V).
* USB-C (TP4056 module) only charges. Programming uses the ESP32 micro-USB (side opening).
* Do not flash or run from USB while the LiPo is charging with the switch ON for long periods: TP4056 has no load sharing (see docs/components).
