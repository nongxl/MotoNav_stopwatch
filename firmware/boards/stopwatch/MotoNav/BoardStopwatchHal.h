#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "pin_config.h"
#include "PortScale.h"
#include "TouchDrvFT6X36.hpp"

/**
 * M5Stack Stopwatch 硬件抽象层 (HAL)
 * 解耦触摸、电池电量、按键与电源管理。
 */
class BoardStopwatchHal {
public:
  TouchDrvFT6X36 touch;
  bool touchOK = false;
  int batPercentage = -1;
  int lastEncoderPos = 0;

  void init() {
    Wire.begin(IIC_SDA, IIC_SCL);
    initTouch();
    initPower();
    initButtons();
  }

  // ----------------------------------------------------------------------------
  // 触摸驱动
  // ----------------------------------------------------------------------------
  void initTouch() {
    if (TP_INT >= 0 && TP_RESET >= 0) {
      touch.setPins(TP_RESET, TP_INT);
    }
    touchOK = touch.begin(Wire, TP_I2C_ADDR, IIC_SDA, IIC_SCL);
    Serial.printf("[HAL] Stopwatch touch init %s (addr: 0x%02X)\n", touchOK ? "SUCCESS" : "FAILED", TP_I2C_ADDR);
  }

  bool getTouchPoint(int16_t &outX, int16_t &outY) {
    if (!touchOK) return false;
    int16_t rawX[1] = {0}, rawY[1] = {0};
    uint8_t n = touch.getPoint(rawX, rawY, 1);
    if (n > 0) {
      // 通过 PortScale 转换为 466x466 逻辑画布坐标
      outX = (int16_t)PortScale::touchX(rawX[0]);
      outY = (int16_t)PortScale::touchY(rawY[0]);
      return true;
    }
    return false;
  }

  // ----------------------------------------------------------------------------
  // 电源与电池管理
  // ----------------------------------------------------------------------------
  void initPower() {
#if PMU_CHIP_TYPE == 1
    // AXP2101 初始化（如有）
    Serial.println("[HAL] Initializing AXP2101 PMU...");
#elif PMU_CHIP_TYPE == 0
    // ADC 电池电压检测
    if (BAT_ADC_PIN >= 0) {
      pinMode(BAT_ADC_PIN, INPUT);
    }
#endif
  }

  int getBatteryPct() {
#if PMU_CHIP_TYPE == 0
    if (BAT_ADC_PIN < 0) return 100;
    // 简易 ADC 读取：12-bit ADC 0~4095
    int raw = analogRead(BAT_ADC_PIN);
    // 假设 2:1 分压电阻 (具体比例可随板卡调整)
    float voltage = (raw / 4095.0f) * 3.3f * 2.0f;
    if (voltage < 3.3f) return 0;
    if (voltage > 4.2f) return 100;
    return (int)((voltage - 3.3f) / (4.2f - 3.3f) * 100.0f);
#else
    return 100; // 默认满电，如挂接 AXP 则从 AXP 寄存器读取
#endif
  }

  void powerOff() {
    Serial.println("[HAL] Device entering deep sleep / power off");
    esp_deep_sleep_start();
  }

  // ----------------------------------------------------------------------------
  // 实体按键与编码器
  // ----------------------------------------------------------------------------
  void initButtons() {
    if (BUTTON_A_PIN >= 0) pinMode(BUTTON_A_PIN, INPUT_PULLUP);
    if (BUTTON_B_PIN >= 0) pinMode(BUTTON_B_PIN, INPUT_PULLUP);
    if (ENCODER_PIN_A >= 0) pinMode(ENCODER_PIN_A, INPUT_PULLUP);
    if (ENCODER_PIN_B >= 0) pinMode(ENCODER_PIN_B, INPUT_PULLUP);
  }
};
