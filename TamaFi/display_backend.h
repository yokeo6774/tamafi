#pragma once

#include <Arduino.h>
#include "hardware_profile.h"

#if TAMAFI_DISPLAY_BACKEND == TAMAFI_DISPLAY_TFT_ESPI

#ifndef USER_SETUP_LOADED
#define USER_SETUP_LOADED

#if TAMAFI_DISPLAY_DRIVER == TAMAFI_DISPLAY_DRIVER_ILI9341
#define ILI9341_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 320
#else
#define ST7789_DRIVER
#define TFT_WIDTH  240
#define TFT_HEIGHT 340
#endif

#define TFT_MISO TAMAFI_LCD_MISO
#define TFT_MOSI TAMAFI_LCD_MOSI
#define TFT_SCLK TAMAFI_LCD_SCK
#define TFT_CS   TAMAFI_LCD_CS
#define TFT_DC   TAMAFI_LCD_DC
#define TFT_RST  TAMAFI_LCD_RST
#define TFT_BL   TAMAFI_TFT_BRIGHTNESS_PIN
#define TFT_BACKLIGHT_ON HIGH

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT

#define SPI_FREQUENCY       27000000
#define SPI_READ_FREQUENCY  16000000
#define SPI_TOUCH_FREQUENCY 2500000
#endif

#include <TFT_eSPI.h>
using TamaFiDisplay = TFT_eSPI;
using TamaFiSprite = TFT_eSprite;

#elif TAMAFI_DISPLAY_BACKEND == TAMAFI_DISPLAY_ARDUINO_GFX

#include <Arduino_GFX_Library.h>

#define TFT_BLACK RGB565_BLACK
#define TFT_WHITE RGB565_WHITE
#define TFT_CYAN RGB565_CYAN
#define TFT_MAGENTA RGB565_MAGENTA
#define TFT_DARKGREY RGB565_DARKGREY
#define TFT_RED RGB565_RED
#define TFT_YELLOW RGB565_YELLOW
#define TFT_GREEN RGB565_GREEN

class TamaFiDisplay {
public:
  void init() {
    if (!bus) {
      bus = new Arduino_HWSPI(
          TAMAFI_LCD_DC,
          TAMAFI_LCD_CS,
          TAMAFI_LCD_SCK,
          TAMAFI_LCD_MOSI,
          TAMAFI_LCD_MISO);
    }

    if (!gfx) {
#if TAMAFI_DISPLAY_DRIVER == TAMAFI_DISPLAY_DRIVER_ILI9341
      gfx = new Arduino_ILI9341(
          bus,
          TAMAFI_LCD_RST,
          0,
          false,
          TAMAFI_SCREEN_WIDTH,
          TAMAFI_SCREEN_HEIGHT);
#else
      gfx = new Arduino_ST7789(
          bus,
          TAMAFI_LCD_RST,
          0,
          false,
          TAMAFI_SCREEN_WIDTH,
          TAMAFI_SCREEN_HEIGHT,
          34,
          0,
          34,
          0);
#endif
    }

    gfx->begin(TAMAFI_GFX_SPI_FREQUENCY);
#if TAMAFI_DISPLAY_DRIVER == TAMAFI_DISPLAY_DRIVER_ST7789
    lcdRegInit();
#endif
    gfx->invertDisplay(TAMAFI_DISPLAY_INVERT_COLORS);
    gfx->fillScreen(RGB565_BLACK);
  }

  void setRotation(uint8_t value) {
    rotation = value;
    if (gfx) {
      gfx->setRotation(value);
      gfx->invertDisplay(TAMAFI_DISPLAY_INVERT_COLORS);
      gfx->fillScreen(RGB565_BLACK);
    }
  }

  uint8_t getRotation() const {
    return gfx ? gfx->getRotation() : rotation;
  }

  void setSwapBytes(bool) {}

  void pushFrame(const uint16_t *pixels, int16_t w, int16_t h, int16_t x, int16_t y) {
    if (!gfx || !pixels) return;

    const int16_t displayW = gfx->width();
    const int16_t displayH = gfx->height();

    if (TAMAFI_DISPLAY_SCALE_MODE == TAMAFI_DISPLAY_SCALE_FIT && w <= displayW && h <= displayH) {
      gfx->draw16bitRGBBitmap(x, y, (uint16_t *)pixels, w, h);
      return;
    }

    int16_t outW;
    int16_t outH;
#if TAMAFI_DISPLAY_SCALE_MODE == TAMAFI_DISPLAY_SCALE_COVER
    if ((int32_t)displayW * h > (int32_t)displayH * w) {
      outW = displayW;
      outH = ((int32_t)h * displayW + w - 1) / w;
    } else {
      outH = displayH;
      outW = ((int32_t)w * displayH + h - 1) / h;
    }
#else
    if ((int32_t)displayW * h < (int32_t)displayH * w) {
      outW = displayW;
      outH = ((int32_t)h * displayW + w - 1) / w;
    } else {
      outH = displayH;
      outW = ((int32_t)w * displayH + h - 1) / h;
    }
#endif

    const int16_t outX = x + ((displayW - outW) / 2);
    const int16_t outY = y + ((displayH - outH) / 2);
    uint16_t row[320];

    const int16_t startX = (outX < 0) ? 0 : outX;
    const int16_t startY = (outY < 0) ? 0 : outY;
    const int16_t endX = ((outX + outW) > displayW) ? displayW : (outX + outW);
    const int16_t endY = ((outY + outH) > displayH) ? displayH : (outY + outH);
    const int16_t visibleW = endX - startX;
    if (visibleW <= 0 || endY <= startY) return;

    for (int16_t dy = startY; dy < endY; dy++) {
      int16_t sy = (int32_t)(dy - outY) * h / outH;
      const uint16_t *srcRow = pixels + (int32_t)sy * w;
      for (int16_t dx = startX; dx < endX; dx++) {
        int16_t sx = (int32_t)(dx - outX) * w / outW;
        row[dx - startX] = srcRow[sx];
      }
      gfx->draw16bitRGBBitmap(startX, dy, row, visibleW, 1);
    }
  }

private:
  void lcdRegInit() {
    static const uint8_t initOperations[] = {
        BEGIN_WRITE,
        WRITE_COMMAND_8, 0x11,
        END_WRITE,
        DELAY, 120,

        BEGIN_WRITE,
        WRITE_C8_D16, 0xDF, 0x98, 0x53,
        WRITE_C8_D8, 0xB2, 0x23,
        WRITE_COMMAND_8, 0xB7,
        WRITE_BYTES, 4,
        0x00, 0x47, 0x00, 0x6F,
        WRITE_COMMAND_8, 0xBB,
        WRITE_BYTES, 6,
        0x1C, 0x1A, 0x55, 0x73, 0x63, 0xF0,
        WRITE_C8_D16, 0xC0, 0x44, 0xA4,
        WRITE_C8_D8, 0xC1, 0x16,
        WRITE_COMMAND_8, 0xC3,
        WRITE_BYTES, 8,
        0x7D, 0x07, 0x14, 0x06, 0xCF, 0x71, 0x72, 0x77,
        WRITE_COMMAND_8, 0xC4,
        WRITE_BYTES, 12,
        0x00, 0x00, 0xA0, 0x79, 0x0B, 0x0A, 0x16, 0x79, 0x0B, 0x0A, 0x16, 0x82,
        WRITE_COMMAND_8, 0xC8,
        WRITE_BYTES, 32,
        0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28,
        0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
        0x3F, 0x32, 0x29, 0x29, 0x27, 0x2B, 0x27, 0x28,
        0x28, 0x26, 0x25, 0x17, 0x12, 0x0D, 0x04, 0x00,
        WRITE_COMMAND_8, 0xD0,
        WRITE_BYTES, 5,
        0x04, 0x06, 0x6B, 0x0F, 0x00,
        WRITE_C8_D16, 0xD7, 0x00, 0x30,
        WRITE_C8_D8, 0xE6, 0x14,
        WRITE_C8_D8, 0xDE, 0x01,
        WRITE_COMMAND_8, 0xB7,
        WRITE_BYTES, 5,
        0x03, 0x13, 0xEF, 0x35, 0x35,
        WRITE_COMMAND_8, 0xC1,
        WRITE_BYTES, 3,
        0x14, 0x15, 0xC0,
        WRITE_C8_D16, 0xC2, 0x06, 0x3A,
        WRITE_C8_D16, 0xC4, 0x72, 0x12,
        WRITE_C8_D8, 0xBE, 0x00,
        WRITE_C8_D8, 0xDE, 0x02,
        WRITE_COMMAND_8, 0xE5,
        WRITE_BYTES, 3,
        0x00, 0x02, 0x00,
        WRITE_COMMAND_8, 0xE5,
        WRITE_BYTES, 3,
        0x01, 0x02, 0x00,
        WRITE_C8_D8, 0xDE, 0x00,
        WRITE_C8_D8, 0x35, 0x00,
        WRITE_C8_D8, 0x3A, 0x05,
        WRITE_COMMAND_8, 0x2A,
        WRITE_BYTES, 4,
        0x00, 0x22, 0x00, 0xCD,
        WRITE_COMMAND_8, 0x2B,
        WRITE_BYTES, 4,
        0x00, 0x00, 0x01, 0x3F,
        WRITE_C8_D8, 0xDE, 0x02,
        WRITE_COMMAND_8, 0xE5,
        WRITE_BYTES, 3,
        0x00, 0x02, 0x00,
        WRITE_C8_D8, 0xDE, 0x00,
        WRITE_C8_D8, 0x36, 0x00,
        WRITE_COMMAND_8, 0x21,
        END_WRITE,
        DELAY, 10,

        BEGIN_WRITE,
        WRITE_COMMAND_8, 0x29,
        END_WRITE};

    if (bus) bus->batchOperation(initOperations, sizeof(initOperations));
  }

  Arduino_DataBus *bus = nullptr;
  Arduino_GFX *gfx = nullptr;
  uint8_t rotation = 0;
};

class TamaFiSprite {
public:
  explicit TamaFiSprite(TamaFiDisplay *display) : display(display) {}

  void setColorDepth(uint8_t) {}
  void setSwapBytes(bool value) { swapBytes = value; }

  void createSprite(int16_t w, int16_t h) {
    spriteW = w;
    spriteH = h;
    canvas = new Arduino_Canvas(w, h, nullptr);
    canvas->begin(GFX_SKIP_OUTPUT_BEGIN);
  }

  void fillSprite(uint16_t color) {
    if (canvas) canvas->fillScreen(color);
  }

  void pushSprite(int16_t x, int16_t y) {
    if (display && canvas) {
      display->pushFrame(canvas->getFramebuffer(), spriteW, spriteH, x, y);
    }
  }

  void pushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *data) {
    if (!canvas || !data) return;
    canvas->draw16bitRGBBitmap(x, y, (uint16_t *)data, w, h);
  }

  void pushToSprite(TamaFiSprite *dst, int16_t x, int16_t y, uint16_t transparent) {
    if (!canvas || !dst || !dst->canvas) return;
    uint16_t *src = canvas->getFramebuffer();
    for (int16_t iy = 0; iy < spriteH; iy++) {
      for (int16_t ix = 0; ix < spriteW; ix++) {
        uint16_t color = src[(int32_t)iy * spriteW + ix];
        if (color != transparent) {
          dst->drawPixel(x + ix, y + iy, color);
        }
      }
    }
  }

  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (canvas) canvas->fillRect(x, y, w, h, color);
  }

  void drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (canvas) canvas->drawRect(x, y, w, h, color);
  }

  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    if (canvas) canvas->drawLine(x0, y0, x1, y1, color);
  }

  void drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (canvas) canvas->drawPixel(x, y, color);
  }

  void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
    if (canvas) canvas->fillCircle(x, y, r, color);
  }

  void drawCircle(int16_t x, int16_t y, int16_t r, uint16_t color) {
    if (canvas) canvas->drawCircle(x, y, r, color);
  }

  void setCursor(int16_t x, int16_t y) {
    if (canvas) canvas->setCursor(x, y);
  }

  void setTextColor(uint16_t color) {
    if (canvas) canvas->setTextColor(color);
  }

  void setTextSize(uint8_t size) {
    if (canvas) canvas->setTextSize(size);
  }

  template <typename T>
  size_t print(T value) {
    return canvas ? canvas->print(value) : 0;
  }

  template <typename... Args>
  size_t printf(const char *format, Args... args) {
    return canvas ? canvas->printf(format, args...) : 0;
  }

private:
  TamaFiDisplay *display = nullptr;
  Arduino_Canvas *canvas = nullptr;
  int16_t spriteW = 0;
  int16_t spriteH = 0;
  bool swapBytes = false;
};

#else
#error "Unsupported TamaFi display backend"
#endif
