#pragma once
#include <Arduino.h>
#include <M5Unified.h>
#include "pin_config.h"

/**
 * M5Stack Stopwatch 面板驱动
 * 直接接入 M5Unified 原厂驱动闭环：
 * 自动识别 board_M5StopWatch，自动配置 M5PM1 供电与 M5IOE1 复位，自动驱动 CO5300 AMOLED。
 */
class BoardStopwatchPanel {
public:
  void init(int speedMHz = 40) {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    cfg.clear_display = true;
    M5.begin(cfg);
    M5.Display.setBrightness(180);
    M5.Display.setColorDepth(16);
    M5.Display.setSwapBytes(true);
    M5.Display.fillScreen(TFT_BLACK);
    Serial.printf("[HAL] M5.begin() complete! Board: %d, Display W:%d H:%d\n",
                  (int)M5.getBoard(), M5.Display.width(), M5.Display.height());
  }

  void setBrightness(uint8_t value) {
    M5.Display.setBrightness(value);
  }
};
