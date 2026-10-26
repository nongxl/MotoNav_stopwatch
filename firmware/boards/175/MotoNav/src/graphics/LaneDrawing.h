#pragma once
// 车道图标缓存和绘制。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// ================= 车道 =================

// 高德车道方向码 → 方向位：1 直行，2 左转，4 右转，8 左掉头，16 右掉头
// 注意：这张对照表来自经验，可能需要实测校准（App 上会显示原始编码）
uint8_t laneFlags(uint8_t code) {
  switch (code) {
    case 0:  return 1;
    case 1:  return 2;
    case 2:  return 1 | 2;
    case 3:  return 4;
    case 4:  return 1 | 4;
    case 5:  return 8;
    case 6:  return 2 | 4;
    case 7:  return 1 | 2 | 4;
    case 8:  return 16;
    case 9:  return 1 | 8;
    case 10: return 1 | 16;
    case 11: return 2 | 8;
    case 12: return 4 | 16;
    default: return 1;
  }
}

// v3.4 车道图标改成抗锯齿：先把线段/三角形“记录”下来（和大箭头同一套），按每个像素的覆盖率
//      算好颜色和透明度存进车道缓存；车道不变就直接用缓存，每帧和地图混合（边缘平滑，拐角是圆弧）
#define LANE_BUF_W 320
#define LANE_BUF_H 44
uint16_t *laneCol = nullptr;
uint8_t *laneA = nullptr;
int laneBX = 0, laneBY = 0, laneBW = 0, laneBH = 0;
uint32_t laneHash = 0;

// 画一段圆弧（用几小段线连起来）；角度：0 = 朝右，90 = 朝下（屏幕坐标）
void arcLine(float ox, float oy, float r, float a0, float a1, float w, uint16_t col) {
  const int N = 4;
  float px = ox + r * cosf(a0 * PI / 180), py = oy + r * sinf(a0 * PI / 180);
  for (int i = 1; i <= N; i++) {
    float a = (a0 + (a1 - a0) * i / N) * PI / 180;
    float nx = ox + r * cosf(a), ny = oy + r * sinf(a);
    thickLine(px, py, nx, ny, w, col);
    px = nx; py = ny;
  }
}

// 在一条车道里画方向符号（只记录图形）；cx = 车道中心 x，y0 = 顶部，h = 高度
void drawLaneGlyph(float cx, float y0, float h, uint8_t flags, uint16_t col) {
  const float k = h / 32.0f;
  const float w = 4.6f * k, hs = 6.8f * k;
  const float base = y0 + h, top = y0 + 3 * k;
  if (flags & 1) {                                         // 直行
    thickLine(cx, base, cx, top + hs, w, col);
    arrowHead(cx, top + hs, 0, hs, col);
  }
  if (flags & (2 | 4)) {                                   // 左转/右转：竖线 → 圆弧 → 横线 → 箭头
    const float rr = 4.5f * k, arm = 9.0f * k;
    float cy = y0 + h * ((flags & 1) ? 0.6f : 0.42f);     // 和直行合在一起时拐点放低
    thickLine(cx, base, cx, cy + rr, w, col);
    if (flags & 4) {
      arcLine(cx + rr, cy + rr, rr, 180, 270, w, col);
      thickLine(cx + rr, cy, cx + arm, cy, w, col);
      arrowHead(cx + arm, cy, 90, hs, col);
    }
    if (flags & 2) {
      arcLine(cx - rr, cy + rr, rr, 0, -90, w, col);
      thickLine(cx - rr, cy, cx - arm, cy, w, col);
      arrowHead(cx - arm, cy, -90, hs, col);
    }
  }
  if (flags & (8 | 16)) {                                  // 掉头：竖线 → 半圆 → 向下 + 箭头
    const float u = 4.2f * k, ut = top + 6 * k + u, down = y0 + h * 0.62f;
    thickLine(cx, base, cx, ut, w, col);
    if (flags & 8) {
      arcLine(cx - u, ut, u, 0, -180, w, col);
      thickLine(cx - 2 * u, ut, cx - 2 * u, down - hs, w, col);
      arrowHead(cx - 2 * u, down - hs, 180, hs, col);
    }
    if (flags & 16) {
      arcLine(cx + u, ut, u, 180, 360, w, col);
      thickLine(cx + 2 * u, ut, cx + 2 * u, down - hs, w, col);
      arrowHead(cx + 2 * u, down - hs, 180, hs, col);
    }
  }
  if (!(flags & (1 | 2 | 4 | 8 | 16))) {                   // 没有方向位时画直行
    thickLine(cx, base, cx, top + hs, w, col);
    arrowHead(cx, top + hs, 0, hs, col);
  }
}

// 把记录好的车道图形算成“颜色 + 透明度”存进缓存
void laneRender() {
  float bx0 = 1e9f, by0 = 1e9f, bx1 = -1e9f, by1 = -1e9f;
  for (int i = 0; i < primN; i++) {
    bx0 = fminf(bx0, prims[i].x0); by0 = fminf(by0, prims[i].y0);
    bx1 = fmaxf(bx1, prims[i].x1); by1 = fmaxf(by1, prims[i].y1);
  }
  laneBX = max(0, (int)floorf(bx0)); laneBY = max(0, (int)floorf(by0));
  laneBW = min(LANE_BUF_W, (int)ceilf(bx1) + 1 - laneBX);
  laneBH = min(LANE_BUF_H, (int)ceilf(by1) + 1 - laneBY);
  if (laneBX + laneBW > LCD_WIDTH) laneBW = LCD_WIDTH - laneBX;
  if (laneBY + laneBH > LCD_HEIGHT) laneBH = LCD_HEIGHT - laneBY;
  if (laneBW <= 0 || laneBH <= 0) { laneBW = laneBH = 0; return; }
  for (int y = 0; y < laneBH; y++) {
    float py = laneBY + y + 0.5f;
    for (int x = 0; x < laneBW; x++) {
      float px = laneBX + x + 0.5f;
      float r = 0, g = 0, b = 0, A = 0;                  // 从透明开始，按顺序叠加每个图形
      for (int i = 0; i < primN; i++) {
        const Prim &p = prims[i];
        if (px < p.x0 || px > p.x1 || py < p.y0 || py > p.y1) continue;
        float a = primAlpha(p, px, py);
        if (a <= 0) continue;
        float cr = ((p.col >> 11) & 0x1F) / 31.0f, cg = ((p.col >> 5) & 0x3F) / 63.0f, cb = (p.col & 0x1F) / 31.0f;
        r += (cr - r) * a; g += (cg - g) * a; b += (cb - b) * a; A += (1 - A) * a;
      }
      int k = y * LANE_BUF_W + x;
      if (A < 0.01f) { laneA[k] = 0; continue; }
      r /= A; g /= A; b /= A;                              // 去掉透明度，得到纯颜色
      laneCol[k] = ((uint16_t)lroundf(fminf(r, 1) * 31) << 11) | ((uint16_t)lroundf(fminf(g, 1) * 63) << 5) | (uint16_t)lroundf(fminf(b, 1) * 31);
      laneA[k] = (uint8_t)lroundf(fminf(A, 1) * 255);
    }
  }
}

void drawLanes(int y0) { drawLanesAt(CX, y0, 300, 32); }

// v2.1 在指定位置画车道：cx = 中心 x，maxW = 最大总宽，h = 高度
void drawLanesAt(float centerX, float y0, float maxW, float h) {
  if (laneN <= 0) return;
  if (!laneCol) {
    laneCol = (uint16_t *)malloc(LANE_BUF_W * LANE_BUF_H * 2);
    laneA = (uint8_t *)malloc(LANE_BUF_W * LANE_BUF_H);
    if (!laneCol || !laneA) return;
  }
  float lw = fminf(36 * h / 32, maxW / laneN);
  float x0 = centerX - lw * laneN / 2;
  // 1. 记录所有车道的图形
  primN = 0; recIcon = true;
  for (int i = 0; i < laneN; i++) {
    float cx = x0 + lw * (i + 0.5f);
    bool rec = laneFr[i] != 0xF;
    drawLaneGlyph(cx, y0, h, laneFlags(laneBg[i]), rec ? C_SEP : C_DIM);   // 底层：这条车道能走的方向
    if (rec) drawLaneGlyph(cx, y0, h, laneFlags(laneFr[i]), C_WHITE);       // 推荐方向：白色
  }
  recIcon = false;
  // 2. 和上次一样就用缓存，否则重新计算
  uint32_t hs = 2166136261u;
  const uint8_t *bytes = (const uint8_t *)prims;
  for (size_t i = 0; i < sizeof(Prim) * primN; i++) { hs ^= bytes[i]; hs *= 16777619u; }
  hs ^= primN * 7919u;
  if (hs != laneHash) { laneRender(); laneHash = hs; }
  primN = 0;
  // 3. 和底下的画面按透明度混合
  uint16_t *fb = gfx->getFramebuffer();
  if (fb) {
    for (int y = 0; y < laneBH; y++) {
      uint16_t *dst = fb + (laneBY + y) * LCD_WIDTH + laneBX;
      const uint16_t *sc = laneCol + y * LANE_BUF_W;
      const uint8_t *sa = laneA + y * LANE_BUF_W;
      for (int x = 0; x < laneBW; x++) {
        uint8_t a = sa[x];
        if (!a) continue;
        dst[x] = a == 255 ? sc[x] : blend565(dst[x], sc[x], a);
      }
    }
  }
  // 4. 车道之间的虚线
  for (int i = 1; i < laneN; i++)
    for (float y = y0; y < y0 + h; y += 8) gfx->drawFastVLine(rnd(x0 + lw * i), rnd(y), 4, C_SEP);
}
