// BMO PLAYER - GPIO map for ESP32 DevKit (ESP32-WROOM-32, NOT the WROVER/PSRAM variant:
// GPIO16/17 are used here). Must match electronics/pinout.csv (checked by tests/test_pinmap.py).
#pragma once

// ---- microSD (SPI, VSPI default pins) ----
#define PIN_SD_SCK    18
#define PIN_SD_MISO   19
#define PIN_SD_MOSI   23
#define PIN_SD_CS      5

// ---- TFT ST7789 2.0" 240x320 (SPI, HSPI default pins). RST is wired to the ESP32 EN pin. ----
#define PIN_TFT_SCK   14
#define PIN_TFT_MOSI  13
#define PIN_TFT_CS    32
#define PIN_TFT_DC    27
#define PIN_TFT_RST   -1
#define TFT_ROTATION   1          // 1 or 3 -> landscape; change if the image is upside down
#define TFT_SPI_MODE  SPI_MODE3   // some modules need SPI_MODE0 - verify with your module

// ---- I2S -> PCM5102A DAC ----
#define PIN_I2S_BCK   22
#define PIN_I2S_WS    21
#define PIN_I2S_DOUT  17

// ---- Buttons (to GND, internal pull-ups) ----
#define PIN_BTN_UP     4
#define PIN_BTN_DOWN  16
#define PIN_BTN_LEFT  25
#define PIN_BTN_RIGHT 26
#define PIN_BTN_OK    33   // green circle
#define PIN_BTN_BACK  15   // red circle   (strapping pin: keep released while resetting)
#define PIN_BTN_MODE   2   // blue triangle (strapping pin: keep released while resetting)

// ---- Battery sense: 100k / 100k divider from battery+ to GPIO35 (ADC1_CH7, input only) ----
#define PIN_VBAT      35
#define VBAT_DIVIDER  2.0f
