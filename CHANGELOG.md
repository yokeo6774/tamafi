# Changelog

All notable changes to this fork are documented here.

## Unreleased

- Ported TamaFi to LCDWiki 2.8inch ESP32-S3 Display ES3C28P.
- Added Waveshare ESP32-C6-Touch-LCD-1.47 profile.
- Added board profile abstraction for display, touch, LEDs, audio, buttons, and
  backlight.
- Added Arduino_GFX display backend wrapper.
- Added touch-first navigation for boards without buttons.
- Added direct FT6336 and AXS5106L touch readers.
- Added basic ES8311/I2S tone audio for the LCDWiki S3 board.
- Fixed LCDWiki S3 ILI9341 color inversion.
- Fixed C6 display flicker by avoiding full-screen clear every frame.
- Added hardware porting manual and GitHub community documentation.
