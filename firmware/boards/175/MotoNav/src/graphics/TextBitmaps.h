#pragma once
// 手机文字位图绘制。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// ================= 文字图片 =================

// 把 4 位灰度图画到 (中心 x, 顶部 y)，tint = 最亮处的灰度 0~255
// v1.7.1：如果图片超出半径 TEXT_R 的圆，就整体等比缩小（按面积取平均，缩小后依然平滑）
static inline uint8_t imgNib(const TextImg &im, int bpr, int x, int y) {
  uint8_t b = im.show[y * bpr + (x >> 1)];
  return (x & 1) ? (b & 0x0F) : (b >> 4);
}

static inline uint16_t greyColor(uint8_t a, uint8_t tint) {      // a = 0~15
  uint8_t g = (uint16_t)(a * 17) * tint / 255;
  return ((g >> 3) << 11) | ((g >> 2) << 5) | (g >> 3);
}

void drawTextImg(int slot, int y0, uint8_t tint) {
  TextImg &im = imgs[slot];
  if (!im.show || im.showW == 0 || im.showH == 0) return;
  int bpr = (im.showW + 1) / 2;

  // 算缩放比例：图片四个角都要落在半径 TEXT_R 的圆里
  float s = 1.0f;
  for (int it = 0; it < 4; it++) {
    float h = im.showH * s;
    float dy = fmaxf(fabsf(y0 - CX), fabsf(y0 + h - CX));
    float half = dy < TEXT_R ? sqrtf((float)TEXT_R * TEXT_R - dy * dy) : 0;
    if (im.showW * s / 2 <= half) break;
    s = 2 * half / im.showW;
  }
  if (s < 0.5f) s = 0.5f;

  if (s >= 0.999f) {                       // 不用缩小：原样画
    int x0 = CX - im.showW / 2;
    for (int y = 0; y < im.showH; y++)
      for (int x = 0; x < im.showW; x++) {
        uint8_t a = imgNib(im, bpr, x, y);
        if (a) gfx->drawPixel(x0 + x, y0 + y, greyColor(a, tint));
      }
    return;
  }

  int dw = (int)(im.showW * s), dh = (int)(im.showH * s);
  if (dw < 1 || dh < 1) return;
  int x0 = CX - dw / 2;
  float inv = 1.0f / s;
  for (int dy = 0; dy < dh; dy++) {
    int sy0 = (int)(dy * inv), sy1 = (int)((dy + 1) * inv);
    if (sy1 <= sy0) sy1 = sy0 + 1;
    if (sy1 > im.showH) sy1 = im.showH;
    for (int dx = 0; dx < dw; dx++) {
      int sx0 = (int)(dx * inv), sx1 = (int)((dx + 1) * inv);
      if (sx1 <= sx0) sx1 = sx0 + 1;
      if (sx1 > im.showW) sx1 = im.showW;
      int sum = 0, cnt = 0;
      for (int y = sy0; y < sy1; y++)
        for (int x = sx0; x < sx1; x++) { sum += imgNib(im, bpr, x, y); cnt++; }
      uint8_t a = cnt ? (sum + cnt / 2) / cnt : 0;
      if (a) gfx->drawPixel(x0 + dx, y0 + dy, greyColor(a, tint));
    }
  }
}
