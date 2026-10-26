#pragma once
// Frozen 7.12.3 implementations for hardware comparisons only.
void fastPushBaseline(const uint16_t *fb) {
  panel->startWrite();
  panel->writeAddrWindow(0, 0, LCD_WIDTH, LCD_HEIGHT);   // 设置写入区域（整屏）+ 写显存命令
  panel->endWrite();
  spi_device_acquire_bus(fastDev, portMAX_DELAY);
  digitalWrite(LCD_CS, LOW);
  static spi_transaction_ext_t tr[2];
  const uint32_t total = (uint32_t)LCD_WIDTH * LCD_HEIGHT;
  uint32_t sent = 0;
  int bi = 0, inflight = 0;
  bool first = true;
  while (sent < total) {
    uint32_t l = total - sent;
    if (l > FAST_PIX) l = FAST_PIX;
    if (inflight == 2) {                                  // 两块都在传：等最早那块传完再复用
      spi_transaction_t *r;
      spi_device_get_trans_result(fastDev, &r, portMAX_DELAY);
      inflight--;
    }
    // 拷贝并交换高低字节（屏幕要高字节在前）
    if (rotFree) {
      // v6.4 任意角度：屏幕上的点 (px,py) 取画布上转过去的点；圆外面涂黑
      uint16_t *d16 = (uint16_t *)fastBuf[bi];
      const int W = LCD_WIDTH, C = LCD_WIDTH / 2;
      int py = sent / W, px = sent - py * W;
      int32_t fx = 0, fy = 0;
      int xs = 0, xe = -1;
      bool rowInit = false;
      for (uint32_t i = 0; i < l; i++) {
        if (!rowInit) {
          int hw = rotSpan[py];
          xs = C - hw; xe = C + hw;
          // 以像素中心算（屏幕宽 466 是双数，正中心在 232.5）：dx2、dy2 是离中心距离的 2 倍
          int dx2 = 2 * px - (W - 1), dy2 = 2 * py - (W - 1);
          // 源点 = (dx·cos + dy·sin, −dx·sin + dy·cos) + 中心；+0.5 四舍五入
          fx = ((dx2 * rotCos + dy2 * rotSin) >> 1) + ((W - 1) << 15) + 32768;
          fy = ((-dx2 * rotSin + dy2 * rotCos) >> 1) + ((W - 1) << 15) + 32768;
          rowInit = true;
        }
        uint16_t v = 0;
        if (px >= xs && px <= xe) {
          // v7.7 双线性插值：取周围 4 个点按距离混合（以前只取最近的一个点，斜着转时字的笔画粗细不均、发糊）
          int32_t bx = fx - 32768, by = fy - 32768;          // 去掉“四舍五入”的 0.5，换成小数部分当权重
          int sx = bx >> 16, sy = by >> 16;
          if ((unsigned)sx < (unsigned)(W - 1) && (unsigned)sy < (unsigned)(W - 1)) {
            const uint16_t *r0 = fb + sy * W + sx, *r1 = r0 + W;
            uint32_t ax = (bx >> 11) & 31, ay = (by >> 11) & 31;   // 0~31 的权重
            uint32_t a = rgbSpread(r0[0]), b = rgbSpread(r0[1]), c = rgbSpread(r1[0]), d = rgbSpread(r1[1]);
            uint32_t top = ((a * (32 - ax) + b * ax) >> 5) & 0x07E0F81Fu;
            uint32_t bot = ((c * (32 - ax) + d * ax) >> 5) & 0x07E0F81Fu;
            uint32_t m = ((top * (32 - ay) + bot * ay) >> 5) & 0x07E0F81Fu;
            v = (uint16_t)(m | (m >> 16));
          } else {
            int nx = fx >> 16, ny = fy >> 16;                 // 画布最边上一圈：还按最近的点取
            if ((unsigned)nx < (unsigned)W && (unsigned)ny < (unsigned)W) v = fb[ny * W + nx];
          }
        }
        d16[i] = (uint16_t)((v >> 8) | (v << 8));
        fx += rotCos; fy -= rotSin;
        if (++px == W) { px = 0; py++; rowInit = false; }
      }
    } else if (screenRot == 0) {
      const uint32_t *src = (const uint32_t *)(fb + sent);
      uint32_t *dst = (uint32_t *)fastBuf[bi];
      for (uint32_t i = 0; i < l / 2; i++) {
        uint32_t v = src[i];
        dst[i] = ((v & 0x00FF00FFu) << 8) | ((v >> 8) & 0x00FF00FFu);
      }
    } else {
      // v6.2 转向：屏幕上第 j 个像素取画布上转过去的那个位置
      uint16_t *d16 = (uint16_t *)fastBuf[bi];
      const uint32_t W = LCD_WIDTH, N = (uint32_t)LCD_WIDTH * LCD_HEIGHT;
      uint32_t py = sent / W, px = sent - py * W;
      for (uint32_t i = 0; i < l; i++) {
        uint32_t idx;
        if (screenRot == 2) idx = N - 1 - (sent + i);
        else if (screenRot == 1) idx = (W - 1 - px) * W + py;
        else idx = px * W + (W - 1 - py);
        uint16_t v = fb[idx];
        d16[i] = (uint16_t)((v >> 8) | (v << 8));
        if (++px == W) { px = 0; py++; }
      }
    }
    spi_transaction_ext_t &t = tr[bi];
    memset(&t, 0, sizeof(t));
    if (first) {
      t.base.flags = SPI_TRANS_MODE_QIO;
      t.base.cmd = 0x32;
      t.base.addr = 0x003C00;
      first = false;
    } else {
      t.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
    }
    t.base.tx_buffer = fastBuf[bi];
    t.base.length = l * 16;
    spi_device_queue_trans(fastDev, (spi_transaction_t *)&t, portMAX_DELAY);
    inflight++;
    bi ^= 1;
    sent += l;
  }
  while (inflight) {
    spi_transaction_t *r;
    spi_device_get_trans_result(fastDev, &r, portMAX_DELAY);
    inflight--;
  }
  digitalWrite(LCD_CS, HIGH);
  spi_device_release_bus(fastDev);
}

void rasterRowsBaseline(int y0, int y1, int step) {
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
      float half = sqrtf(R * R - dyc * dyc);
      int xa = max(0, (int)(CX - half)), xb = min(LCD_WIDTH - 1, (int)(CX + half));
      memcpy(fb + y * LCD_WIDTH + xa, fb + (y - 1) * LCD_WIDTH + xa, (xb - xa + 1) * 2);
      continue;
    }
    if (fabsf(dyc) >= R) continue;
    float half = sqrtf(R * R - dyc * dyc);
    int xa = max(0, (int)(CX - half)), xb = min(LCD_WIDTH - 1, (int)(CX + half));
    if (RM.pix2) xa -= xa % PIXB;                       // 掌机像素：列也按 3 个一组对齐
    float rx = (xa + 0.5f - CX) * inv, ry = (MAP_PY - (y + 0.5f)) * inv;
    float dx = rx * cs + ry * sn, dy = -rx * sn + ry * cs;
    float u = (cx + dx - rasX0) / rasMpp, v = (rasY0 - (cy + dy)) / rasMpp;
    int32_t U = (int32_t)(u * 65536.0f), V = (int32_t)(v * 65536.0f);
    uint16_t *row = fb + y * LCD_WIDTH;
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

static inline void ringPixelBaseline(uint16_t *px, int ix, long iy2, int iy) {
  long q = (long)ix * ix + iy2;
  int k = (int)((q - RQ_MIN) >> RQ_SH);
  int r64 = rq2r[k < 0 ? 0 : k];
  uint16_t c = *px;
  int a = bandCov64(r64, 211, 240);
  if (a) c = a >= 64 ? C_BLACK : blendF(c, C_BLACK, a * 4);
  if (RP.track) { a = bandCov64(r64, RING_R - 5, RING_R + 5); if (a) c = a >= 64 ? C_TRACK : blendF(c, C_TRACK, a * 4); }
  if (RP.trip >= 0) {                                  // v7.4 行程环：走过的灰，没走的绿
    a = bandCov64(r64, RING_R - TRIP_H, RING_R + TRIP_H);
    if (a) {
      bool right = ix >= 0;
      bool before = (ix * tripUy - iy * tripUx) >= 0;
      bool done = RP.trip >= 0.9999f ? true : RP.trip <= 0.0001f ? false : (RP.trip <= 0.5f ? (right && before) : (right || before));
      uint16_t tc = done ? TRIP_DONE_C : TRIP_LEFT_C;
      c = a >= 64 ? tc : blendF(c, tc, a * 4);
    }
  }
  if (RP.prog > 0.002f) {
    int h = (int)RP.ph;
    a = bandCov64(r64, RING_R - h, RING_R + h);
    if (a) {
      // 在进度范围内吗：正上方顺时针转到末端方向 (ux, uy)
      bool right = ix >= 0;
      bool before = (ix * ringUy - iy * ringUx) >= 0;
      bool in = RP.prog >= 0.9999f ? true : (RP.prog <= 0.5f ? (right && before) : (right || before));
      if (in) c = a >= 64 ? RP.pcol : blendF(c, RP.pcol, a * 4);
    }
  }
  if (RP.over) { a = bandCov64(r64, 228, 233); if (a) c = a >= 64 ? C_RED : blendF(c, C_RED, a * 4); }
  *px = c;
}

void ringRowsBaseline(int y0, int y1, int step) {
  uint16_t *fb = RP.fb;
  const int W = LCD_WIDTH, HW = LCD_WIDTH / 2;
  for (int y = y0; y < y1; y += step) {
    uint16_t *row = fb + y * W;
    int iy = 2 * y + 1 - LCD_HEIGHT;
    long iy2 = (long)iy * iy;
    int ho = ringOutH[y], hi = ringInH[y];
    // 圆外两头：整段清零（黑色 = 0）
    if (HW - ho > 0) memset(row, 0, (HW - ho) * 2);
    if (HW + ho < W) memset(row + HW + ho, 0, (W - HW - ho) * 2);
    // 左边一段 [HW-ho, HW-hi)，右边一段 [HW+hi, HW+ho)
    for (int x = HW - ho; x < HW - hi; x++) ringPixelBaseline(row + x, 2 * x + 1 - W, iy2, iy);
    for (int x = HW + hi; x < HW + ho; x++) ringPixelBaseline(row + x, 2 * x + 1 - W, iy2, iy);
  }
}

void vignetteRowsBaseline(int y0, int y1, int step) {
  uint16_t *fb = VP.fb;
  float r0 = VP.r0, r1 = VP.r1, ri2 = r0 * r0, ro2 = r1 * r1;
  for (int y = y0; y < y1; y += step) {
    float dy = y + 0.5f - CX, dy2 = dy * dy;
    if (dy2 >= ro2) continue;
    float ho = sqrtf(ro2 - dy2), hi = dy2 < ri2 ? sqrtf(ri2 - dy2) : -1;
    uint16_t *row = fb + y * LCD_WIDTH;
    for (int side = 0; side < 2; side++) {
      int xa, xb;
      if (hi < 0) { if (side) break; xa = (int)(CX - ho); xb = (int)(CX + ho); }
      else if (side == 0) { xa = (int)(CX - ho); xb = (int)(CX - hi) + 1; }
      else { xa = (int)(CX + hi); xb = (int)(CX + ho) + 1; }
      xa = max(xa, 0); xb = min(xb, LCD_WIDTH - 1);
      for (int x = xa; x <= xb; x++) {
        float dx = x + 0.5f - CX;
        int k = (int)(dx * dx + dy2) - vigK0;
        if (k < 0 || k >= vigK1 - vigK0) continue;
        uint32_t keep = (uint32_t)vigKeep[k] + 1;
        if (keep >= 256) continue;
        uint32_t c = row[x];
        row[x] = (uint16_t)((((c >> 11) * keep >> 8) << 11) | ((((c >> 5) & 63) * keep >> 8) << 5) | ((c & 31) * keep >> 8));
      }
    }
  }
}

static inline void ringPixelNoCache(uint16_t *px, int ix, long iy2, int iy) {
  long q = (long)ix * ix + iy2;
  int k = (int)((q - RQ_MIN) >> RQ_SH);
  int r64 = rq2r[k < 0 ? 0 : k];
  uint16_t c = *px;
  int a = bandCov64(r64, 211, 240);
  if (a) c = a >= 64 ? C_BLACK : blendF(c, C_BLACK, a * 4);
  if (RP.track) { a = bandCov64(r64, RING_R - 5, RING_R + 5); if (a) c = a >= 64 ? C_TRACK : blendF(c, C_TRACK, a * 4); }
  if (RP.trip >= 0) {                                  // v7.4 行程环：走过的灰，没走的绿
    a = bandCov64(r64, RING_R - TRIP_H, RING_R + TRIP_H);
    if (a) {
      bool right = ix >= 0;
      bool before = (ix * tripUy - iy * tripUx) >= 0;
      bool done = RP.trip >= 0.9999f ? true : RP.trip <= 0.0001f ? false : (RP.trip <= 0.5f ? (right && before) : (right || before));
      uint16_t tc = done ? TRIP_DONE_C : TRIP_LEFT_C;
      c = a >= 64 ? tc : blendF(c, tc, a * 4);
    }
  }
  if (RP.prog > 0.002f) {
    int h = (int)RP.ph;
    a = bandCov64(r64, RING_R - h, RING_R + h);
    if (a) {
      // 在进度范围内吗：正上方顺时针转到末端方向 (ux, uy)
      bool right = ix >= 0;
      bool before = (ix * ringUy - iy * ringUx) >= 0;
      bool in = RP.prog >= 0.9999f ? true : (RP.prog <= 0.5f ? (right && before) : (right || before));
      if (in) c = a >= 64 ? RP.pcol : blendF(c, RP.pcol, a * 4);
    }
  }
  if (RP.over) { a = bandCov64(r64, 228, 233); if (a) c = a >= 64 ? C_RED : blendF(c, C_RED, a * 4); }
  *px = c;
}

void __attribute__((optimize("O3"))) ringRowsNoCache(int y0, int y1, int step) {
  uint16_t *fb = RP.fb;
  const int W = LCD_WIDTH, HW = LCD_WIDTH / 2;
  for (int y = y0; y < y1; y += step) {
    uint16_t *row = fb + y * W;
    int iy = 2 * y + 1 - LCD_HEIGHT;
    long iy2 = (long)iy * iy;
    int ho = ringOutH[y], hi = ringInH[y];
    // 圆外两头：整段清零（黑色 = 0）
    if (HW - ho > 0) memset(row, 0, (HW - ho) * 2);
    if (HW + ho < W) memset(row + HW + ho, 0, (W - HW - ho) * 2);
    // 左边一段 [HW-ho, HW-hi)，右边一段 [HW+hi, HW+ho)
    for (int x = HW - ho; x < HW - hi; x++) ringPixelNoCache(row + x, 2 * x + 1 - W, iy2, iy);
    for (int x = HW + hi; x < HW + ho; x++) ringPixelNoCache(row + x, 2 * x + 1 - W, iy2, iy);
  }
}
