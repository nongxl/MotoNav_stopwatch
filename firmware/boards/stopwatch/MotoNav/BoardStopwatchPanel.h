#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "pin_config.h"
#include "Arduino_GFX_Library.h"

// 简单可靠的 I2C 寄存器读写辅助
static inline void stopwatchI2CWrite(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static inline uint8_t stopwatchI2CRead(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom(addr, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}

/**
 * M5Stack Stopwatch 面板驱动
 * 硬件特性：466x466 AMOLED (CO5300 驱动，QSPI 接口)
 * 依赖 M5PM1 开启 3.3V DCDC/LDO 供电，依赖 M5IOE1 控制 OLED/TP 复位。
 */
class BoardStopwatchPanel {
public:
  Arduino_DataBus *bus = nullptr;
  Arduino_CO5300 *panel = nullptr;

  BoardStopwatchPanel() {
    // QSPI 总线与 CO5300 AMOLED 控制器
    bus = new Arduino_ESP32QSPI(LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3, true);
    panel = new Arduino_CO5300(bus, LCD_RESET, 0, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);
  }

  void powerOnHardware() {
    Wire.begin(IIC_SDA, IIC_SCL, 400000);

    // 1. 配置 M5PM1 电源芯片：开启屏幕供电
    stopwatchI2CWrite(M5PM1_ADDR, 0x09, 0x00); // 禁用 I2C 空闲休眠
    stopwatchI2CWrite(M5PM1_ADDR, 0x0A, 0x00); // 禁用看门狗
    uint8_t pwr = stopwatchI2CRead(M5PM1_ADDR, 0x06);
    stopwatchI2CWrite(M5PM1_ADDR, 0x06, pwr | 0x17); // 开启 3.3V_LDO_EN, 3.3V_DCDC_EN, CHG_EN
    delay(20);

    // 2. 配置 M5IOE1 IO扩展芯片：使能并复位 OLED 及触摸
    stopwatchI2CWrite(M5IOE1_ADDR, 0x23, 0x00); // 禁用休眠
    // 设置 IO1(MUX_CTR), IO3(AUDIO_EN), IO4(TP_RST), IO5(OLED_RST), IO8(L3B_EN) 为推挽输出
    uint8_t drv = stopwatchI2CRead(M5IOE1_ADDR, 0x13);
    stopwatchI2CWrite(M5IOE1_ADDR, 0x13, drv & ~0b10011101);
    uint8_t mode = stopwatchI2CRead(M5IOE1_ADDR, 0x03);
    stopwatchI2CWrite(M5IOE1_ADDR, 0x03, mode | 0b10011101);

    // 拉高各外设供电与复位
    uint8_t out = stopwatchI2CRead(M5IOE1_ADDR, 0x05);
    stopwatchI2CWrite(M5IOE1_ADDR, 0x05, out | 0b10011001);
    delay(10);

    // OLED + TP 复位序列 (拉低 IO4, IO5 后重新拉高)
    stopwatchI2CWrite(M5IOE1_ADDR, 0x05, (out | 0b10011001) & ~0b00011000);
    delay(15);
    stopwatchI2CWrite(M5IOE1_ADDR, 0x05, out | 0b10011001);
    delay(20);

    // 3. 配置 OLED 片选引脚
    pinMode(LCD_CS, OUTPUT);
    digitalWrite(LCD_CS, HIGH);
    Serial.println("[HAL] M5StopWatch power-on and OLED reset sequence complete.");
  }

  void init(int speedMHz = 40) {
    powerOnHardware();
    if (panel) {
      if (!panel->begin(speedMHz * 1000000)) {
        Serial.println("[HAL] panel->begin() failed!");
      } else {
        Serial.println("[HAL] CO5300 AMOLED panel initialized successfully!");
      }
      setBrightness(200);
    }
  }

  void setBrightness(uint8_t value) {
    if (panel) {
      panel->setBrightness(value);
    }
  }
};
