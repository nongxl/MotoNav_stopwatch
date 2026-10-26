#pragma once
// 位图地图采样和绘制。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// v5.7 改成按行分给两个核心画（参数放在全局变量里）
struct { float cx, cy, sc, cs, sn; uint16_t pal[16]; uint16_t *fb; bool pix2; int32_t viceGain; int viceRotation; } RM;
#define PIXB 3                                      // v6.1 掌机像素：3×3 个屏幕像素当一个大像素

void __attribute__((optimize("O3"))) rasterRows(int y0, int y1, int step) {
  uint16_t *fb = RM.fb;
  const uint16_t *pal = RM.pal;
  float cx = RM.cx, cy = RM.cy, sc = RM.sc, cs = RM.cs, sn = RM.sn;
  float inv = 1.0f / sc, kpx = inv / rasMpp;
  int32_t DU = (int32_t)(cs * kpx * 65536.0f), DV = (int32_t)(sn * kpx * 65536.0f);
  const bool useBack=rasterBackValid && rasterBackHeader.frame==rasterFrame && !rasHasPal && !rasterViceValid;
  const int backU=useBack ? (int)lroundf((rasX0-rasterBackHeader.x0)/rasMpp) : 0;
  const int backV=useBack ? (int)lroundf((rasterBackHeader.y0-rasY0)/rasMpp) : 0;
  const float R = 212;
  for (int y = y0; y < y1; y += step) {
    float dyc = y + 0.5f - CX;
    if (RM.pix2 && (y % PIXB) && y > y0 && fabsf(dyc) < R - 2) {  // v6.1 掌机像素：每 3 行只算第一行，其余照抄
      int xa = mapCircleRows.left[y], xb = mapCircleRows.right[y];
      memcpy(fb + y * LCD_WIDTH + xa, fb + (y - 1) * LCD_WIDTH + xa, (xb - xa + 1) * 2);
      continue;
    }
    if (fabsf(dyc) >= R) continue;
    int xa = mapCircleRows.left[y], xb = mapCircleRows.right[y];
    if (RM.pix2) xa -= xa % PIXB;                       // 掌机像素：列也按 3 个一组对齐
    float rx = (xa + 0.5f - CX) * inv, ry = (MAP_PY - (y + 0.5f)) * inv;
    float dx = rx * cs + ry * sn, dy = -rx * sn + ry * cs;
    float u = (cx + dx - rasX0) / rasMpp, v = (rasY0 - (cy + dy)) / rasMpp;
    int32_t U = (int32_t)(u * 65536.0f), V = (int32_t)(v * 65536.0f);
    uint16_t *row = fb+y*LCD_WIDTH;
    int xstep = RM.pix2 ? PIXB : 1;
    if (RM.pix2) { DU *= PIXB; DV *= PIXB; }
    for (int x = xa; x <= xb; x += xstep) {
      int32_t ui = U >> 16, vi = V >> 16;
      uint8_t idx = 0;
      if ((uint32_t)ui < (uint32_t)rasW && (uint32_t)vi < (uint32_t)rasH) {
        uint32_t k = ((uint32_t)((vi >> 4) * rasBW + (ui >> 4)) << 8) | ((vi & 15) << 4) | (ui & 15);
        if (rasterViceValid) {
          idx = (rasterVice[k] >> viceBaseShift) & 3;
        } else {
          uint8_t b = rasterBuf[k >> 1];
          idx = (k & 1) ? (b >> 4) : (b & 15);
        }
      } else if (useBack) {
        rasterRead(rasterBackBuf,rasterBackHeader,ui+backU,vi+backV,idx);
      }
      row[x] = pal[idx];
      for (int k = 1; k < xstep && x + k <= xb; k++) row[x + k] = pal[idx];
      U += DU; V += DV;
    }
    if (RM.pix2) { DU /= PIXB; DV /= PIXB; }
  }
}

static inline uint16_t vicePatchColor(uint16_t cell, int fx, int fy) {
  uint16_t base = RM.pal[(cell >> 14) & 3];
  uint32_t index = cell & 2047;
  if (index >= vicePatchCount || !vicePatchData) return base;
  const uint16_t *q = vicePatchData + index * 4;
  int tx = max(0, min(256, fx + 128)), ty = max(0, min(256, fy + 128));
  uint32_t r = 0, g = 0, b = 0;
  for (int c = 0; c < 4; c++) {
    int shift = c * 4;
    int a = (q[0] >> shift) & 15, e = (q[1] >> shift) & 15;
    int f = (q[2] >> shift) & 15, d = (q[3] >> shift) & 15;
    int top = a * 256 + (e - a) * tx, bottom = f * 256 + (d - f) * tx;
    uint32_t mass = top * (256 - ty) + bottom * ty;
    uint16_t color = RM.pal[c];
    r += (color >> 11) * mass;
    g += ((color >> 5) & 63) * mass;
    b += (color & 31) * mass;
  }
  // 每个顶点的四材质权重之和固定为 15；双线性权重之和固定为 65536。
  return (uint16_t)((((r + 491520) / 983040) << 11) |
      (((g + 491520) / 983040) << 5) | ((b + 491520) / 983040));
}

static inline bool viceReadColor(int x, int y, int fx, int fy, uint16_t &color) {
  if ((uint32_t)x >= (uint32_t)rasW || (uint32_t)y >= (uint32_t)rasH) return false;
  uint32_t k = ((uint32_t)((y >> 4) * rasBW + (x >> 4)) << 8) | ((y & 15) << 4) | (x & 15);
  uint16_t cell = rasterVice[k];
  int pair = (cell >> 11) & 7;
  color = RM.pal[(cell >> 14) & 3];
  if (pair == 7) { color = vicePatchColor(cell, fx, fy); return true; }
  if (pair == 0) return true;
  int direction = cell & 63, offset = (cell >> 6) & 31;
  int distance = viceNX[direction] * fx + viceNY[direction] * fy + viceOffsets[1][offset];
  int index = 128 + (int)((distance * RM.viceGain) >> 16);
  index = max(0, min(256, index));
  int angle = (direction - RM.viceRotation) & 15;
  if (angle > 8) angle = 16 - angle;
  color = viceEdgePal[pair][viceCoverage[angle][index]];
  return true;
}

static inline uint16_t viceJoinColor(uint16_t color, int x, int y, int qx, int qy) {
  // 仅在格子交界的 1/8 源像素带内过渡；内部轮廓保持原来的清晰度。
  int sx = qx < 32 ? -1 : (qx > 224 ? 1 : 0);
  int sy = qy < 32 ? -1 : (qy > 224 ? 1 : 0);
  int wx = sx < 0 ? (32 - qx) * 4 : (sx > 0 ? (qx - 224) * 4 : 0);
  int wy = sy < 0 ? (32 - qy) * 4 : (sy > 0 ? (qy - 224) * 4 : 0);
  int fx = qx - 128, fy = qy - 128;
  uint16_t bx = color, by = color, diagonal = color;
  if (wx && !viceReadColor(x + sx, y, fx - sx * 256, fy, bx)) wx = 0;
  if (wy && !viceReadColor(x, y + sy, fx, fy - sy * 256, by)) wy = 0;
  if (!wx) return wy ? blendF(color, by, wy) : color;
  if (!wy) return blendF(color, bx, wx);
  if (!viceReadColor(x + sx, y + sy, fx - sx * 256, fy - sy * 256, diagonal)) diagonal = by;
  uint32_t weights[4] = {(uint32_t)((256 - wx) * (256 - wy)),
    (uint32_t)(wx * (256 - wy)), (uint32_t)((256 - wx) * wy), (uint32_t)(wx * wy)};
  uint16_t colors[4] = {color, bx, by, diagonal};
  uint32_t r = 0, g = 0, b = 0;
  for (int i = 0; i < 4; i++) {
    r += (colors[i] >> 11) * weights[i];
    g += ((colors[i] >> 5) & 63) * weights[i];
    b += (colors[i] & 31) * weights[i];
  }
  return (uint16_t)((((r + 32768) >> 16) << 11) |
      (((g + 32768) >> 16) << 5) | ((b + 32768) >> 16));
}

void rasterViceRows(int y0, int y1, int step) {
  uint16_t *fb = RM.fb;
  const uint16_t *pal = RM.pal;
  const uint16_t *cells = rasterVice;
  float inv = 1.0f / RM.sc, kpx = inv / rasMpp;
  int32_t du = (int32_t)(RM.cs * kpx * 65536.0f), dv = (int32_t)(RM.sn * kpx * 65536.0f);
  const int32_t gain = RM.viceGain;
  const int pairShift = rasterViceFine ? 11 : 10;
  const int offsetShift = rasterViceFine ? 6 : 5;
  const int baseShift = viceBaseShift;
  const int32_t *offsets = viceOffsets[rasterViceFine ? 1 : 0];
  const bool fine = rasterViceFine;
  const bool joins = rasterViceVersion == 3;
  const int rotation = RM.viceRotation;
  for (int y = y0; y < y1; y += step) {
    float dyc = y + 0.5f - CX;
    if (fabsf(dyc) >= 212) continue;
    int xa = mapCircleRows.left[y], xb = mapCircleRows.right[y];
    float rx = (xa + 0.5f - CX) * inv, ry = (MAP_PY - (y + 0.5f)) * inv;
    float dx = rx * RM.cs + ry * RM.sn, dy = -rx * RM.sn + ry * RM.cs;
    int32_t u = (int32_t)(((RM.cx + dx - rasX0) / rasMpp) * 65536.0f);
    int32_t v = (int32_t)(((rasY0 - (RM.cy + dy)) / rasMpp) * 65536.0f);
    uint16_t *out = fb + y * LCD_WIDTH;
    for (int x = xa; x <= xb; x++, u += du, v += dv) {
      int ui = u >> 16, vi = v >> 16;
      uint16_t color = pal[0];
      if ((uint32_t)ui < (uint32_t)rasW && (uint32_t)vi < (uint32_t)rasH) {
        uint32_t k = ((uint32_t)((vi >> 4) * rasBW + (ui >> 4)) << 8) | ((vi & 15) << 4) | (ui & 15);
        uint16_t cell = cells[k];
        uint32_t pair = (cell >> pairShift) & 7;
        color = pal[(cell >> baseShift) & 3];
        if (pair > 0 && (pair <= 6 || joins)) {
          int qx = (u >> 8) & 255, qy = (v >> 8) & 255;
          int fx = qx - 128, fy = qy - 128;
          if (pair <= 6) {
            int direction = fine ? (cell & 63) : ((cell & 31) << 1);
            int offset = (cell >> offsetShift) & 31;
            int distance = viceNX[direction] * fx + viceNY[direction] * fy + offsets[offset];
            int index = 128 + (int)((distance * gain) >> 16);
            index = max(0, min(256, index));
            int angle = (direction - rotation) & 15;
            if (angle > 8) angle = 16 - angle;
            color = viceEdgePal[pair][viceCoverage[angle][index]];
          } else {
            color = vicePatchColor(cell, fx, fy);
          }
          if (joins && (qx < 32 || qx > 224 || qy < 32 || qy > 224))
            color = viceJoinColor(color, ui, vi, qx, qy);
        }
      }
      out[x] = color;
    }
  }
}

uint32_t rasMsAcc = 0;                     // v5.7 统计：底图用时
void drawRasterMap(float cx, float cy, float sc, float cs, float sn) {
  unsigned long t0 = millis();
  RM.fb = gfx->getFramebuffer();
  if (rasHasPal) {                                     // v6.1 卫星图：用照片自带的颜色（夜间压暗）
    for (int i = 0; i < 16; i++) RM.pal[i] = (themeIdx != 0 && nightOn) ? blendF(rasPal[i], C_BLACK, 110) : rasPal[i];
  } else {
    for (int i = 0; i < 16; i++) RM.pal[i] = C_BLOCK;
    RM.pal[1] = C_ROADM; RM.pal[2] = C_PARK; RM.pal[3] = C_WATER;
    RM.pal[4] = blendF(C_BLOCK,C_ROADM,64);   // 俯视屋顶
    RM.pal[5] = blendF(C_BLOCK,C_ROADM,112);  // 屋顶轮廓
    RM.pal[6] = RM.pal[4]; RM.pal[7] = RM.pal[5]; // 保留原绿地类别的屋顶
  }
  if (styleIdx() == ST_MINIMAL) {
    for (int i=0;i<16;i++) RM.pal[i]=0x0000;
    RM.pal[1]=0x8410; // independent monochrome road mask; no building/park/water layer
  }
  RM.pix2 = styleIdx() == ST_PIXEL;
  RM.cx = cx; RM.cy = cy; RM.sc = sc; RM.cs = cs; RM.sn = sn;
  if (styleIdx() == 1 && !rasHasPal && rasterViceValid) {
    prepareVicePalette(RM.pal);
    // 按方形像素的面积覆盖率过渡，斜边的支撑范围自然扩展到约 1.414 像素。
    RM.viceGain = (int32_t)(sc * rasMpp * (65536.0f / (127.0f * 1.5f)));
    RM.viceRotation = (int)lroundf(atan2f(sn, cs) * (32.0f / PI));
    parallelRows(rasterViceRows, (int)(CX - 213), (int)(CX + 213));
  } else {
    parallelRows(rasterRows, (int)(CX - 213), (int)(CX + 213));
  }
  rasMsAcc += millis() - t0;
}
