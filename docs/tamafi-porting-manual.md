# TamaFi Hardware Porting Manual

Last updated: 2026-05-02

This is the practical manual for future TamaFi-like ESP32 projects. It records
what actually worked on the bench, not only what the vendor pages said.

## Current Working Targets

| Target | Port | Status | Build board |
| --- | --- | --- | --- |
| LCDWiki 2.8inch ESP32-S3 Display ES3C28P | COM6 | display, touch, NeoPixel, basic ES8311 tone audio OK | ESP32S3 Dev Module |
| Waveshare ESP32-C6-Touch-LCD-1.47 | COM5 | display and touch OK | ESP32C6 Dev Module |

Default firmware target is the LCDWiki ESP32-S3 profile. Build the C6 target by
passing `-DTAMAFI_BOARD=2`.

## Repo Files To Check First

- `TamaFi/hardware_profile.h`: board selection, pins, touch/display/audio profile.
- `TamaFi/display_backend.h`: TFT_eSPI/Arduino_GFX wrapper and scaling behavior.
- `TamaFi/TamaFi.ino`: touch input, LED handling, sound routing, setup/loop.
- `TamaFi/ui.cpp` and `TamaFi/ui.h`: UI drawing and screen state externs.
- `docs/board-targets.md`: source/research notes from vendor docs.

For a new board, add a new `TAMAFI_BOARD_*` profile first. Keep board details in
one place and avoid scattering pin numbers through the sketch.

## Build Commands

Arduino CLI used from Arduino IDE install:

```powershell
$cli = Join-Path $env:LOCALAPPDATA 'Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
```

ESP32-S3 LCDWiki:

```powershell
& $cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" TamaFi
& $cli upload -p COM6 --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" TamaFi
```

ESP32-C6 Waveshare touch:

```powershell
& $cli compile -j 1 --build-path ".arduino-c6-build" --fqbn "esp32:esp32:esp32c6:FlashSize=8M,PartitionScheme=default_8MB" --build-property "compiler.cpp.extra_flags=-DTAMAFI_BOARD=2" TamaFi
& $cli upload -p COM5 --fqbn "esp32:esp32:esp32c6:FlashSize=8M,PartitionScheme=default_8MB" --input-dir ".arduino-c6-build" TamaFi
```

Notes:

- `GFX Library for Arduino` can make C6 builds very slow, especially at link
  time. Build once into a fixed `--build-path`, then upload with `--input-dir`.
- If a build appears to timeout but `TamaFi.ino.bin` exists in the build path,
  upload that bin directly.
- Clean temporary build folders after upload if they are inside the repo.

## Required Arduino Libraries

- ESP32 core by Espressif Systems, tested with `3.3.8`.
- `GFX Library for Arduino`, tested with `1.6.5`.
- `Adafruit NeoPixel`, tested with `1.15.4`.
- `TFT_eSPI`, still used by the original custom board profile.
- `ESP_I2S`, included with the ESP32 Arduino core, used for S3 ES8311 tone audio.

## LCDWiki ESP32-S3 ES3C28P

Working profile:

```cpp
#define TAMAFI_BOARD_LCDWIKI_ES3C28P 1
#define TAMAFI_DISPLAY_BACKEND TAMAFI_DISPLAY_ARDUINO_GFX
#define TAMAFI_DISPLAY_DRIVER TAMAFI_DISPLAY_DRIVER_ILI9341
#define TAMAFI_DISPLAY_ROTATION 0
#define TAMAFI_DISPLAY_INVERT_COLORS 1
#define TAMAFI_GFX_SPI_FREQUENCY 27000000
```

Pinout:

| Function | GPIO |
| --- | ---: |
| TFT CS | 10 |
| TFT DC | 46 |
| TFT SCK | 12 |
| TFT MOSI | 11 |
| TFT MISO | 13 |
| TFT RST | -1, tied to EN/CHIP_PU |
| TFT BL | 45 |
| Touch SDA | 16 |
| Touch SCL | 15 |
| Touch RST | 18 |
| Touch INT | 17 |
| RGB LED data | 42 |
| Audio EN | 1, active low |
| I2S MCLK | 4 |
| I2S BCLK | 5 |
| I2S DOUT | 6 |
| I2S LRCK | 7 |
| I2S DIN | 8 |

Display notes:

- `TFT_eSPI` produced a black screen during bring-up even though backlight worked.
  `Arduino_GFX` with `Arduino_ILI9341` works.
- Backlight is active high on GPIO45. Set it high early, before or around display
  init, then PWM can take over later.
- Colors required `invertDisplay(true)`, represented by
  `TAMAFI_DISPLAY_INVERT_COLORS 1`.
- SPI at 27 MHz is a safe starting point.

Touch notes:

- Controller is FT6336-compatible on I2C address `0x38`.
- Current code uses a direct I2C read from register `0x02`.
- Logical UI stays 240x240, clipped to the top 240 pixels of the 240x320 panel.

NeoPixel notes:

- There is exactly one onboard RGB LED on GPIO42.
- Do not clear it in every loop. Effects must remain visible after being set.
- `Adafruit_NeoPixel leds(1, 42, NEO_GRB + NEO_KHZ800)` works.

Audio notes:

- Board uses ES8311 at I2C address `0x18`.
- Audio enable GPIO1 is active low.
- Minimal tone output works through `ESP_I2S` with 16-bit stereo at 16 kHz.
- Current sound backend is good enough for UI beeps. For music or WAV playback,
  replace the blocking square-tone path with buffered PCM.

## Waveshare ESP32-C6-Touch-LCD-1.47

Working profile:

```cpp
#define TAMAFI_BOARD_WAVESHARE_C6_TOUCH_147 2
#define TAMAFI_DISPLAY_BACKEND TAMAFI_DISPLAY_ARDUINO_GFX
#define TAMAFI_DISPLAY_DRIVER TAMAFI_DISPLAY_DRIVER_ST7789
#define TAMAFI_DISPLAY_ROTATION 1
#define TAMAFI_DISPLAY_SCALE_MODE TAMAFI_DISPLAY_SCALE_COVER
#define TAMAFI_DISPLAY_INVERT_COLORS 0
#define TAMAFI_GFX_SPI_FREQUENCY 80000000
```

Pinout:

| Function | GPIO |
| --- | ---: |
| LCD SCK | 1 |
| LCD MOSI | 2 |
| LCD MISO | -1 for display |
| LCD CS | 14 |
| LCD DC | 15 |
| LCD RST | 22 |
| LCD BL | 23 |
| Touch SDA | 18 |
| Touch SCL | 19 |
| Touch RST | 20 |
| Touch INT | 21 |
| SD MISO | 3 |
| SD CS | 4 |
| Battery ADC | 0 |

Display notes:

- Panel is listed as JD9853, driven as ST7789-compatible via `Arduino_ST7789`.
- Geometry is 172x320 with column offsets `34, 0, 34, 0`.
- Rotation `1` gives the desired 320x172 landscape UI.
- Full-screen clear every frame causes visible flicker. Clear during init/rotation,
  not before every frame.
- Current UI uses `cover` scaling: it fills the 320-pixel width and crops top/bottom
  instead of showing a tiny 172-pixel square.

Touch notes:

- Touch chip is AXS5106L on I2C address `0x63`.
- Use `Wire.begin(18, 19)` and reset GPIO20.
- Current direct reader follows the Waveshare demo frame:
  - read register `0x01`
  - `data[1]` is touch count
  - first point X is `((data[2] & 0x0F) << 8) | data[3]`
  - first point Y is `((data[4] & 0x0F) << 8) | data[5]`
- Use normal `Wire.endTransmission()` before `requestFrom()`. Repeated-start was
  unreliable here.
- For rotation `1`, Waveshare maps `x = rawY`, `y = rawX`.
- Touch active area after rotation is 320x172.
- Because display uses `cover` scaling, touch coordinates must be mapped back
  through the same cover transform, including cropped top/bottom.

Hardware reuse notes:

- No onboard RGB LED or audio output is currently used for this target.
- Keep `TAMAFI_LED_COUNT 0` and `TAMAFI_SOUND_NONE` unless hardware proves otherwise.

## Bring-Up Checklist For New Boards

1. Start with a dedicated `TAMAFI_BOARD_*` profile.
2. Add display pins, backlight pin, dimensions, rotation, and SPI frequency.
3. Make display show a solid color before testing sprites.
4. If backlight works but screen is black, try the other backend or lower SPI.
5. If colors are negative, use display inversion. If red/blue are swapped, adjust
   RGB/BGR/MADCTL instead.
6. Do not clear the whole screen every frame unless the panel can keep up.
7. Bring touch up with raw coordinates first, then add UI mapping.
8. Match touch transform to display rotation and scaling mode.
9. Test LED separately and avoid clearing it inside the main loop.
10. For codec audio, verify enable pin, I2C address, then I2S clocks.
11. Compile both profiles before declaring the shared code stable.
12. Upload from a fixed build path when Arduino_GFX link time gets silly.

## Known Symptoms And Fixes

| Symptom | Likely cause | Fix |
| --- | --- | --- |
| Backlight on, no S3 picture | `TFT_eSPI` setup/backend mismatch | Use `Arduino_GFX` + `Arduino_ILI9341` |
| S3 colors look inverted | ILI9341 inversion mode | Set `TAMAFI_DISPLAY_INVERT_COLORS 1` |
| C6 shows two images/stale pixels | Old GRAM outside newly drawn area | Clear at init/rotation, avoid stale area or draw full visible area |
| C6 flickers | Full-screen clear every frame | Remove per-frame `fillScreen()` |
| C6 UI too small | Fit scaling 240x240 into 172 px | Use `TAMAFI_DISPLAY_SCALE_COVER` |
| C6 touch dead | Wrong AXS5106L I2C transaction/mapping | Use Waveshare register frame and STOP before read |
| S3 NeoPixel does not appear | Main loop clears it instantly | Do not zero LED each loop |
| S3 no sound | Codec backend stub or EN/I2S not initialized | Enable GPIO1 low, init ES8311 over I2C, output I2S |

## Source Links

- https://www.lcdwiki.com/2.8inch_ESP32-S3_Display
- https://www.waveshare.com/wiki/ESP32-C6-Touch-LCD-1.47
- https://files.waveshare.com/wiki/ESP32-C6-Touch-LCD-1.47/ESP32-C6-Touch-LCD-1.47-Demo.zip
- https://components.espressif.com/components/espressif/es8311
