# Contributing

Thanks for helping improve this TamaFi fork. The main goal is reliable ESP32
touch-board bring-up, with board-specific details kept clean and reproducible.

## Development Rules

- Put all board-specific pinout and feature flags in `TamaFi/hardware_profile.h`.
- Keep display-specific behavior in `TamaFi/display_backend.h`.
- Avoid hardcoded GPIO numbers in `TamaFi.ino`, `ui.cpp`, or other shared code.
- Do not clear unrelated user settings or saved state during normal firmware
  changes.
- Prefer small, testable changes over broad rewrites.
- Update `docs/tamafi-porting-manual.md` when hardware behavior changes.

## Before Opening A Pull Request

Build every profile affected by your change.

LCDWiki ESP32-S3:

```powershell
arduino-cli compile --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi,PartitionScheme=app3M_fat9M_16MB" TamaFi
```

Waveshare ESP32-C6:

```powershell
arduino-cli compile -j 1 --build-path ".arduino-c6-build" --fqbn "esp32:esp32:esp32c6:FlashSize=8M,PartitionScheme=default_8MB" --build-property "compiler.cpp.extra_flags=-DTAMAFI_BOARD=2" TamaFi
```

If hardware behavior changed, include:

- board name and port,
- what was tested on the physical board,
- display orientation/color status,
- touch behavior,
- LED/audio behavior where applicable.

## Commit Style

Use short imperative commit messages, for example:

```text
Add Waveshare C6 touch profile
Fix S3 ILI9341 color inversion
Document ES8311 audio bring-up
```

## Documentation

The manual is part of the product. If a fix depends on a board quirk, document
the quirk and the fix in `docs/tamafi-porting-manual.md`.
