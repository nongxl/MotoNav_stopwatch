#pragma once
// Frozen 7.12.8 raster functions; self-test only.
void __attribute__((optimize("O3"))) rasterRowsBefore(int y0, int y1, int step) {
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
void rasterViceRowsBefore(int y0, int y1, int step) {
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
    float half = sqrtf(212.0f * 212.0f - dyc * dyc);
    int xa = max(0, (int)(CX - half)), xb = min(LCD_WIDTH - 1, (int)(CX + half));
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

void viceAAQuadBefore(const float *px, const float *py, uint16_t color) {
  VicePlane plane[4];
  float cx = 0, cy = 0, ymin = py[0], ymax = py[0];
  for (int i = 0; i < 4; i++) { cx += px[i]; cy += py[i]; ymin = fminf(ymin, py[i]); ymax = fmaxf(ymax, py[i]); }
  cx *= 0.25f; cy *= 0.25f;
  for (int i = 0; i < 4; i++) {
    int j = (i + 1) & 3;
    float dx = px[j] - px[i], dy = py[j] - py[i], length = sqrtf(dx * dx + dy * dy);
    if (length < 0.001f) return;
    float nx = -dy / length, ny = dx / length;
    if ((cx - px[i]) * nx + (cy - py[i]) * ny < 0) { nx = -nx; ny = -ny; }
    plane[i] = {nx, ny, -nx * px[i] - ny * py[i], (fabsf(nx) + fabsf(ny)) * 0.5f, viceCoverageAngle(nx, ny)};
  }
  uint16_t *fb = gfx->getFramebuffer();
  int ya = max(0, (int)floorf(ymin - 1)), yb = min(LCD_HEIGHT - 1, (int)ceilf(ymax + 1));
  for (int y = ya; y <= yb; y++) {
    float sy = y + 0.5f, left = 0.5f, right = LCD_WIDTH - 0.5f;
    float solidLeft = left, solidRight = right;
    bool outside = false, noSolid = false;
    float q[4];
    for (int i = 0; i < 4; i++) {
      q[i] = plane[i].ny * sy + plane[i].c;
      float nx = plane[i].nx, support = plane[i].support;
      if (fabsf(nx) < 0.00001f) {
        if (q[i] <= -support) outside = true;
        if (q[i] < support) noSolid = true;
      } else if (nx > 0) {
        left = fmaxf(left, (-support - q[i]) / nx);
        solidLeft = fmaxf(solidLeft, (support - q[i]) / nx);
      } else {
        right = fminf(right, (-support - q[i]) / nx);
        solidRight = fminf(solidRight, (support - q[i]) / nx);
      }
    }
    if (outside || left > right) continue;
    int xa = max(0, (int)ceilf(left - 0.5f)), xb = min(LCD_WIDTH - 1, (int)floorf(right - 0.5f));
    int ia = max(xa, (int)ceilf(solidLeft - 0.5f)), ib = min(xb, (int)floorf(solidRight - 0.5f));
    if (noSolid || ia > ib) { ia = xb + 1; ib = xb; }
    uint16_t *row = fb + y * LCD_WIDTH;
    for (int x = xa; x <= xb;) {
      if (x == ia && ia <= ib) {
        gfx->drawFastHLine(x, y, ib - ia + 1, color);
        x = ib + 1; continue;
      }
      float sx = x + 0.5f;
      int alpha = 256;
      for (int i = 0; i < 4; i++) {
        float distance = plane[i].nx * sx + q[i];
        if (distance <= -plane[i].support) { alpha = 0; break; }
        if (distance < plane[i].support) {
          int index = max(0, min(256, (int)floorf(128.5f + distance * (256.0f / 1.5f))));
          alpha = min(alpha, (int)viceCoverage[plane[i].angle][index]);
        }
      }
      if (alpha) row[x] = alpha >= 256 ? color : blendF(row[x], color, alpha);
      x++;
    }
  }
}

void viceAACircleBefore(float cx, float cy, float radius, uint16_t color) {
  uint16_t *fb = gfx->getFramebuffer();
  int ya = max(0, (int)floorf(cy - radius - 1)), yb = min(LCD_HEIGHT - 1, (int)ceilf(cy + radius + 1));
  float outer = radius + 0.5f, inner = fmaxf(0, radius - 0.5f);
  for (int y = ya; y <= yb; y++) {
    float dy = y + 0.5f - cy;
    if (fabsf(dy) >= outer) continue;
    float extent = sqrtf(outer * outer - dy * dy);
    int xa = max(0, (int)ceilf(cx - extent - 0.5f)), xb = min(LCD_WIDTH - 1, (int)floorf(cx + extent - 0.5f));
    int ia = xb + 1, ib = xb;
    if (fabsf(dy) < inner) {
      float core = sqrtf(inner * inner - dy * dy);
      ia = max(xa, (int)ceilf(cx - core - 0.5f)); ib = min(xb, (int)floorf(cx + core - 0.5f));
    }
    uint16_t *row = fb + y * LCD_WIDTH;
    for (int x = xa; x <= xb;) {
      if (x == ia && ia <= ib) {
        gfx->drawFastHLine(x, y, ib - ia + 1, color);
        x = ib + 1; continue;
      }
      float dx = x + 0.5f - cx;
      viceBlendPixel(row, x, color, outer - sqrtf(dx * dx + dy * dy));
      x++;
    }
  }
}

void mapPolylineBefore(const float *xs, const float *ys, int n, float w, uint16_t c, bool caps) {
  bool vice = styleIdx() == 1;
  if (vice) initViceSampling();
  float r = w / 2;
  float pdx = 0, pdy = 0;
  bool havePrev = false;
  for (int i = 0; i + 1 < n; i++) {
    float x0 = xs[i], y0 = ys[i], x1 = xs[i + 1], y1 = ys[i + 1];
    if ((x0 < -w && x1 < -w) || (x0 > LCD_WIDTH + w && x1 > LCD_WIDTH + w) ||
        (y0 < -w && y1 < -w) || (y0 > LCD_HEIGHT + w && y1 > LCD_HEIGHT + w)) continue;
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.5f) continue;
    float ux = dx / len, uy = dy / len;
    float nx = -uy * r, ny = ux * r;
    if (vice) {
      float px[4] = {x0 + nx, x0 - nx, x1 - nx, x1 + nx};
      float py[4] = {y0 + ny, y0 - ny, y1 - ny, y1 + ny};
      viceAAQuadBefore(px, py, c);
    } else {
      gfx->fillTriangle(x0 + nx, y0 + ny, x0 - nx, y0 - ny, x1 + nx, y1 + ny, c);
      gfx->fillTriangle(x0 - nx, y0 - ny, x1 - nx, y1 - ny, x1 + nx, y1 + ny, c);
    }
    if (r >= 1.5f) {
      if (havePrev ? (ux * pdx + uy * pdy < 0.97f) : caps) {
        if (vice) viceAACircleBefore(x0, y0, r, c); else gfx->fillCircle(x0, y0, r, c);
      }
      if (caps && i + 2 == n) {
        if (vice) viceAACircleBefore(x1, y1, r, c); else gfx->fillCircle(x1, y1, r, c);
      }
    }
    pdx = ux; pdy = uy; havePrev = true;
  }
}
