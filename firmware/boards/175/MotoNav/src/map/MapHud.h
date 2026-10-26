#pragma once
// 地图上的车速、车道和行程信息。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// ---- v2.7 上下弧形面板：和黑色外圈连成一体 ----
#define CAP_R      213      // 面板填到这个半径（和外圈黑边接上）
#define TOPCAP_Y   98       // 上面板下沿（有测速时 112）
#define BOTCAP_Y   368      // 下面板上沿（离路口 150 米内 334）
#define BOTCAP_Y2  334

// 填充圆内 y0~y1 行（圆的一个弓形）
void fillCapRows(int y0, int y1, uint16_t c) {
  for (int y = max(0, y0); y <= min(LCD_HEIGHT - 1, y1); y++) {
    float dy = y + 0.5f - CX;
    if (fabsf(dy) >= CAP_R) continue;
    float h = sqrtf((float)CAP_R * CAP_R - dy * dy);
    int xa = max(0, (int)(CX - h)), xb = min(LCD_WIDTH - 1, (int)(CX + h));
    gfx->drawFastHLine(xa, y, xb - xa + 1, c);
  }
}

// 面板和地图之间的分隔线（只画到外圈黑边以内）
void capLine(int y, int thick, uint16_t c) {
  for (int k = 0; k < thick; k++) {
    float dy = y + k + 0.5f - CX;
    if (fabsf(dy) >= 211) continue;
    float h = sqrtf(211.0f * 211.0f - dy * dy);
    gfx->drawFastHLine((int)(CX - h), y + k, (int)(2 * h), c);
  }
}

// v2.8 带黑色描边的文字（直接浮在地图上）
void printOutlined(const String &t, int x, int y, int size, uint16_t col) {
  int o = size >= 3 ? 2 : 1;
  // v5.8 只画最外一圈偏移（字的笔画至少 o 像素粗，里面那圈会被盖住，画了也白画）
  for (int dy = -o; dy <= o; dy++)
    for (int dx = -o; dx <= o; dx++)
      if ((dx || dy) && (o < 2 || abs(dx) == o || abs(dy) == o)) printCentered(t, x + dx, y + dy, size, C_BLACK);
  printCentered(t, x, y, size, col);
}

// 把一块区域调暗（半透明深色底），a = 保留多少原来的亮度（0~255）
void dimRect(int x, int y, int w, int h, int r, int keep) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  for (int j = max(0, y); j < min(LCD_HEIGHT, y + h); j++) {
    for (int i = max(0, x); i < min(LCD_WIDTH, x + w); i++) {
      int dx = i < x + r ? x + r - i : (i >= x + w - r ? i - (x + w - r - 1) : 0);
      int dy = j < y + r ? y + r - j : (j >= y + h - r ? j - (y + h - r - 1) : 0);
      if (dx * dx + dy * dy > r * r) continue;
      uint16_t &p = fb[j * LCD_WIDTH + i];
      p = blend565(C_BLACK, p, keep);
    }
  }
}

// 车速（浮在顶部）+ 测速牌
void drawMapSpeed(bool over) {
  if (speedKmh < 0) return;
  int sx = hasCam ? CX - 40 : CX;
  printOutlined(String(speedKmh), sx, 44, 4, over ? C_RED : TH.hud);
  printOutlined("km/h", sx, 80, 1, over ? 0xFC10 : TH.hud);
  if (hasCam) {
    const int bx = CX + 52, by = 60;
    aaDisc(bx, by, 27, C_BLACK);
    aaDisc(bx, by, 25, C_SIGNRED);
    aaDisc(bx, by, 19, C_WHITE);
    String lim = String(camLimit);
    int sz = lim.length() >= 3 ? 2 : 3;
    printCentered(lim, bx, by - 4 * sz, sz, C_BLACK);
    printOutlined(distText(camDist), bx, by + 31, 2, over ? 0xFC10 : C_WHITE);
  }
}

// ---- v3.1 红绿灯小图标：竖着的黑色灯箱 + 红黄绿三个灯 ----
void drawTrafficLight(float cx, float cy, float h) {
  float w = h * 0.46f, r = h * 0.13f;
  gfx->fillRoundRect((int)(cx - w / 2 - 1), (int)(cy - h / 2 - 1), (int)w + 2, (int)h + 2, (int)(w / 2.6f), C_BLACK);
  gfx->fillRoundRect((int)(cx - w / 2), (int)(cy - h / 2), (int)w, (int)h, (int)(w / 3), 0x2104);
  aaDisc(cx, cy - h * 0.29f, r, 0xF8A2);     // 红
  aaDisc(cx, cy, r, 0xFDA0);                // 黄
  aaDisc(cx, cy + h * 0.29f, r, 0x3F09);    // 绿
}

// v3.1 底部一行行程信息：到达时间 · 剩余里程 · 红绿灯数（半透明深色底）
void drawTripLine(int y) {
  char eta[8];
  snprintf(eta, sizeof(eta), "%02d:%02d", (tripEta / 100) % 24, tripEta % 100);
  String a = String(eta);
  String b = tripRemainM >= 100000 ? String(tripRemainM / 1000) + "km" : distText(tripRemainM);
  String c = String(tripLights);
  const int CW = 12, GAP = 14, ICON = 10;
  bool lights = tripLights > 0;
  int w = (a.length() + b.length()) * CW + GAP + (lights ? GAP + ICON + 3 + c.length() * CW : 0);
  int x = CX - w / 2;
  dimRect(x - 10, y - 5, w + 20, 26, 12, 75);
  printCentered(a, x + a.length() * CW / 2, y, 2, TH.hud);
  x += a.length() * CW + GAP;
  printCentered(b, x + b.length() * CW / 2, y, 2, C_WHITE);
  x += b.length() * CW + GAP;
  if (lights) {
    drawTrafficLight(x + ICON / 2, y + 7, 18);
    x += ICON + 3;
    printCentered(c, x + c.length() * CW / 2, y, 2, C_WHITE);
  }
}

// 车道跟随转弯倒计时显示（当前从 500 米开始）；有测速牌时再往下让一点。
void drawMapLanes() {
  const bool visible = laneN > 0 && ringActive;
#if FPS_SERIAL_DIAGNOSTICS
  static int previous = -1;
  const int state = visible ? laneN : 0;
  if (state != previous) {
    Serial.printf("LANE HUD visible=%d lanes=%d ring=%d metres=%d\n", visible, laneN, ringActive, navDist);
    previous = state;
  }
#endif
  if (!visible) return;
  const int H = 30, MAXW = 210;
  int lw = (int)(fminf(36.0f * H / 32, (float)MAXW / laneN) * laneN);
  int y = hasCam ? 114 : 100;
  dimRect(CX - lw / 2 - 9, y - 5, lw + 18, H + 10, 11, 70);
  drawLanesAt(CX, y, MAXW, H);
}
