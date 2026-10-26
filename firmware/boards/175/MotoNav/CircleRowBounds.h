#pragma once
#include <stdint.h>
#include <math.h>

// Exact original pixel-center/truncation rules, computed once before drawing tasks start.
template<int Width, int Height> struct CircleRowBounds {
  int16_t left[Height], right[Height];
  void init(float center, float radius) {
    for (int y=0;y<Height;y++) {
      float dy=y+0.5f-center;
      if (fabsf(dy)>=radius) { left[y]=1;right[y]=0;continue; }
      float half=sqrtf(radius*radius-dy*dy);
      int a=(int)(center-half),b=(int)(center+half);
      left[y]=(int16_t)(a<0?0:a);
      right[y]=(int16_t)(b>=Width?Width-1:b);
    }
  }
};
