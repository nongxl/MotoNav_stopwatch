#pragma once
// Frozen 7.12.8 reference, only included in on-board self-test.
#ifndef RENDER_BASELINE_POLYLINE
#define RENDER_BASELINE_POLYLINE mapPolyline
#endif
void drawRouteRangeBaseline(float a, float b, float cx, float cy, float sc, float cs, float sn, float viewR,
                    float w0, float w1, uint16_t c0, uint16_t c1) {
  if (b - a < 0.001f) return;
  for (int pass = 0; pass < 2; pass++) {
    for (int bi = 0; bi * RB_STEP < routeN - 1; bi++) {
      int s = bi * RB_STEP, e = min(routeN - 1, s + RB_STEP);
      if (e <= a || s >= b) continue;
      const int32_t *bb = rbBox + 4 * bi;
      if (bb[2] < cx - viewR || bb[0] > cx + viewR || bb[3] < cy - viewR || bb[1] > cy + viewR) continue;
      float ua = fmaxf(a, (float)s), ub = fminf(b, (float)e);
      int m = 0;
      float x, y;
      routePt(ua, x, y);
      tmpX[m] = CX + sc * ((x - cx) * cs - (y - cy) * sn); tmpY[m] = MAP_PY - sc * ((x - cx) * sn + (y - cy) * cs); m++;
      for (int k = (int)floorf(ua) + 1; k < ub; k++) {
        float dx = routeX[k] - cx, dy = routeY[k] - cy;
        tmpX[m] = CX + sc * (dx * cs - dy * sn); tmpY[m] = MAP_PY - sc * (dx * sn + dy * cs); m++;
      }
      routePt(ub, x, y);
      tmpX[m] = CX + sc * ((x - cx) * cs - (y - cy) * sn); tmpY[m] = MAP_PY - sc * ((x - cx) * sn + (y - cy) * cs); m++;
      if (m >= 2) RENDER_BASELINE_POLYLINE(tmpX, tmpY, m, pass == 0 ? w0 : w1, pass == 0 ? c0 : c1, true);
    }
  }
}
