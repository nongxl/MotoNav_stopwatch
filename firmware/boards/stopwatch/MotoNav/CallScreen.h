#pragma once
#include <Arduino.h>

void drawHandset(float cx, float cy, float r, uint16_t c) {
  const float ROT = -35 * PI / 180, cr = cosf(ROT), sr = sinf(ROT);
  auto dot = [&](float x, float y, float rr) {
    aaDisc(cx + x * cr - y * sr, cy + x * sr + y * cr, rr, c);
  };
  for (int a = 25; a <= 155; a += 5) {
    float rad = a * PI / 180;
    dot(cosf(rad) * r, -sinf(rad) * r * 0.55f + r * 0.25f, r * 0.22f);
  }
  for (int k = 0; k < 2; k++) {
    float rad = (k ? 155 : 25) * PI / 180;
    dot(cosf(rad) * r + (k ? -1 : 1) * r * 0.04f, -sinf(rad) * r * 0.55f + r * 0.25f + r * 0.21f, r * 0.32f);
  }
}

void drawCallScreen() {
  onWaitScreen = false;
  frameIsMap = false;
  gfx->fillScreen(C_BLACK);
  unsigned long t = millis() - callAt;
  const float icy = 168;
  // 往外扩散的两圈波纹
  for (int k = 0; k < 2; k++) {
    float ph = fmodf(t / 1400.0f + k * 0.5f, 1.0f);
    float r = 52 + ph * 110;
    uint16_t c = blend565(C_GREEN, C_BLACK, (int)(ph * 255));
    for (int w = 0; w < 3; w++) gfx->drawCircle(CX, (int)icy, (int)r + w, c);
  }
  aaDisc(CX, icy, 50, 0x2DC6);
  aaDisc(CX, icy, 46, C_GREEN);
  // 听筒轻轻左右晃
  float wob = ((t / 120) % 4 < 2) ? -2.5f : 2.5f;
  drawHandset(CX + wob, icy, 26, C_WHITE);
  printCentered("INCOMING CALL", CX, 58, 2, C_GREEN);
  bool hasName = imgs[2].show && imgs[2].showW > 0;
  if (hasName) {
    drawTextImg(2, 252, 0xFF);
    if (callNum.length()) printCentered(callNum, CX, 318, 2, C_GREY);
  } else if (callNum.length()) {
    printCentered(callNum, CX, 262, callNum.length() > 12 ? 3 : 4, C_WHITE);
  } else {
    printCentered("UNKNOWN", CX, 262, 4, C_WHITE);
  }
  // v5.2 触屏接听提示；按住时绿圈外面一圈白点从正上方顺时针走满
  if (touchOK) {
    if (callAnswerSent) printCentered("ANSWERING...", CX, 392, 2, C_GREEN);
    else printCentered("TAP & HOLD", CX, 392, 2, C_GREY);      // v6.9
    if (touchHold > 0 && !callAnswerSent) {
      int steps = (int)(touchHold * 72);
      for (int k = 0; k <= steps; k++) {
        float a = k * 5 * PI / 180;
        aaDisc(CX + sinf(a) * 64, icy - cosf(a) * 64, 4, C_WHITE);
      }
    }
  }
  needFlush = true;
}

