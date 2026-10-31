#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <time.h>
#include <sys/time.h>

// Epson RX8130CE I2C RTC Driver for MotoNav Stopwatch
// I2C 7-bit Address: 0x32
#define RX8130_I2C_ADDR   0x32

#define RX8130_REG_SEC    0x10
#define RX8130_REG_MIN    0x11
#define RX8130_REG_HOUR   0x12
#define RX8130_REG_WDAY   0x13
#define RX8130_REG_MDAY   0x14
#define RX8130_REG_MONTH  0x15
#define RX8130_REG_YEAR   0x16

#define RX8130_REG_FLAG   0x1D
#define RX8130_REG_CTRL0  0x1E
#define RX8130_REG_CTRL1  0x1F

#define RX8130_FLAG_VLF   (1 << 1)
#define RX8130_CTRL0_STOP (1 << 6)

class RtcRX8130 {
private:
  static uint8_t bcd2dec(uint8_t val) {
    return (val >> 4) * 10 + (val & 0x0F);
  }

  static uint8_t dec2bcd(uint8_t val) {
    return ((val / 10) << 4) + (val % 10);
  }

  static bool readRegs(uint8_t reg, uint8_t *buf, size_t len) {
    Wire.beginTransmission(RX8130_I2C_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    size_t r = Wire.requestFrom((uint8_t)RX8130_I2C_ADDR, (uint8_t)len);
    if (r != len) return false;
    for (size_t i = 0; i < len; i++) {
      buf[i] = Wire.read();
    }
    return true;
  }

  static bool writeReg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(RX8130_I2C_ADDR);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
  }

  static bool writeRegs(uint8_t reg, const uint8_t *buf, size_t len) {
    Wire.beginTransmission(RX8130_I2C_ADDR);
    Wire.write(reg);
    for (size_t i = 0; i < len; i++) {
      Wire.write(buf[i]);
    }
    return Wire.endTransmission() == 0;
  }

public:
  static bool available;

  static bool begin() {
    Wire.beginTransmission(RX8130_I2C_ADDR);
    if (Wire.endTransmission() != 0) {
      Serial.println("[RTC] RX8130CE not detected on I2C");
      available = false;
      return false;
    }
    available = true;

    // 使能备用电池/法拉电容充电 (CTRL1 寄存器置 bit 4, bit 5)
    uint8_t ctrl1 = 0;
    if (readRegs(RX8130_REG_CTRL1, &ctrl1, 1)) {
      ctrl1 |= 0x30; // 0b00110000 开启备用电源充电
      writeReg(RX8130_REG_CTRL1, ctrl1);
    }

    // 检查并清除中断/标志位
    uint8_t flag = 0;
    if (readRegs(RX8130_REG_FLAG, &flag, 1)) {
      if (flag & RX8130_FLAG_VLF) {
        Serial.println("[RTC] RX8130 VLF set (power lost previously)");
        // 清除 VLF
        writeReg(RX8130_REG_FLAG, flag & ~RX8130_FLAG_VLF);
      }
    }

    Serial.println("[RTC] RX8130CE initialized successfully");
    return true;
  }

  static bool getTime(struct tm *t) {
    if (!available || !t) return false;
    uint8_t raw[7] = {0};
    if (!readRegs(RX8130_REG_SEC, raw, 7)) return false;

    t->tm_sec  = bcd2dec(raw[0] & 0x7F);
    t->tm_min  = bcd2dec(raw[1] & 0x7F);
    t->tm_hour = bcd2dec(raw[2] & 0x3F);
    t->tm_wday = bcd2dec(raw[3] & 0x07);
    t->tm_mday = bcd2dec(raw[4] & 0x3F);
    int mon    = bcd2dec(raw[5] & 0x1F);
    t->tm_mon  = (mon >= 1 && mon <= 12) ? mon - 1 : 0;
    t->tm_year = bcd2dec(raw[6]) + 100; // 2000 年后偏移 (2000-1900=100)

    // 基础合法性检验：年份大于 2024
    if (t->tm_year < 124 || t->tm_mon > 11 || t->tm_mday < 1 || t->tm_mday > 31 ||
        t->tm_hour > 23 || t->tm_min > 59 || t->tm_sec > 59) {
      return false;
    }
    return true;
  }

  static bool setTime(const struct tm *t) {
    if (!available || !t) return false;

    // 写时钟前置 STOP 位
    uint8_t ctrl0 = 0;
    readRegs(RX8130_REG_CTRL0, &ctrl0, 1);
    writeReg(RX8130_REG_CTRL0, ctrl0 | RX8130_CTRL0_STOP);

    uint8_t raw[7];
    raw[0] = dec2bcd(t->tm_sec);
    raw[1] = dec2bcd(t->tm_min);
    raw[2] = dec2bcd(t->tm_hour);
    raw[3] = dec2bcd(t->tm_wday);
    raw[4] = dec2bcd(t->tm_mday);
    raw[5] = dec2bcd((t->tm_mon + 1) & 0x1F);
    raw[6] = dec2bcd((t->tm_year >= 100 ? t->tm_year - 100 : t->tm_year) % 100);

    bool ok = writeRegs(RX8130_REG_SEC, raw, 7);

    // 清除 STOP 位继续计时
    writeReg(RX8130_REG_CTRL0, ctrl0 & ~RX8130_CTRL0_STOP);

    if (ok) {
      Serial.printf("[RTC] Set RX8130 time: %04d-%02d-%02d %02d:%02d:%02d\n",
                    t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
                    t->tm_hour, t->tm_min, t->tm_sec);
    }
    return ok;
  }

  // 同步 RTC 时间到 ESP32 系统时间 (用于开机离线恢复)
  static bool syncToSystemTime() {
    struct tm rtcTm;
    if (!getTime(&rtcTm)) return false;

    time_t epoch = mktime(&rtcTm);
    if (epoch < 0) return false;

    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    Serial.printf("[RTC] System time restored from RX8130: epoch=%ld\n", (long)epoch);
    return true;
  }

  // 同步当前系统时间到 RTC (在收到手机时间更新时调用)
  static bool syncFromSystemTime() {
    time_t now = time(NULL);
    if (now < 1700000000) return false; // 必须是合法现实时间
    struct tm *local = localtime(&now);
    return setTime(local);
  }
};

bool RtcRX8130::available = false;
