#pragma once
#include <Arduino.h>
#include <M5Unified.h>
#include "pin_config.h"
#include "PortScale.h"

/**
 * M5Stack Stopwatch 硬件抽象层 (HAL)
 * 基于 M5Unified 的触控、电源与按键管理。
 */
class BoardStopwatchHal {
public:
  bool touchOK = true;

  void init() {
    // 基础硬件初始化已在 BoardStopwatchPanel::init 中通过 M5.begin() 完成
  }

  void initTouch() {
    touchOK = true;
  }

  bool getTouchPoint(int16_t &outX, int16_t &outY) {
    M5.update();
    if (M5.Touch.getCount() > 0) {
      auto detail = M5.Touch.getDetail(0);
      if (detail.isPressed()) {
        outX = detail.x;
        outY = detail.y;
        return true;
      }
    }
    return false;
  }

  void initPower() {
  }

  int getBatteryPct() {
    int pct = M5.Power.getBatteryLevel();
    return pct >= 0 ? pct : 100;
  }

  void powerOff() {
    Serial.println("[HAL] Power off requested");
    M5.Power.powerOff();
  }

  void initButtons() {
  }
};
