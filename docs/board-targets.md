# TamaFi Board Targets

Research date: 2026-05-02

This note captures the board pinouts and library choices needed before porting
TamaFi from its current custom ESP32-S3/ST7789/button hardware to the requested
target boards.

For the final bench-tested configuration, build/upload commands, and practical
bring-up notes, use `docs/tamafi-porting-manual.md`. This file is the research
log and still includes some intermediate porting assumptions.

## Current TamaFi Baseline

Firmware stack:

- Arduino framework
- `TFT_eSPI`
- `Adafruit_NeoPixel`
- `WiFi`
- `Preferences`

Current display assumptions:

- 240x240 framebuffer in firmware (`TFT_W`, `TFT_H`)
- `TFT_eSprite` based drawing
- `User_Setup.h` uses `ST7789_DRIVER`, `TFT_WIDTH 240`, `TFT_HEIGHT 340`
- Current TFT pins: MOSI 23, SCLK 18, CS 22, DC 21, RST 12

Current inputs and outputs:

- Buttons: UP 13, OK 12, DOWN 11, RIGHT1 8, RIGHT2 9, RIGHT3 10
- WS2812 LEDs: GPIO1, 4 LEDs
- Buzzer PWM: GPIO2
- TFT backlight PWM: GPIO7

## Porting Direction

The fork should treat both target boards as touch-first devices with no gameplay
buttons. Button logic should remain only as a compatibility backend for the
original custom board.

Input model:

- Tap boot screen: start
- Tap hatch screen: hatch
- Tap home screen: open main menu
- Tap menu row: select that row
- Tap status/environment/system/diagnostics pages: go back to menu
- Tap controls/settings rows: activate or toggle the tapped option

Hardware reuse goals:

- LCDWiki ESP32-S3: use the onboard capacitive touch, onboard WS2812-style RGB
  LED on GPIO42, and the ES8311 audio path instead of the old buzzer.
- Waveshare C6 touch: use the onboard capacitive touch. No programmable RGB LED
  or audio output is documented in the official C6 touch demos, so those should
  stay disabled unless the physical board proves otherwise.

Implementation status in this fork:

- `TamaFi/hardware_profile.h` now describes the custom board, LCDWiki ES3C28P,
  and Waveshare C6 touch target.
- The default profile is now LCDWiki ES3C28P because the ESP32-S3 board is the
  richer target for this fork. Change `TAMAFI_BOARD` in `hardware_profile.h` to
  build for the original custom board or the Waveshare C6 touch board.
- Touch input is routed through a shared tap handler.
- LCDWiki touch uses a direct FT6336 I2C reader at address `0x38`.
- Waveshare C6 touch uses a direct AXS5106L I2C reader at address `0x63`,
  based on the register map used by the official Waveshare demo.
- The display layer now switches by board: LCDWiki/custom boards keep
  `TFT_eSPI`, while Waveshare C6 touch uses `Arduino_GFX` with the ST7789/JD9853
  init sequence from the Waveshare Arduino example.
- The LCDWiki S3 `TFT_eSPI` setup is now defined in the firmware profile instead
  of relying on the old root `User_Setup.h`. It uses ILI9341 with TFT CS 10,
  DC 46, SCK 12, MOSI 11, MISO 13, reset disabled, and backlight GPIO45.
- The C6 display profile is rotated 90 degrees and the square TamaFi UI is
  centered in the rotated 320x172 coordinate space.
- Sound calls now route through a sound backend instead of calling LEDC directly.
  The old buzzer backend still works; the LCDWiki ES8311 codec backend is marked
  as the next integration step because it needs codec initialization over I2C in
  addition to I2S sample output.
- Rendering still uses the existing 240x240 logical UI. The 240x320 and 172x320
  screen layout pass is separate, because the current image arrays are 240x240
  oriented and should not be blindly stretched by changing `TFT_H`.
- Touch is currently clipped to the visible 240-pixel-high TamaFi area so taps
  in unused lower screen space do not trigger hidden UI rows.

Build status:

- LCDWiki ES3C28P compiles with Arduino ESP32 core 3.3.8 when built as ESP32S3
  Dev Module with 16 MB flash, OPI PSRAM, and a 3 MB app partition.
- Waveshare ESP32-C6 touch compiles with Arduino ESP32 core 3.3.8 when built as
  ESP32C6 Dev Module with 8 MB flash and the 8 MB/default app partition.
- Required Arduino libraries in this workstation build: `TFT_eSPI`,
  `Adafruit NeoPixel`, and `GFX Library for Arduino`.

Build commands used:

```powershell
arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" TamaFi
arduino-cli compile --fqbn "esp32:esp32:esp32c6:FlashSize=8M,PartitionScheme=default_8MB" --build-property "compiler.cpp.extra_flags=-DTAMAFI_BOARD=2" TamaFi
```

## Waveshare ESP32-C6-LCD-1.47

Important: the exact URL provided by the user is the non-touch variant:
`ESP32-C6-LCD-1.47`. Waveshare has a separate touch variant named
`ESP32-C6-Touch-LCD-1.47`; see the next section.

Official specs:

- MCU: ESP32-C6FH4
- Flash: 4 MB
- Display: 1.47 inch TFT, ST7789, 172x320, 262K colors
- Wireless: Wi-Fi 6, BLE 5, IEEE 802.15.4
- Onboard: TF card slot, RGB LED, BOOT, RESET
- No touch controller is documented for this exact non-touch page.

Pinout from Waveshare wiki and demo:

| Function | GPIO |
| --- | ---: |
| LCD MOSI | 6 |
| LCD SCLK | 7 |
| LCD MISO / SD MISO | 5 |
| LCD CS | 14 |
| LCD DC | 15 |
| LCD RST | 21 |
| LCD BL | 22 |
| RGB LED data | 8 |
| SD CS | 4 |
| SD MOSI | 6 |
| SD SCLK | 7 |
| SD MISO | 5 |

Display driver details from official Arduino demo:

- `LCD_WIDTH 172`
- `LCD_HEIGHT 320`
- `SPIFreq 80000000`
- `Offset_X 34`
- `Offset_Y 0`
- Backlight PWM uses GPIO22, 1 kHz, 10-bit resolution
- The LVGL touch read callback in the non-touch demo is a dummy/no-op.

Libraries from Waveshare:

- Arduino ESP32 core: `esp32 by Espressif Systems >= 3.0.0`
- Arduino demo libraries: `lvgl 8.3.10`, `PNGdec 1.0.2`
- For TamaFi's existing code, `TFT_eSPI` can probably still be used if configured
  for ST7789, 172x320, pins above, and the 34-pixel column offset. The official
  Waveshare demo uses a custom ST7789 driver instead of `TFT_eSPI`.

## Waveshare ESP32-C6-Touch-LCD-1.47

This appears to be the board that matches "C6 1.47 with touch".

Official specs:

- MCU: ESP32-C6FH8
- Flash: 8 MB
- Display: 1.47 inch capacitive touch LCD, 172x320, 262K colors
- Display chip listed by Waveshare: JD9853
- Touch chip: AXS5106L
- Onboard: TF card slot, QMI8658 IMU, battery ADC, BOOT, RESET

Pinout from official Arduino demos:

| Function | GPIO |
| --- | ---: |
| LCD SCK | 1 |
| LCD MOSI | 2 |
| LCD MISO | unused in display demo |
| LCD CS | 14 |
| LCD DC | 15 |
| LCD RST | 22 |
| LCD BL | 23 |
| Touch SDA | 18 |
| Touch SCL | 19 |
| Touch RST | 20 |
| Touch INT | 21 |
| SD SCK | 1 |
| SD MOSI | 2 |
| SD MISO | 3 |
| SD CS | 4 |
| Battery ADC | 0 |
| QMI8658 I2C SDA/SCL | 18/19 |
| QMI8658 I2C address | 0x6B |

Display and touch driver details from official Arduino demo:

- Display object: `Arduino_ST7789`
- Bus: `Arduino_HWSPI(15 /* DC */, 14 /* CS */, 1 /* SCK */, 2 /* MOSI */)`
- Geometry: 172x320, `col_offset1 34`, `row_offset1 0`, `col_offset2 34`,
  `row_offset2 0`
- Requires custom `lcd_reg_init()` sequence for the JD9853-compatible panel
- Touch I2C: `Wire.begin(18, 19)`
- Touch init: `bsp_touch_init(&Wire, 20, 21, rotation, width, height)`
- Touch address from library: `AXS5106L_ADDR 0x63`

Libraries from Waveshare:

- Arduino ESP32 core: `esp32 by Espressif Systems >= 3.0.0`
- `lvgl 8.4.0`
- `GFX Library for Arduino 1.5.9`
- `FastIMU 1.2.8`
- The Waveshare demo includes `esp_lcd_touch_axs5106l`; the fork currently uses
  a tiny direct I2C reader instead of requiring that library.

Porting note:

- This touch variant is not a drop-in `TFT_eSPI` board in the official demos.
  This fork uses an Arduino_GFX backend for the C6 target.
- The existing 240x240 TamaFi UI is scaled down to fit the 172-pixel-wide C6
  panel. A proper C6-specific layout can improve readability later.

## LCDWiki 2.8inch ESP32-S3 Display

Official specs:

- SKU with touch: ES3C28P
- SKU without touch: ES3N28P
- MCU: ESP32-S3 N16R8
- CPU: dual-core Xtensa LX7, 240 MHz
- Memory: 384 KB ROM, 512 KB SRAM, 16 KB RTC SRAM, 8 MB internal OPI PSRAM,
  16 MB external SPI Flash
- Display: 2.8 inch IPS TFT, 240x320, ILI9341V, 4-line SPI
- Touch: D-FT6336G capacitive touch, I2C

Pinout from LCDWiki page and Arduino demo instructions:

| Function | GPIO |
| --- | ---: |
| TFT CS | 10 |
| TFT DC / RS | 46 |
| TFT SCK | 12 |
| TFT MOSI | 11 |
| TFT MISO | 13 |
| TFT RST | EN / CHIP_PU |
| TFT BL | 45 |
| Touch SDA | 16 |
| Touch SCL | 15 |
| Touch RST | 18 |
| Touch INT | 17 |
| RGB LED data | 42 |
| SD CLK | 38 |
| SD CMD | 40 |
| SD D0 | 39 |
| SD D1 | 41 |
| SD D2 | 48 |
| SD D3 | 47 |
| Battery ADC | 9 |
| Audio EN | 1 |
| I2S MCK | 4 |
| I2S SCK/BCLK | 5 |
| I2S DO | 6 |
| I2S LRCK | 7 |
| I2S DI | 8 |
| BOOT key | 0 |
| UART RX0 | 43 |
| UART TX0 | 44 |
| Expand GPIO | 2, 3, 14, 21 |

Libraries from LCDWiki Arduino instructions:

- Arduino IDE: 2.3.4 in the vendor docs
- Arduino ESP32 core: 3.2.0 in the vendor docs
- Core display/UI: `TFT_eSPI`, `lvgl` v8.x only, `FT6336-arduino`
- Common demo libraries: `Adafruit_NeoPixel`, `TJpg_Decoder`, `ESP32Time`,
  `ArduinoJson`, `ArduinoWebsockets`, `HttpClient`, `NTPClient`, `Time`,
  `UrlEncode`, `base64_encode`
- Optional audio demo libraries: `arduino-audio-driver`, `arduino-audio-tools`,
  `ESP32-audioI2S`, `XT_DAC_Audio`

Porting note:

- This board is much closer to the current TamaFi graphics stack because the
  vendor examples use `TFT_eSPI`.
- TamaFi must still be adapted from 240x240 to 240x320.
- Physical button navigation can be replaced by touch zones or LVGL-style pointer
  handling. Only BOOT is a built-in key; GPIO2/3/14/21 are available on the
  expansion connector if hardware buttons are still desired.

## Sources

- https://www.waveshare.com/wiki/ESP32-C6-LCD-1.47
- https://files.waveshare.com/wiki/ESP32-C6-LCD-1.47/ESP32-C6-LCD-1.47-Demo.zip
- https://www.waveshare.com/wiki/ESP32-C6-Touch-LCD-1.47
- https://files.waveshare.com/wiki/ESP32-C6-Touch-LCD-1.47/ESP32-C6-Touch-LCD-1.47-Demo.zip
- https://www.lcdwiki.com/2.8inch_ESP32-S3_Display
- https://www.lcdwiki.com/res/ES3C28P/2.8inch_ES3C28P_ES3N28P_arduino_Demo_Instructions.pdf
