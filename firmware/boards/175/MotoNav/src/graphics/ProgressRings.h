#pragma once
// 进度环及边缘告警绘制。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// ================= 圆环 =================

// 填充一段圆环：p0~p1 表示位置（0 = 正上方，顺时针，1 = 一整圈），每 0.5° 两个三角形
// v1.8.3 抗锯齿圆环：开机时把半径 RLUT_MIN~RLUT_MAX 之间每个像素的“半径、角度”算好存表，
// 按整数半径分桶；画环时只遍历需要的那几圈像素，按到边缘的距离算覆盖率，和底色混合。
#define RLUT_MIN 212
#define RLUT_MAX 236
#define RLUT_N   (RLUT_MAX - RLUT_MIN)
struct RingPix { uint32_t idx; uint16_t ang; uint16_t r8; };   // 像素位置、角度（0~65535 = 一圈）、半径×256
RingPix *ringLut = nullptr;
int ringOff[RLUT_N + 1];

void buildRingLut() {
  int cnt[RLUT_N] = {0};
  for (int pass = 0; pass < 2; pass++) {
    if (pass == 1) {
      int total = 0;
      for (int b = 0; b < RLUT_N; b++) { ringOff[b] = total; total += cnt[b]; cnt[b] = 0; }
      ringOff[RLUT_N] = total;
      ringLut = (RingPix *)ps_malloc(sizeof(RingPix) * (total + 1));
      if (!ringLut) return;
    }
    for (int y = 0; y < LCD_HEIGHT; y++) {
      float dy = y + 0.5f - CX;
      for (int x = 0; x < LCD_WIDTH; x++) {
        float dx = x + 0.5f - CX;
        float r = sqrtf(dx * dx + dy * dy);
        int b = (int)floorf(r) - RLUT_MIN;
        if (b < 0 || b >= RLUT_N) continue;
        if (pass == 0) { cnt[b]++; continue; }
        float a = atan2f(dx, -dy) / (2 * PI);                     // 0 = 正上方，顺时针
        if (a < 0) a += 1;
        RingPix &e = ringLut[ringOff[b] + cnt[b]++];
        e.idx = y * LCD_WIDTH + x;
        e.ang = (uint16_t)fminf(a * 65536.0f, 65535.0f);
        e.r8 = (uint16_t)(r * 256);
      }
    }
  }
}

// 两个 RGB565 颜色按 t（0~255）混合
static inline uint16_t blend565(uint16_t a, uint16_t b, int t) {
  int it = 255 - t;
  int r = (((a >> 11) & 0x1F) * it + ((b >> 11) & 0x1F) * t + 127) / 255;
  int g = (((a >> 5) & 0x3F) * it + ((b >> 5) & 0x3F) * t + 127) / 255;
  int bl = ((a & 0x1F) * it + (b & 0x1F) * t + 127) / 255;
  return (r << 11) | (g << 5) | bl;
}

// 抗锯齿实心圆（和已有内容混合）
void aaDisc(float cx, float cy, float rad, uint16_t c) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) { gfx->fillCircle(cx, cy, rad, c); return; }
  int x0 = max(0, (int)floorf(cx - rad - 1)), x1 = min(LCD_WIDTH - 1, (int)ceilf(cx + rad + 1));
  int y0 = max(0, (int)floorf(cy - rad - 1)), y1 = min(LCD_HEIGHT - 1, (int)ceilf(cy + rad + 1));
  for (int y = y0; y <= y1; y++) {
    float dy = y + 0.5f - cy;
    for (int x = x0; x <= x1; x++) {
      float dx = x + 0.5f - cx;
      float a = rad - sqrtf(dx * dx + dy * dy) + 0.5f;
      if (a <= 0) continue;
      int t = a >= 1 ? 255 : (int)(a * 255);
      uint16_t &px = fb[y * LCD_WIDTH + x];
      px = (t == 255) ? c : blend565(px, c, t);
    }
  }
}

// 填充一段圆环：p0~p1 表示位置（0 = 正上方，顺时针，1 = 一整圈），rIn~rOut 为内外半径
// v2.6 整圈圆环：按行填充（中间实心部分直接写，只有边缘一两个像素算覆盖率）
void fillAnnulus(float rIn, float rOut, uint16_t c) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  int y0 = max(0, (int)floorf(CX - rOut - 1)), y1 = min(LCD_HEIGHT - 1, (int)ceilf(CX + rOut + 1));
  float ro2 = (rOut + 0.5f) * (rOut + 0.5f), ri2 = (rIn - 0.5f) * (rIn - 0.5f);
  float so2 = (rOut - 0.5f) * (rOut - 0.5f), si2 = (rIn + 0.5f) * (rIn + 0.5f);
  for (int y = y0; y <= y1; y++) {
    float dy = y + 0.5f - CX, dy2 = dy * dy;
    if (dy2 >= ro2) continue;
    float ho = sqrtf(ro2 - dy2);                            // 有覆盖的外沿
    float hi = dy2 < ri2 ? sqrtf(ri2 - dy2) : -1;           // 有覆盖的内沿（空心）
    float so = dy2 < so2 ? sqrtf(so2 - dy2) : -1;           // 实心外沿
    float si = dy2 < si2 ? sqrtf(si2 - dy2) : 0;            // 实心内沿
    uint16_t *row = fb + y * LCD_WIDTH;
    for (int side = 0; side < 2; side++) {
      int xa, xb;
      if (hi < 0) {                                          // 这一行没有空心：一整段
        if (side) break;
        xa = (int)floorf(CX - ho); xb = (int)ceilf(CX + ho);
      } else if (side == 0) {
        xa = (int)floorf(CX - ho); xb = (int)ceilf(CX - hi);
      } else {
        xa = (int)floorf(CX + hi); xb = (int)ceilf(CX + ho);
      }
      xa = max(xa, 0); xb = min(xb, LCD_WIDTH - 1);
      for (int x = xa; x <= xb; x++) {
        float dx = fabsf(x + 0.5f - CX);
        if (so >= 0 && dx >= si && dx <= so) { row[x] = c; continue; }
        float r = sqrtf(dx * dx + dy2);
        float a = fminf(r - rIn + 0.5f, rOut - r + 0.5f);
        if (a <= 0) continue;
        if (a >= 1) { row[x] = c; continue; }
        row[x] = blend565(row[x], c, (int)(a * 255));
      }
    }
  }
}

void fillRingBand(float p0, float p1, float rIn, float rOut, uint16_t c) {
  if (p1 <= p0) return;
  if (p1 - p0 >= 0.9999f) { fillAnnulus(rIn, rOut, c); return; }   // v2.6 整圈走快速路径
  uint16_t *fb = gfx->getFramebuffer();
  if (!ringLut || !fb) return;
  bool full = (p1 - p0) >= 0.9999f;
  int b0 = (int)floorf(rIn - 1) - RLUT_MIN, b1 = (int)ceilf(rOut + 1) - RLUT_MIN;
  if (b0 < 0) b0 = 0;
  if (b1 > RLUT_N) b1 = RLUT_N;
  if (b0 >= b1) return;
  const float TWO_PI_F = 2 * PI;
  for (int i = ringOff[b0]; i < ringOff[b1]; i++) {
    const RingPix &e = ringLut[i];
    float r = e.r8 * (1.0f / 256);
    float ar = fminf(r - rIn + 0.5f, rOut - r + 0.5f);
    if (ar <= 0) continue;
    if (ar > 1) ar = 1;
    if (!full) {
      float a = e.ang * (1.0f / 65536);
      float aa = fminf((a - p0) * TWO_PI_F * r, (p1 - a) * TWO_PI_F * r) + 0.5f;   // 到两端的弧长（像素）
      if (aa <= 0) continue;
      if (aa < 1) ar *= aa;
    }
    int t = (int)(ar * 255);
    if (t <= 0) continue;
    uint16_t &px = fb[e.idx];
    px = (t >= 255) ? c : blend565(px, c, t);
  }
}

void drawRingTrack() { fillRingBand(0, 1, RING_R - 5, RING_R + 5, C_TRACK); }

void drawRingProgress(float p, int thick, uint16_t col) {
  if (p <= 0.002f) return;
  if (p > 1) p = 1;
  float h = thick / 2.0f;
  fillRingBand(0, p, RING_R - h, RING_R + h, col);
  float a = p * 2 * PI;
  // 两端圆头（像素中心在 +0.5 处，所以圆心也取 CX 而不是 CX-0.5）
  aaDisc(CX, CX - RING_R, h, col);
  aaDisc(CX + RING_R * sinf(a), CX - RING_R * cosf(a), h, col);
}

// v7.3 抗锯齿圆弧带：角度 a0 → a1（度，0 = 正上方，顺时针；跨度要小于 180°），半径 rIn~rOut，alpha 0~255
void arcBand(float a0, float a1, float rIn, float rOut, uint16_t c, int alpha = 255) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb || a1 <= a0) return;
  float s0 = sinf(a0 * PI / 180), c0 = cosf(a0 * PI / 180);
  float s1 = sinf(a1 * PI / 180), c1 = cosf(a1 * PI / 180);
  float ro2 = (rOut + 1) * (rOut + 1), ri2 = rIn > 1 ? (rIn - 1) * (rIn - 1) : 0;
  int y0 = max(0, (int)(CX - rOut - 1)), y1 = min(LCD_HEIGHT - 1, (int)(CX + rOut + 1));
  for (int y = y0; y <= y1; y++) {
    float dy = y + 0.5f - CX, dy2 = dy * dy;
    if (dy2 >= ro2) continue;
    float ho = sqrtf(ro2 - dy2), hi = dy2 < ri2 ? sqrtf(ri2 - dy2) : 0;
    uint16_t *row = fb + y * LCD_WIDTH;
    for (int side = 0; side < 2; side++) {
      int xa = side ? (int)(CX + hi) : (int)(CX - ho) - 1;
      int xb = side ? (int)(CX + ho) + 1 : (int)(CX - hi) + 1;
      xa = max(xa, 0); xb = min(xb, LCD_WIDTH - 1);
      for (int x = xa; x <= xb; x++) {
        float dx = x + 0.5f - CX;
        float e0 = s0 * dy + c0 * dx;                    // 到起点那条半径线的距离（在弧这边为正）
        float e1 = -(s1 * dy + c1 * dx);                 // 到终点那条半径线的距离（在弧这边为正）
        if (e0 < -0.5f || e1 < -0.5f) continue;
        float r = sqrtf(dx * dx + dy2);
        float a = fminf(fminf(r - rIn, rOut - r), fminf(e0, e1)) + 0.5f;
        if (a <= 0) continue;
        int t = a >= 1 ? alpha : (int)(a * alpha);
        if (t <= 0) continue;
        uint16_t &px = row[x];
        px = t >= 255 ? c : blend565(px, c, t);
      }
    }
  }
}

// v7.4 路程进度：0~1（走过的比例）；不显示时返回 -1
#define TRIP_DONE_C 0x5AEB          // 走过的：灰
#define TRIP_LEFT_C 0x26E8          // 没走的：绿 #22DD44
#define TRIP_H      2               // 行程环半宽（v7.5 整环 8 → 4 像素粗）
float tripFrac() {
  if (!progOn || !tripOn || cruiseOn || progLastRm < 0) return -1;
  long total = progDone + tripRemainM;
  if (total < 300) return -1;                            // 太短的路线不显示
  return tripProgress.value(millis());
}

// 行程环上“现在的位置”小白点
void drawTripDot(float p) {
  if (p <= 0.002f || p >= 0.998f) return;
  float a = p * 2 * PI;
  float x = CX + RING_R * sinf(a), y = CX - RING_R * cosf(a);
  aaDisc(x, y, 5.0f, C_BLACK);                           // v7.5 环变细了，小白点保持原来大小
  aaDisc(x, y, 3.5f, C_WHITE);
}

// 大箭头画面用：整圈行程环
void drawTripRing(float p) {
  fillRingBand(0, 1, RING_R - TRIP_H, RING_R + TRIP_H, TRIP_LEFT_C);
  if (p > 0.002f) fillRingBand(0, p, RING_R - TRIP_H, RING_R + TRIP_H, TRIP_DONE_C);
  drawTripDot(p);
}

// 屏幕边缘的红圈（断线、数据中断用）
void drawRedRing() {
  fillRingBand(0, 1, 222, 232, C_RED);      // v1.8.3 抗锯齿
}

// 顶部小圆点：绿 = 蓝牙已连接，灰 = 未连接
