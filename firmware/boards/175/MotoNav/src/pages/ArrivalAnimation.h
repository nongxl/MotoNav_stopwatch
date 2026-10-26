#pragma once
// 到达动画。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// ================= v7.1 到达特效 =================
// 到达目的地（"A,0"）：约 7 秒的庆祝动画——粉色放射光芒慢慢转、冲击波圆环、几轮彩色烟花、
// 中间一面飘动的黑白格终点旗、"ARRIVED" 霓虹字，手机发了行程统计（"#O米/分钟/最高车速"）就逐字打出来。
// 到达途经点（"#J"）：约 3 秒的短版（青色，"WAYPOINT"）。点一下屏幕提前结束，之后回到原来的画面。
#define C_PINK   0xF973   // #FF2E9A 罪恶都市粉
#define FX_DEST_MS 7000
#define FX_WP_MS   3000
#define FX_PN      180
#define CCY 233                // 屏幕中心 y（圆屏 466×466）
bool fxOn = false, fxWp = false;
unsigned long fxAt = 0, fxDur = 0;
bool touchEat = false;                 // 点掉特效的那一下：不当成滑动换页
long tripM = -1; int tripMin = -1, tripMaxKmh = -1; unsigned long tripAt = 0;   // "#O" 行程统计
struct FxP { int16_t x, y, vx, vy; uint16_t t0; uint16_t col; };
FxP fxP[FX_PN];
int fxPN = 0;

static inline float frand(float a, float b) { return a + (b - a) * (esp_random() & 0xFFFF) / 65535.0f; }

void startArriveFx(bool wp) {
  fxOn = true; fxWp = wp;
  fxAt = millis();
  fxDur = wp ? FX_WP_MS : FX_DEST_MS;
  // 烟花：几轮，每轮从一个点炸开
  const uint16_t pal[5] = { C_PINK, C_CYAN, C_YELLOW, 0x07F0, C_WHITE };
  const int burstsD[6] = { 0, 650, 1300, 2200, 3200, 4300 };
  const int burstsW[2] = { 0, 700 };
  int nb = wp ? 2 : 6;
  int per = FX_PN / nb;
  fxPN = 0;
  for (int b = 0; b < nb; b++) {
    float bx = CX + frand(-110, 110), by = (wp ? 200 : 150) + frand(-50, 50);
    uint16_t c1 = wp ? C_CYAN : pal[esp_random() % 5], c2 = wp ? C_WHITE : pal[esp_random() % 5];
    for (int i = 0; i < per && fxPN < FX_PN; i++) {
      float a = frand(0, 2 * PI), sp = frand(70, 230);
      FxP &p = fxP[fxPN++];
      p.x = (int16_t)bx; p.y = (int16_t)by;
      p.vx = (int16_t)(cosf(a) * sp); p.vy = (int16_t)(sinf(a) * sp - 40);
      p.t0 = (uint16_t)((wp ? burstsW[b] : burstsD[b]) + frand(0, 60));
      p.col = (i & 1) ? c1 : c2;
    }
  }
}

void stopArriveFx() { fxOn = false; }

// 一圈 w 像素粗的圆环
void fxRing(float r, int w, uint16_t c) {
  for (int k = 0; k < w; k++) if (r + k > 0) gfx->drawCircle(CX, CCY, (int)(r + k), c);
}

// 飘动的黑白格旗：(px, py) 是旗杆顶，s = 缩放，t = 毫秒
void fxFlag(float px, float py, float s, unsigned long t, uint16_t light) {
  const int COLS = 8, ROWS = 5;
  float cell = 13 * s;
  thickLine(px, py - 6 * s, px, py + 150 * s, 5 * s, 0xC618);            // 旗杆
  aaDisc(px, py - 8 * s, 5 * s, C_YELLOW);                               // 杆头
  for (int c = 0; c < COLS; c++) {
    float k = (c + 1) / (float)COLS;                                     // 越往外飘得越厉害
    float yo = sinf(t * 0.008f - c * 0.75f) * 7 * s * k;
    float x = px + 3 * s + c * cell;
    float y = py + yo;
    gfx->fillRect((int)x, (int)(y - 2), (int)ceilf(cell) + (c == COLS - 1 ? 2 : 0), (int)(ROWS * cell + 4), light);
    for (int r = 0; r < ROWS; r++)
      if ((r + c) & 1) gfx->fillRect((int)x, (int)(y + r * cell), (int)ceilf(cell), (int)ceilf(cell), C_BLACK);
  }
}

// 霓虹字：外面一圈暗色描边当光晕
void fxNeonText(const String &s, int y, int size, uint16_t col, uint16_t glow) {
  for (int dx = -2; dx <= 2; dx += 2)
    for (int dy = -2; dy <= 2; dy += 2)
      if (dx || dy) printCentered(s, CX + dx, y + dy, size, glow);
  printCentered(s, CX, y, size, col);
}

void drawArriveFx(unsigned long t) {
  onWaitScreen = false;
  frameIsMap = false;
  gfx->fillScreen(C_BLACK);
  uint16_t mc = fxWp ? C_CYAN : C_PINK;
  float fade = (t + 600 > fxDur) ? (fxDur - t) / 600.0f : 1.0f;          // 最后 0.6 秒淡出
  if (fade < 0) fade = 0;
  int fadeA = 255 - (int)(fade * 255);                                   // 和黑色混合的比例

  // 1. 放射光芒（慢慢转）
  {
    float rot = t * 0.00025f;
    uint16_t rc = blend565(blend565(mc, C_BLACK, 205), C_BLACK, fadeA);
    for (int i = 0; i < 16; i++) {
      float a0 = rot + i * (2 * PI / 16), a1 = a0 + 0.11f;
      gfx->fillTriangle(CX, CCY, CX + cosf(a0) * 260, CCY + sinf(a0) * 260,
                        CX + cosf(a1) * 260, CCY + sinf(a1) * 260, rc);
    }
  }
  // 2. 冲击波圆环
  for (int k = 0; k < 3; k++) {
    float ph = ((long)t - k * 260) / 900.0f;
    if (ph < 0 || ph > 1) continue;
    uint16_t c = blend565((k & 1) ? C_CYAN : mc, C_BLACK, (int)(ph * 255));
    fxRing(30 + ph * 210, 4, c);
  }
  // 3. 外圈霓虹跑马灯
  {
    float base = t * 0.0016f;
    for (int i = 0; i < 24; i++) {
      float a0 = base + i * (2 * PI / 24), a1 = a0 + 0.14f;
      uint16_t c = blend565((i & 1) ? C_CYAN : mc, C_BLACK, fadeA);
      thickLine(CX + cosf(a0) * 222, CCY + sinf(a0) * 222, CX + cosf(a1) * 222, CCY + sinf(a1) * 222, 5, c);
    }
  }
  // 4. 旗子：弹出来（先放大一点再回到正常大小）
  {
    float pt = t / 450.0f;
    float s = pt >= 1 ? 1.0f : (pt < 0.7f ? pt / 0.7f * 1.15f : 1.15f - (pt - 0.7f) / 0.3f * 0.15f);
    if (fxWp) s *= 0.75f;
    if (s > 0.05f) {
      float fw = (3 + 8 * 13) * s;
      fxFlag(CX - fw / 2, (fxWp ? 120 : 92) + (1 - s) * 60, s, t, blend565(C_WHITE, C_BLACK, fadeA));
    }
  }
  // 5. 烟花粒子（带重力，越到后面越暗越小）
  {
    const float LIFE = 1.7f, G = 170;
    for (int i = 0; i < fxPN; i++) {
      float dt = ((long)t - fxP[i].t0) / 1000.0f;
      if (dt < 0 || dt > LIFE) continue;
      float x = fxP[i].x + fxP[i].vx * dt;
      float y = fxP[i].y + fxP[i].vy * dt + 0.5f * G * dt * dt;
      float lf = dt / LIFE;
      int r = lf < 0.4f ? 4 : (lf < 0.75f ? 3 : 2);
      uint16_t c = blend565(fxP[i].col, C_BLACK, (int)(lf * lf * 230));
      gfx->fillRect((int)x - r / 2, (int)y - r / 2, r + 1, r + 1, c);
      if (dt < 0.35f) {                                                  // 刚炸开时带一小段拖尾
        float tx = x - fxP[i].vx * 0.04f, ty = y - (fxP[i].vy + G * dt) * 0.04f;
        gfx->drawLine((int)tx, (int)ty, (int)x, (int)y, blend565(c, C_BLACK, 120));
      }
    }
  }
  // 6. 文字
  if (t > 250) {
    uint16_t tc = (t / 180) % 2 ? C_WHITE : mc;                        // 前 1.5 秒闪烁，之后定住
    if (t > 1500) tc = C_WHITE;
    tc = blend565(tc, C_BLACK, fadeA);
    uint16_t gc = blend565(blend565(mc, C_BLACK, 110), C_BLACK, fadeA);
    if (fxWp) fxNeonText("WAYPOINT", 268, 4, tc, gc);
    else      fxNeonText("ARRIVED", 262, 5, tc, gc);
  }
  if (!fxWp && t > 900) {
    String l1 = "", l2 = "";
    bool haveTrip = tripM >= 0 && millis() - tripAt < 30000;
    if (haveTrip) {
      l1 = (tripM >= 1000 ? String(tripM / 1000.0, 1) + " km" : String(tripM) + " m") + "  " + String(tripMin) + " min";
      if (tripMaxKmh > 0) l2 = "MAX " + String(tripMaxKmh) + " km/h";
    } else {
      l1 = "RIDE SAFE";
    }
    int n = (t - 900) / 45;                                              // 逐字打出来
    String s1 = l1.substring(0, min(n, (int)l1.length()));
    printCentered(s1, CX, 328, 3, blend565(C_CYAN, C_BLACK, fadeA));
    int n2 = n - (int)l1.length() - 4;
    if (l2.length() && n2 > 0) printCentered(l2.substring(0, min(n2, (int)l2.length())), CX, 366, 2, blend565(C_YELLOW, C_BLACK, fadeA));
  }
  needFlush = true;
}
