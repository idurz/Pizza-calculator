# CYD Pizza Dough Calculator

An ESP32 firmware project that turns a **Cheap Yellow Display (CYD)** into a pizza dough
recipe calculator. Enter the number of dough balls, ball weight, hydration percentage and
yeast type on the touchscreen; the app computes the exact amounts of flour, water, yeast,
salt and oil and guides you through the process with a timer.

The UI is in Dutch and was designed with SquareLine Studio, exported to LVGL 8.

## Hardware

- **Board:** Cheap Yellow Display (ESP32-2432S028), ST7789 variant
  - ESP32 (`esp32dev`), 2.8" 320x240 ST7789 SPI display, XPT2046 resistive touch
- Orientation: landscape, 270° (rotation 3)

### Pin layout

| Function      | GPIO | Notes                                    |
| ------------- | ---- | ---------------------------------------- |
| TFT MOSI      | 13   | SPI (ST7789)                             |
| TFT CLK       | 14   | SPI                                      |
| TFT CS        | 15   |                                          |
| TFT DC        | 2    |                                          |
| TFT RST       | —    | Not connected (bb_spi_lcd preset)        |
| TFT BL        | 21   | Backlight                                |
| Touch MISO    | 39   | XPT2046                                  |
| Touch MOSI    | 32   | XPT2046                                  |
| Touch CLK     | 25   | XPT2046                                  |
| Touch CS      | 33   | XPT2046                                  |
| Speaker       | 26   | Buzzer                                   |
| RGB LED blue  | 17   | Onboard RGB LED                          |

Display is driven with the bb_spi_lcd `DISPLAY_CYD_2USB` preset (LCD_ST7789,
inversion OFF, 40 MHz, orientation 270) plus custom gamma writes (0xE0/0xE1).

## Screens / flow

1. **Splash screen** — logo/intro
2. **Parameter screen** — set:
   - Aantal bollen (number of dough balls, default 2)
   - Bolgrootte (ball weight, default 220 g)
   - Hydratiepercentage (water-to-flour ratio, default 58 %)
   - Gistsoort (fresh vs. dry yeast)
   - Static recipe constants: 2.5 % salt, 3 % oil
3. **Result screen** — computed amounts (flour, water, yeast, salt, oil) and a timer

## Software stack

| Component        | Version / source                                             |
| ---------------- | ----------------------------------------------------------- |
| Platform         | pioarduino platform-espressif32 (git, Arduino 3.x)          |
| Framework        | Arduino                                                     |
| UI library       | LVGL 8.3.11 (SquareLine Studio v8 export — NOT 9.x compatible) |
| Display driver   | bb_spi_lcd 2.8.1 (git tag — see note below)                 |
| Touch            | XPT2046_Touchscreen (paulstoffregen alpha sha.26b691b2c8)   |

### Critical build notes

- **Pinned libraries — do not upgrade casually:**
  - `lvgl@8.3.11`: the UI is a SquareLine v8 API export; LVGL 9.x breaks it.
  - `bb_spi_lcd#2.8.1`: newer registry versions (2.9.9) have an upstream bug
    (`s3_io_config` used in the generic ESP32 path) and fail on classic ESP32;
    older 2.4.4 lacks the `rtInit` touch API.
  - Platform must be the pioarduino git platform (Arduino 3.x) — the code uses
    `ledcAttach`/`ledcWrite(pin, ...)` which don't exist in official espressif32
    (Arduino 2.x).
- **Color byte-swap:** bb_spi_lcd `pushPixels` does not byte-swap and the SquareLine
  export forces `LV_COLOR_16_SWAP 0`, so `my_disp_flush()` swaps every 16-bit pixel
  with `__builtin_bswap16` before pushing. Removing this garbles all UI colors.
- `[env:esp32dev]` section name is required; a bare `[env]` causes
  `ProjectEnvsNotAvailableError`.

## Build & upload

```sh
~/.platformio/penv/bin/pio run            # build
~/.platformio/penv/bin/pio run -t upload  # flash
~/.platformio/penv/bin/pio device monitor # serial monitor (115200 baud)
```

Notes:

- Use only the PlatformIO core at `~/.platformio/penv/bin/pio` (6.2.0); older/duplicate
  cores (Homebrew, pip user installs) cannot run the pioarduino platform.
- `upload_port = /dev/cu.usbserial-120` is set explicitly in
  [platformio.ini](platformio.ini) because auto-detect can grab a Jabra headset dongle.
  Adjust the port to match your board's enumeration (`/dev/cu.usbserial-*`).

## Project structure

```
platformio.ini        Build config (pinned deps, flags)
src/
  cyd_helloworld.cpp  Main firmware: init, LVGL glue, touch, flush
  ui*.c / ui*.h       SquareLine Studio export (screens, images, events, fonts)
  lv_conf.h           LVGL configuration
  User_Setup.h        Display/touch setup defines
```

## Case

With some searching you'll find 3D-printable enclosures for a CYD project. 
I used one on [Printables](https://www.printables.com/model/793451-case-esp32-2432s028-usb-c-module-cheap-yellow-disp)

## How to use
Plug in the USB-C power supply. You will get the parameter screen in a few seconds. Decide which type of yeast you are using (dried, fresh
