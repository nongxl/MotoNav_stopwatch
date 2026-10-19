#pragma once
#include <Arduino.h>
#include "pin_config.h"
#include "Arduino_GFX_Library.h"

/**
 * M5Stack Stopwatch 面板驱动抽象
 * 支持圆屏 GC9A01 / ST7789 等典型秒表硬件，支持背光亮度调节。
 */
class BoardStopwatchPanel {
public:
  Arduino_DataBus *bus = nullptr;
  Arduino_GFX *panel = nullptr;
  bool backlightPWM = false;

  BoardStopwatchPanel() {
#if LCD_BUS_TYPE == 1
    // QSPI 总线
    bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3, true);
#else
    // 典型 SPI 总线 (适用于 M5Dial / ESP32-S3 Stopwatch)
    bus = new Arduino_ESP32SPI(LCD_DC, LCD_CS, LCD_SCLK, LCD_MOSI, LCD_MISO, FSPI, true);
#endif

#if defined(USE_ST7789)
    panel = new Arduino_ST7789(bus, LCD_RESET, 0, true, PANEL_WIDTH, PANEL_HEIGHT);
#else
    // 默认 GC9A01 (240x240 圆屏标准)
    panel = new Arduino_GC9A01(bus, LCD_RESET, 0, true);
#endif
  }

  void init(int speedMHz = 40) {
    if (LCD_BL >= 0) {
      pinMode(LCD_BL, OUTPUT);
      digitalWrite(LCD_BL, LOW);
      backlightPWM = ledcAttach(LCD_BL, 20000, 8);
    }
    if (panel) {
      panel->begin(speedMHz * 1000000);
    }
    setBrightness(200);
  }

  void setBrightness(uint8_t value) {
    if (LCD_BL < 0) return;
    if (backlightPWM) {
      ledcWrite(LCD_BL, value);
    } else {
      digitalWrite(LCD_BL, value > 0 ? HIGH : LOW);
    }
  }
};
