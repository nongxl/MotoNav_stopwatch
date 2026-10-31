#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "IdlePowerOff.h"

// 顶部小圆点：绿 = 蓝牙已连接，灰 = 未连接
// ================= v6.8 电池（AXP2101 电源芯片，和触摸屏共用 I2C，地址 0x34）=================
#define PMU_ADDR 0x34
bool pmuOK = false;
int batPct = -1;                       // -1 = 没接电池
bool batChg = false, usbIn = false;
int batMv = 0;
unsigned long batAt = 0, batSentAt = 0;
String pendingBat = "";

int pmuRead(uint8_t reg) {
  Wire.beginTransmission(PMU_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return -1;
  if (Wire.requestFrom((uint8_t)PMU_ADDR, (uint8_t)1) != 1) return -1;
  return Wire.read();
}
void pmuSetBit(uint8_t reg, uint8_t bit) {
  int v = pmuRead(reg);
  if (v < 0 || (v & (1 << bit))) return;
  Wire.beginTransmission(PMU_ADDR);
  Wire.write(reg);
  Wire.write((uint8_t)(v | (1 << bit)));
  Wire.endTransmission();
}

// 开机时调一次：只打开电量计、电池检测、电池电压测量，不动任何供电设置
void batInit() {
  // StopWatch：电源管理是 M5PM1（0x6E），已在 StopWatchBoard::init() 里完成初始化
  //（含关看门狗、使能充电、CHG_PROG/CHG_STAT 脚配置）。这里做一次首读确认电池在位。
  pmuOK = StopWatchBoard::pm1OK;
  if (!pmuOK) return;
  uint16_t mv = 0;
  if (StopWatchBoard::pm1.readVbat(&mv) != M5PM1_OK || mv < 2800) pmuOK = false;
}

// StopWatch: M5PM1 Vin > 4000mV indicates USB external power.
// Unknown/stale power readings keep the device awake, rather than guessing battery.
void idlePowerPollSupply(bool force = false) {
  const uint32_t now = millis();
  if (!force && idlePowerSupply.sampled &&
      (uint32_t)(now - idlePowerSupply.checkedAt) < 500) return;
  const bool wasBattery = idlePowerSupplyAllowsOff(&idlePowerSupply, now);
  uint16_t vin = 0;
  // If Vin read fails or pmu is not OK, status is -1 (unknown) so we stay awake safely.
  int status = -1;
  if (pmuOK && StopWatchBoard::pm1.readVin(&vin) == M5PM1_OK) {
    status = (vin > 4000) ? 0x20 : 0x00; // Bit 5 represents VBUS/external power
  }
  idlePowerSupplyUpdate(&idlePowerSupply, now, status);
  // Unplugging / recovering a valid battery-only reading starts a full interval.
  if (!wasBattery && idlePowerSupplyAllowsOff(&idlePowerSupply, now))
    idlePowerOffReset(&idlePowerOffTimer, now);
}

// 电池图标与内嵌电量：主体 52x24，右侧凸头 4x10，电量数字 setTextSize(2) 居中嵌在电池内部（更清晰饱满）
void drawBattery(int x0, int y0, bool backing = false) {
  if (batPct < 0) return;                                  // 没接电池不画
  const int BW = 52, BH = 24, capW = 4, capH = 10;
  const int totalW = BW + capW;

  if (backing) {                                           // 小地图上垫一块黑底
    gfx->fillRoundRect(x0 - 4, y0 - 3, totalW + 8, BH + 6, 6, C_BLACK);
  }

  uint16_t c = batChg ? C_CYAN : batPct > 50 ? C_GREEN : batPct >= 20 ? 0xFE60 : C_RED;
  if (!batChg && batPct < 10 && (millis() / 600) % 2) c = 0x4208;   // 快没电：闪烁

  // 1. 电池主体外框与正极凸头
  gfx->drawRoundRect(x0, y0, BW, BH, 4, c);
  gfx->drawRoundRect(x0 + 1, y0 + 1, BW - 2, BH - 2, 3, c);
  gfx->fillRoundRect(x0 + BW, y0 + (BH - capH) / 2, capW + 1, capH, 2, c);

  // 2. 内部底色（纯黑）
  gfx->fillRect(x0 + 2, y0 + 2, BW - 4, BH - 4, C_BLACK);

  // 3. 电量条填充
  int innerW = BW - 4; // 48
  int innerH = BH - 4; // 20
  int fw = innerW * constrain(batPct, 0, 100) / 100;
  if (fw > 0) {
    gfx->fillRect(x0 + 2, y0 + 2, fw, innerH, c);
  }

  // 4. 内嵌电量文字及充电标识（setTextSize(2) 放大一倍）
  String pctStr = String(constrain(batPct, 0, 100));
  int fillEdge = x0 + 2 + fw;
  int textW = (int)pctStr.length() * 12 - 2;
  int ty = y0 + 2 + (innerH - 14) / 2; // 14px 字符高度垂直居中

  if (batChg) {
    // 充电中：小闪电图标 + 放大数字
    int totalContentW = 8 + 3 + textW;
    int startX = x0 + 2 + (innerW - totalContentW) / 2;
    int boltX = startX + 3;
    int boltY = y0 + BH / 2;

    gfx->fillTriangle(boltX + 1, boltY - 6, boltX - 4, boltY, boltX, boltY, C_BLACK);
    gfx->fillTriangle(boltX - 1, boltY + 6, boltX + 4, boltY, boltX, boltY, C_BLACK);
    gfx->drawLine(boltX - 1, boltY - 3, boltX - 2, boltY + 3, C_WHITE);

    int tx = startX + 11;
    for (int i = 0; i < (int)pctStr.length(); i++) {
      int cx = tx + i * 12;
      bool inFill = (cx + 6 <= fillEdge);
      if (!inFill && cx < fillEdge + 8) {
        gfx->setTextSize(2);
        gfx->setTextColor(C_BLACK);
        gfx->setCursor(cx + 1, ty); gfx->print(pctStr[i]);
        gfx->setCursor(cx - 1, ty); gfx->print(pctStr[i]);
        gfx->setCursor(cx, ty + 1); gfx->print(pctStr[i]);
        gfx->setCursor(cx, ty - 1); gfx->print(pctStr[i]);
      }
      gfx->setTextSize(2);
      gfx->setTextColor(inFill ? C_BLACK : C_WHITE);
      gfx->setCursor(cx, ty);
      gfx->print(pctStr[i]);
    }
  } else {
    // 未充电：电量数字完全居中
    int tx = x0 + 2 + (innerW - textW) / 2;
    for (int i = 0; i < (int)pctStr.length(); i++) {
      int cx = tx + i * 12;
      bool inFill = (cx + 6 <= fillEdge);
      if (!inFill && cx < fillEdge + 8) {
        gfx->setTextSize(2);
        gfx->setTextColor(C_BLACK);
        gfx->setCursor(cx + 1, ty); gfx->print(pctStr[i]);
        gfx->setCursor(cx - 1, ty); gfx->print(pctStr[i]);
        gfx->setCursor(cx, ty + 1); gfx->print(pctStr[i]);
        gfx->setCursor(cx, ty - 1); gfx->print(pctStr[i]);
      }
      gfx->setTextSize(2);
      gfx->setTextColor(inFill ? C_BLACK : C_WHITE);
      gfx->setCursor(cx, ty);
      gfx->print(pctStr[i]);
    }
  }
}

void drawStatus() {
  gfx->fillRect(CX - 32, 18, 64, 30, C_BLACK);   // 擦除旧电池区域
  drawBattery(CX - 28, 20);                      // 放大电池图标并居中
  needFlush = true;
}


