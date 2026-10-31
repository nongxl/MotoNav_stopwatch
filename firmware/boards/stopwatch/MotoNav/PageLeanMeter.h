#pragma once
#include <Arduino.h>
#include "LeanAngleMeter.h"

extern bool dragging;

static inline uint16_t blend565(uint16_t fg, uint16_t bg, uint8_t alpha) {
  uint32_t r1 = (fg >> 11) & 0x1F, g1 = (fg >> 5) & 0x3F, b1 = fg & 0x1F;
  uint32_t r2 = (bg >> 11) & 0x1F, g2 = (bg >> 5) & 0x3F, b2 = bg & 0x1F;
  uint32_t r = (r1 * alpha + r2 * (255 - alpha)) >> 8;
  uint32_t g = (g1 * alpha + g2 * (255 - alpha)) >> 8;
  uint32_t b = (b1 * alpha + b2 * (255 - alpha)) >> 8;
  return (uint16_t)((r << 11) | (g << 5) | b);
}

extern int themeIdx;
extern bool nightOn;
#ifndef ST_PAPER
#define ST_PAPER 4
#endif
#ifndef ST_NEON
#define ST_NEON 3
#endif

void drawLeanMeter() {
  onWaitScreen = false;
  frameIsMap = false;
  gfx->fillScreen(C_BLACK);

  int curMapStyle = (nightOn && themeIdx == ST_PAPER) ? ST_NEON : themeIdx;
  LeanTheme t = LeanTheme::get(curMapStyle);

  // 1. 顶部状态栏：仅保留左侧微型蓝牙指示点
  aaDisc(CX - 150, 28, 5, bleConnected ? C_GREEN : C_GREY);

  const int CY = CX; // 466x466 圆形屏中心 (233, 233)
  const float CX_DIAL = (float)CX;
  const float CY_DIAL = (float)CY;

  // ==========================================
  // 2. 外层环形：倾角扇形与加粗加大刻度盘 (R: 155 ~ 208)
  // ==========================================
  const float R_OUT = 206.0f;
  const float R_IN  = 154.0f;

  float curLean = LeanAngleMeter::leanAngle;
  float absCurLean = fabsf(curLean);

  // 扇形实心连续色彩填充 (微四边形网格光栅化，颜色自适应当前主题)
  if (absCurLean >= 1.0f) {
    float a1 = 270.0f;
    float a2 = 270.0f + curLean;
    if (a1 > a2) { float tmp = a1; a1 = a2; a2 = tmp; }

    uint16_t sectorColor;
    if (absCurLean <= 35.0f) sectorColor = t.safeZone;
    else if (absCurLean <= 50.0f) sectorColor = t.warnZone;
    else sectorColor = t.alertZone;

    const float STEP = 0.5f;
    for (float a = a1; a < a2; a += STEP) {
      float nextA = a + STEP;
      if (nextA > a2) nextA = a2;

      float rad1 = a * (float)M_PI / 180.0f;
      float rad2 = nextA * (float)M_PI / 180.0f;

      float cos1 = cosf(rad1), sin1 = sinf(rad1);
      float cos2 = cosf(rad2), sin2 = sinf(rad2);

      int x_in1  = (int)lroundf(CX_DIAL + R_IN * cos1);
      int y_in1  = (int)lroundf(CY_DIAL + R_IN * sin1);
      int x_out1 = (int)lroundf(CX_DIAL + R_OUT * cos1);
      int y_out1 = (int)lroundf(CY_DIAL + R_OUT * sin1);

      int x_in2  = (int)lroundf(CX_DIAL + R_IN * cos2);
      int y_in2  = (int)lroundf(CY_DIAL + R_IN * sin2);
      int x_out2 = (int)lroundf(CX_DIAL + R_OUT * cos2);
      int y_out2 = (int)lroundf(CY_DIAL + R_OUT * sin2);

      gfx->fillTriangle(x_in1, y_in1, x_out1, y_out1, x_out2, y_out2, sectorColor);
      gfx->fillTriangle(x_in1, y_in1, x_out2, y_out2, x_in2, y_in2, sectorColor);
    }

    // 扇面当前倾角边缘加粗边界指针 (3 像素宽纯白高亮线)
    float curRad = (270.0f + curLean) * (float)M_PI / 180.0f;
    float cCos = cosf(curRad), cSin = sinf(curRad);
    int px1 = (int)lroundf(CX_DIAL + (R_IN - 4) * cCos), py1 = (int)lroundf(CY_DIAL + (R_IN - 4) * cSin);
    int px2 = (int)lroundf(CX_DIAL + (R_OUT + 4) * cCos), py2 = (int)lroundf(CY_DIAL + (R_OUT + 4) * cSin);
    gfx->drawLine(px1, py1, px2, py2, C_WHITE);
    gfx->drawLine(px1 + 1, py1, px2 + 1, py2, C_WHITE);
    gfx->drawLine(px1 - 1, py1, px2 - 1, py2, C_WHITE);
  }

  // 覆盖加粗加大放射状刻度盘 (-60° 到 +60°，自适应主题色)
  for (int a = -60; a <= 60; a += 2) {
    float rad = (270.0f + a) * (float)M_PI / 180.0f;
    float cosA = cosf(rad), sinA = sinf(rad);
    int absA = abs(a);

    float len = (absA % 30 == 0) ? 26.0f : ((absA % 10 == 0) ? 16.0f : ((absA % 5 == 0) ? 8.0f : 0.0f));
    if (len == 0.0f) continue;

    uint16_t c;
    if (absA == 0) {
      c = C_WHITE;
    } else if (absA <= 35) {
      c = (absA % 30 == 0) ? t.safeZone : ((absA % 10 == 0) ? blend565(t.safeZone, C_BLACK, 180) : blend565(t.safeZone, C_BLACK, 100));
    } else if (absA <= 50) {
      c = (absA % 10 == 0) ? t.warnZone : blend565(t.warnZone, C_BLACK, 150);
    } else {
      c = (absA % 10 == 0) ? t.alertZone : blend565(t.alertZone, C_BLACK, 150);
    }

    int x1 = (int)lroundf(CX_DIAL + (R_OUT - len) * cosA);
    int y1 = (int)lroundf(CY_DIAL + (R_OUT - len) * sinA);
    int x2 = (int)lroundf(CX_DIAL + R_OUT * cosA);
    int y2 = (int)lroundf(CY_DIAL + R_OUT * sinA);
    gfx->drawLine(x1, y1, x2, y2, c);

    if (absA % 30 == 0) {
      gfx->drawLine(x1 + 1, y1, x2 + 1, y2, c);
      gfx->drawLine(x1 - 1, y1, x2 - 1, y2, c);
    } else if (absA % 10 == 0) {
      gfx->drawLine(x1 + 1, y1, x2 + 1, y2, c);
    }
  }

  // 0° 正上方中线强化 (5 像素宽纯白)
  gfx->drawLine((int)CX_DIAL - 2, (int)(CY_DIAL - R_OUT), (int)CX_DIAL - 2, (int)(CY_DIAL - R_OUT + 28), C_WHITE);
  gfx->drawLine((int)CX_DIAL - 1, (int)(CY_DIAL - R_OUT), (int)CX_DIAL - 1, (int)(CY_DIAL - R_OUT + 28), C_WHITE);
  gfx->drawLine((int)CX_DIAL,     (int)(CY_DIAL - R_OUT), (int)CX_DIAL,     (int)(CY_DIAL - R_OUT + 28), C_WHITE);
  gfx->drawLine((int)CX_DIAL + 1, (int)(CY_DIAL - R_OUT), (int)CX_DIAL + 1, (int)(CY_DIAL - R_OUT + 28), C_WHITE);
  gfx->drawLine((int)CX_DIAL + 2, (int)(CY_DIAL - R_OUT), (int)CX_DIAL + 2, (int)(CY_DIAL - R_OUT + 28), C_WHITE);

  // 刻度值数字 (统一规范 size 2，清晰不遮挡且符合原项目字体风格)
  printCentered("0", (int)CX_DIAL, (int)(CY_DIAL - R_OUT - 14), 2, C_WHITE);
  printCentered("30", (int)(CX_DIAL + (R_OUT + 15) * cosf(300.0f * (float)M_PI / 180.0f)),
                      (int)(CY_DIAL + (R_OUT + 15) * sinf(300.0f * (float)M_PI / 180.0f)), 2, t.safeZone);
  printCentered("-30", (int)(CX_DIAL + (R_OUT + 16) * cosf(240.0f * (float)M_PI / 180.0f)),
                       (int)(CY_DIAL + (R_OUT + 16) * sinf(240.0f * (float)M_PI / 180.0f)), 2, t.safeZone);
  printCentered("60", (int)(CX_DIAL + (R_OUT + 15) * cosf(330.0f * (float)M_PI / 180.0f)),
                      (int)(CY_DIAL + (R_OUT + 15) * sinf(330.0f * (float)M_PI / 180.0f)), 2, t.alertZone);
  printCentered("-60", (int)(CX_DIAL + (R_OUT + 16) * cosf(210.0f * (float)M_PI / 180.0f)),
                       (int)(CY_DIAL + (R_OUT + 16) * sinf(210.0f * (float)M_PI / 180.0f)), 2, t.alertZone);

  // ==========================================
  // 3. 准备实时倾角与动态颜色
  // ==========================================
  char leanStr[16];
  if (absCurLean < 0.5f) {
    snprintf(leanStr, sizeof(leanStr), "0*");
  } else if (curLean < 0) {
    snprintf(leanStr, sizeof(leanStr), "-%d*", (int)lroundf(absCurLean));
  } else {
    snprintf(leanStr, sizeof(leanStr), "+%d*", (int)lroundf(absCurLean));
  }

  uint16_t leanColor = (absCurLean < 0.5f) ? C_WHITE :
                       ((absCurLean <= 35.0f) ? t.safeZone :
                       ((absCurLean <= 50.0f) ? t.warnZone : t.alertZone));

  // ==========================================
  // 4. G 值表圆圈与赛车多层同心圆标尺 (0.5G / 1.0G / 1.5G)
  // ==========================================
  const int GR = 104; // 104px 半径，居中协调
  const float MAX_G = 1.5f;

  // 赛车标准多层同心刻度环
  gfx->drawCircle(CX, CY, 35,  t.radarRing1); // 0.5G 细刻度环
  gfx->drawCircle(CX, CY, 70,  t.radarRing2); // 1.0G 标准临界刻度环
  gfx->drawCircle(CX, CY, GR,  t.radarRing3); // 1.5G 极限抓地边界环

  // 十字准星 (中心留空 8px)
  gfx->drawLine(CX - GR, CY, CX - 8, CY, t.crosshair);
  gfx->drawLine(CX + 8, CY, CX + GR, CY, t.crosshair);
  gfx->drawLine(CX, CY - GR, CX, CY - 8, t.crosshair);
  gfx->drawLine(CX, CY + 8, CX, CY + GR, t.crosshair);

  // 十字端点微小标注
  printCentered("^ ACC", CX, CY - GR + 10, 1, t.labelColor);
  printCentered("v BRK", CX, CY + GR - 12, 1, t.labelColor);

  // 动态大光圈半透明小球坐标
  const float MOVE_R = (float)(GR - 13);
  float normLong = LeanAngleMeter::gLongitudinal / MAX_G;
  float normLat  = LeanAngleMeter::gLateral / MAX_G;
  float dx = normLat * MOVE_R;
  float dy = -normLong * MOVE_R; // 加速向上(-)，刹车向下(+)

  float dist = sqrtf(dx * dx + dy * dy);
  if (dist > MOVE_R) {
    dx = (dx / dist) * MOVE_R;
    dy = (dy / dist) * MOVE_R;
  }

  int bx = (int)lroundf(CX_DIAL + dx);
  int by = (int)lroundf(CY_DIAL + dy);

  // 绘制半透明移动圆圈 (Alpha Blending, 透出背后的同心圆与十字准星)
  const int BALL_R = 12;
  for (int py = by - BALL_R; py <= by + BALL_R; py++) {
    for (int px = bx - BALL_R; px <= bx + BALL_R; px++) {
      int d2 = (px - bx) * (px - bx) + (py - by) * (py - by);
      if (d2 <= BALL_R * BALL_R) {
        uint16_t cBg = C_BLACK;
        if ((px == CX && abs(py - CY) <= GR && abs(py - CY) > 8) ||
            (py == CY && abs(px - CX) <= GR && abs(px - CX) > 8)) {
          cBg = t.crosshair;
        } else {
          int dC = (int)lroundf(sqrtf((float)((px - CX)*(px - CX) + (py - CY)*(py - CY))));
          if (dC == 70) cBg = t.radarRing2;
          else if (dC == 35) cBg = t.radarRing1;
          else if (dC == GR) cBg = t.radarRing3;
        }

        if (d2 <= 3) {
          gfx->drawPixel(px, py, C_WHITE); // 中心亮点
        } else if (d2 >= (BALL_R - 2) * (BALL_R - 2)) {
          gfx->drawPixel(px, py, blend565(t.gBallGlow, cBg, 220)); // 主题光晕外环
        } else {
          gfx->drawPixel(px, py, blend565(t.gBallGlow, cBg, 90));   // 半透明内部
        }
      }
    }
  }

  // 实时综合 G 值读数 (圆心下方，平滑滤波，主题高光)
  static float dispG = 0.0f;
  float targetG = LeanAngleMeter::gTotal;
  if (fabsf(targetG - dispG) > 0.015f) {
    dispG += 0.10f * (targetG - dispG);
  }
  if (dispG < 0.02f) dispG = 0.0f;

  char gNumBuf[16];
  snprintf(gNumBuf, sizeof(gNumBuf), "%.2f G", dispG);
  printCentered(gNumBuf, CX, CY + 52, 3, t.accent);

  // ==========================================
  // 5. 屏幕两侧胶囊：MAX L 与 MAX R (严格几何镜像对称)
  // ==========================================
  const int SIDE_W = 86;
  const int SIDE_H = 40;
  const int SIDE_OFFSET = 169; // 距中心左右对称距离
  const int SIDE_L_X = CX - SIDE_OFFSET - SIDE_W / 2;
  const int SIDE_R_X = CX + SIDE_OFFSET - SIDE_W / 2;
  const int SIDE_Y   = CY - SIDE_H / 2;

  // 左侧胶囊: MAX L
  gfx->fillRoundRect(SIDE_L_X, SIDE_Y, SIDE_W, SIDE_H, 14, t.pillBg);
  gfx->drawRoundRect(SIDE_L_X, SIDE_Y, SIDE_W, SIDE_H, 14, t.pillBorder);

  char maxLStr[16];
  snprintf(maxLStr, sizeof(maxLStr), "-%d*", (int)lroundf(LeanAngleMeter::maxLeftLean));
  printCentered("MAX L", SIDE_L_X + SIDE_W / 2, SIDE_Y + 7, 1, t.labelColor);
  printCentered(maxLStr, SIDE_L_X + SIDE_W / 2, SIDE_Y + 21, 2, C_WHITE);

  // 右侧胶囊: MAX R
  gfx->fillRoundRect(SIDE_R_X, SIDE_Y, SIDE_W, SIDE_H, 14, t.pillBg);
  gfx->drawRoundRect(SIDE_R_X, SIDE_Y, SIDE_W, SIDE_H, 14, t.pillBorder);

  char maxRStr[16];
  snprintf(maxRStr, sizeof(maxRStr), "+%d*", (int)lroundf(LeanAngleMeter::maxRightLean));
  printCentered("MAX R", SIDE_R_X + SIDE_W / 2, SIDE_Y + 7, 1, t.labelColor);
  printCentered(maxRStr, SIDE_R_X + SIDE_W / 2, SIDE_Y + 21, 2, C_WHITE);

  // ==========================================
  // 6. 底部第 1 行: MAX G 胶囊卡片 (Y = 346)
  // ==========================================
  const int CAP1_Y = 346;
  const int CAP1_W = 180;
  const int CAP1_H = 28;
  const int CAP1_X = CX - CAP1_W / 2;

  bool justCalib = millis() < LeanAngleMeter::calibSuccessUntil;
  if (justCalib) {
    gfx->fillRoundRect(CAP1_X, CAP1_Y, CAP1_W, CAP1_H, 14, C_GREEN);
    printCentered("CALIBRATED OK!", CX, CAP1_Y + 7, 2, C_BLACK);
  } else {
    gfx->fillRoundRect(CAP1_X, CAP1_Y, CAP1_W, CAP1_H, 14, t.pillBg);
    gfx->drawRoundRect(CAP1_X, CAP1_Y, CAP1_W, CAP1_H, 14, t.pillBorder);

    char maxGBuf[16];
    snprintf(maxGBuf, sizeof(maxGBuf), "%.2f G", LeanAngleMeter::maxG);
    printCentered("MAX G", CX - 44, CAP1_Y + 9, 1, t.labelColor);
    printCentered(maxGBuf, CX + 30, CAP1_Y + 7, 2, t.accent);
  }

  // ==========================================
  // 7. 底部第 2 行: 实时倾角数字胶囊卡片 (Y = 382)
  // ==========================================
  const int CAP2_Y = 382;
  const int CAP2_W = 180;
  const int CAP2_H = 28;
  const int CAP2_X = CX - CAP2_W / 2;

  gfx->fillRoundRect(CAP2_X, CAP2_Y, CAP2_W, CAP2_H, 14, t.pillBg);
  gfx->drawRoundRect(CAP2_X, CAP2_Y, CAP2_W, CAP2_H, 14, t.pillBorder);

  printCentered("LEAN", CX - 44, CAP2_Y + 9, 1, t.labelColor);
  printCentered(leanStr, CX + 30, CAP2_Y + 5, 3, leanColor);

  // 零位校准提示 (最底部低调微字)
  printCentered("HOLD 2S TO CALIBRATE", CX, 418, 1, C_DIM);

  // 8. 底部页面圆点
  drawPageDots();
  needFlush = true;
}

void leanMeterTap(int x, int y) {
  if (dragging || page != 3 || callOn || pairOn) return;
  float dx = x - CX, dy = y - CX;
  touchVectorToCanvas(dx, dy);
  x = (int)lroundf(CX + dx); y = (int)lroundf(CX + dy);

  static unsigned long lastTapAt = 0;
  unsigned long now = millis();
  bool isDouble = (now - lastTapAt < 400);
  lastTapAt = now;

  // 双击屏幕重置极值统计 (单击不再触发校准，校准只能通过长按底部按钮 1.5 秒触发)
  if (isDouble) {
    LeanAngleMeter::resetStats();
    leanDirty = true;
  }
