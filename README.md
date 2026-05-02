# TamaFi ESP32 Touch Fork

TamaFi is a WiFi-fed virtual pet for ESP32 boards. This fork ports the original
TamaFi firmware to two touch-screen development boards and replaces the
button-first workflow with touch input.

## Supported Hardware

| Board | Status | Notes |
| --- | --- | --- |
| LCDWiki 2.8inch ESP32-S3 Display ES3C28P | Working | ILI9341 display, FT6336 touch, one NeoPixel, ES8311 tone audio |
| Waveshare ESP32-C6-Touch-LCD-1.47 | Working | JD9853/ST7789-compatible display, AXS5106L touch |
| Original TamaFi custom board | Compatibility profile | TFT_eSPI, buttons, NeoPixels, buzzer |

The default build target is the LCDWiki ESP32-S3 board. The Waveshare C6 board
is selected at compile time with `-DTAMAFI_BOARD=2`.

## Features

- 240x240 logical TamaFi UI adapted to touch displays.
- Touch-first navigation for boot, hatch, home, menus, controls, and settings.
- WiFi-driven pet logic: hunger, happiness, health, mood, stage, environment.
- Persistent state and settings via ESP32 `Preferences`.
- Board profile layer for display, touch, LEDs, audio, buttons, and backlight.
- Arduino_GFX backend for LCDWiki S3 and Waveshare C6 touch targets.
- TFT_eSPI compatibility path for the original custom board.
- Basic ES8311/I2S tone output on the LCDWiki ESP32-S3 board.

## Repository Layout

```text
TamaFi/
  TamaFi.ino             Main firmware, setup/loop, input, logic, sound
  hardware_profile.h     Board profiles, pinout, display/touch/audio options
  display_backend.h      TFT_eSPI and Arduino_GFX compatibility wrapper
  ui.cpp / ui.h          UI rendering and screen state
  ui_anim.h              Animation frame tables
docs/
  tamafi-porting-manual.md  Bench-tested porting manual
  board-targets.md          Vendor research notes and pinout references
PCB/ and Schematic/         Original hardware assets
Ui Graphics/                Source and exported UI artwork
```

## Requirements

- Arduino IDE or Arduino CLI.
- ESP32 Arduino core, tested with `3.3.8`.
- `GFX Library for Arduino`, tested with `1.6.5`.
- `Adafruit NeoPixel`, tested with `1.15.4`.
- `TFT_eSPI`, used by the original custom board profile.
- `ESP_I2S`, included with the ESP32 Arduino core.

## Build And Upload

Set the Arduino CLI path in PowerShell:

```powershell
$cli = Join-Path $env:LOCALAPPDATA 'Programs\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe'
```

Build and upload LCDWiki ESP32-S3 on `COM6`:

```powershell
& $cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" TamaFi
& $cli upload -p COM6 --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" TamaFi
```

Build and upload Waveshare ESP32-C6 touch on `COM5`:

```powershell
& $cli compile -j 1 --build-path ".arduino-c6-build" --fqbn "esp32:esp32:esp32c6:FlashSize=8M,PartitionScheme=default_8MB" --build-property "compiler.cpp.extra_flags=-DTAMAFI_BOARD=2" TamaFi
& $cli upload -p COM5 --fqbn "esp32:esp32:esp32c6:FlashSize=8M,PartitionScheme=default_8MB" --input-dir ".arduino-c6-build" TamaFi
```

The C6 build can take a long time because Arduino_GFX links many display
drivers. If the binary exists after a timeout, upload it directly from the fixed
build path.

## Documentation

- [Hardware Porting Manual](docs/tamafi-porting-manual.md)
- [Board Target Research Notes](docs/board-targets.md)
- [Contributing Guide](CONTRIBUTING.md)
- [Security Policy](SECURITY.md)
- [Support Guide](SUPPORT.md)

## Current Known Limitations

- The UI still uses the original 240x240 logical layout. The C6 target uses
  cover scaling to improve readability on a 320x172 rotated display.
- ES8311 audio currently supports simple blocking tone playback. Full PCM/WAV
  playback should use a buffered audio path.
- C6 does not currently use onboard RGB or audio hardware.
- The original custom board profile is kept for compatibility but is not the
  primary focus of this fork.

## Credits

This fork is based on the original TamaFi project by CiferTech:

- Original repository: https://github.com/cifertech/TamaFi
- Original project page: https://cifertech.net/tamafi-reviving-the-classic-virtual-pet-with-a-tech-twist/

Artwork credits from the original project:

- Mecha-Stone Golem by Darkpixel/Kronovi
- Fantasy Wooden GUI by Kanekizlf
- MyPixelWorld by Scarloxy
- Transparent Sprites by VolosR

## License

MIT License. See [LICENSE](LICENSE).
