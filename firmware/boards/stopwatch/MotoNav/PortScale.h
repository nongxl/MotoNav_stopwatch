#pragma once
#include <stdint.h>
#include "pin_config.h"

/**
 * PortScale: 逻辑画布 (466x466) 到 M5Stack Stopwatch 物理面板 (PANEL_WIDTH x PANEL_HEIGHT)
 * 的高精度双线性缩放与触摸坐标反向映射。
 */
class PortScale {
public:
  static constexpr int LOGICAL = LCD_WIDTH;       // 466
  static constexpr int PHYSICAL_W = PANEL_WIDTH;  // 240
  static constexpr int PHYSICAL_H = PANEL_HEIGHT; // 240

  int32_t axisX[PHYSICAL_W];
  int32_t axisY[PHYSICAL_H];

  PortScale() {
    for (int p = 0; p < PHYSICAL_W; p++) {
      axisX[p] = (int32_t)(((int64_t)(2 * p + 1) * LOGICAL * 32768) / PHYSICAL_W - 32768);
    }
    for (int p = 0; p < PHYSICAL_H; p++) {
      axisY[p] = (int32_t)(((int64_t)(2 * p + 1) * LOGICAL * 32768) / PHYSICAL_H - 32768);
    }
  }

  // 物理触摸坐标映射回 466 逻辑坐标
  static int touchX(int raw) {
    if (raw < 0) raw = 0;
    if (raw >= PHYSICAL_W) raw = PHYSICAL_W - 1;
#if BOARD_TOUCH_FLIP_X
    raw = PHYSICAL_W - 1 - raw;
#endif
    return (int)(((int64_t)(2 * raw + 1) * LOGICAL) / (2 * PHYSICAL_W));
  }

  static int touchY(int raw) {
    if (raw < 0) raw = 0;
    if (raw >= PHYSICAL_H) raw = PHYSICAL_H - 1;
#if BOARD_TOUCH_FLIP_Y
    raw = PHYSICAL_H - 1 - raw;
#endif
    return (int)(((int64_t)(2 * raw + 1) * LOGICAL) / (2 * PHYSICAL_H));
  }

  static inline uint32_t spread(uint16_t c) {
    return ((uint32_t)c | ((uint32_t)c << 16)) & 0x07E0F81Fu;
  }

  // 双线性插值抗锯齿像素采样
  uint16_t pixel(const uint16_t* source, int x, int y, int rotation, bool freeRotation, int32_t cosine, int32_t sine) const {
    int32_t fx = axisX[x];
    int32_t fy = axisY[y];
    constexpr int32_t center = (LOGICAL - 1) * 32768;

    if (freeRotation) {
      int32_t dx = fx - center, dy = fy - center;
      fx = (int32_t)(((int64_t)dx * cosine + (int64_t)dy * sine) >> 16) + center;
      fy = (int32_t)((-(int64_t)dx * sine + (int64_t)dy * cosine) >> 16) + center;
    } else if (rotation == 1) {
      int32_t a = fx; fx = fy; fy = 2 * center - a;
    } else if (rotation == 2) {
      fx = 2 * center - fx; fy = 2 * center - fy;
    } else if (rotation == 3) {
      int32_t a = fx; fx = 2 * center - fy; fy = a;
    }

    int sx = fx >> 16, sy = fy >> 16;
    if ((unsigned)sx >= LOGICAL - 1 || (unsigned)sy >= LOGICAL - 1) {
      int nx = (fx + 32768) >> 16, ny = (fy + 32768) >> 16;
      return ((unsigned)nx < LOGICAL && (unsigned)ny < LOGICAL) ? source[ny * LOGICAL + nx] : 0;
    }

    const uint16_t* a = source + sy * LOGICAL + sx;
    uint32_t ax = (fx >> 11) & 31, ay = (fy >> 11) & 31;
    uint32_t top = ((spread(a[0]) * (32 - ax) + spread(a[1]) * ax) >> 5) & 0x07E0F81Fu;
    uint32_t bottom = ((spread(a[LOGICAL]) * (32 - ax) + spread(a[LOGICAL + 1]) * ax) >> 5) & 0x07E0F81Fu;
    uint32_t result = ((top * (32 - ay) + bottom * ay) >> 5) & 0x07E0F81Fu;
    return (uint16_t)(result | (result >> 16));
  }
};
