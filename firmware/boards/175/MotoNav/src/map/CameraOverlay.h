#pragma once
// 电子眼图标、避让和位置锁定。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// ---- v3.8 小地图上的“占位框”：先登记已经占用的地方，电子眼图标只放在空位上 ----
#define OBS_MAX 40
int16_t obsX0[OBS_MAX], obsY0[OBS_MAX], obsX1[OBS_MAX], obsY1[OBS_MAX];
int obsN = 0;
void obsAdd(float x0, float y0, float x1, float y1) {
  if (obsN >= OBS_MAX) return;
  obsX0[obsN] = (int16_t)x0; obsY0[obsN] = (int16_t)y0; obsX1[obsN] = (int16_t)x1; obsY1[obsN] = (int16_t)y1;
  obsN++;
}
// 和已占用的地方重叠了多少面积（像素）；from = 从第几个占位框开始算（v4.2）
long obsOverlap(int x0, int y0, int x1, int y1, int from = 0) {
  long sum = 0;
  for (int i = from; i < obsN; i++) {
    int w = min(x1, (int)obsX1[i]) - max(x0, (int)obsX0[i]);
    int h = min(y1, (int)obsY1[i]) - max(y0, (int)obsY0[i]);
    if (w > 0 && h > 0) sum += (long)w * h;
  }
  return sum;
}
bool obsHit(int x0, int y0, int x1, int y1) {
  for (int i = 0; i < obsN; i++)
    if (x0 < obsX1[i] && x1 > obsX0[i] && y0 < obsY1[i] && y1 > obsY0[i]) return true;
  return false;
}

// 本帧在雷达里的电子眼，在屏幕上的实际位置
float camPx[MCAM_MAX], camPy[MCAM_MAX];
bool camShown[MCAM_MAX];

// v3.5 算出电子眼在小地图上的位置（已经开过的 = 在车后面的不画）；v3.8 真正的绘制放到 drawCamIcons()
void drawMapCams(float cx, float cy, float sc, float cs, float sn) {
  for (int i = 0; i < MCAM_MAX; i++) camShown[i] = false;
  if (mapCamN == 0 || mapCamFrame != posFrame) return;
  for (int i = 0; i < mapCamN; i++) {
    float dx = mapCamX[i] - cx, dy = mapCamY[i] - cy;
    float px = CX + sc * (dx * cs - dy * sn), py = MAP_PY - sc * (dx * sn + dy * cs);
    if (py > MAP_PY + 12) continue;                                     // 在车后面：已经过了
    float ex = px - CX, ex2 = py - CX;
    if (ex * ex + ex2 * ex2 > 194.0f * 194.0f) continue;
    camPx[i] = px; camPy[i] = py; camShown[i] = true;
  }
}

#include "../../resources/CameraIconPixels.h"

// 把一张 n×n 的图标（每像素 RGB565 + 透明度）画在 (中心 x, 中心 y)，按透明度和地图混合
void blitRGBA(const uint8_t *src, int n, int cx, int cy) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  int x0 = cx - n / 2, y0 = cy - n / 2;
  for (int j = 0; j < n; j++) {
    int y = y0 + j;
    if (y < 0 || y >= LCD_HEIGHT) continue;
    uint16_t *row = fb + y * LCD_WIDTH;
    const uint8_t *p = src + j * n * 3;
    for (int i = 0; i < n; i++, p += 3) {
      uint8_t a = p[2];
      if (!a) continue;
      int x = x0 + i;
      if (x < 0 || x >= LCD_WIDTH) continue;
      uint16_t c = p[0] | (p[1] << 8);
      row[x] = a >= 250 ? c : blend565(row[x], c, a);
    }
  }
}
void blitCamIcon(int id, int cx, int cy) {
  if (id < 0 || id >= CAMI_N) id = 0;
  blitRGBA(CAM_ICONS[id], CAMI_S, cx, cy);
}

// 画一个电子眼：测速且有限速值 = 红圈限速牌（数字是实时的）；其他 = 对应类型的图标
void drawOneCam(int i, float x, float y) {
  if (mapCamTyp[i] == CAMI_SPEED && mapCamSpd[i] > 0) {
    aaDisc(x, y, 15, C_BLACK);
    aaDisc(x, y, 13.6f, C_SIGNRED);
    aaDisc(x, y, 10.2f, C_WHITE);
    String t = String(mapCamSpd[i]);
    printCentered(t, (int)x + (t.length() == 3 ? 0 : 1), (int)y - 3, 1, C_BLACK);
  } else {
    blitCamIcon(mapCamTyp[i], (int)lroundf(x), (int)lroundf(y));
  }
}

static float angDiff(float a, float b) {
  float d = a - b;
  while (d > 180) d -= 360;
  while (d < -180) d += 360;
  return fabsf(d);
}

// v4.1 世界坐标 (wx, wy) 离导航路线最近多远（米）；ux, uy 返回最近那一段路的方向（单位向量）。
//      只看外框在 maxD 以内的路段组，算得很快；没有路线返回很大的数
float routeDistW(float wx, float wy, float maxD, float &ux, float &uy) {
  float best = 1e9f;
  ux = 0; uy = 1;
  if (routeN < 2 || routeFrame != posFrame) return best;
  for (int bi = 0; bi * RB_STEP < routeN - 1; bi++) {
    const int32_t *bb = rbBox + 4 * bi;
    if (wx < bb[0] - maxD || wx > bb[2] + maxD || wy < bb[1] - maxD || wy > bb[3] + maxD) continue;
    int s0 = bi * RB_STEP, e0 = min(routeN - 1, s0 + RB_STEP);
    for (int k = s0; k < e0; k++) {
      float ax = routeX[k], ay = routeY[k], dx = routeX[k + 1] - ax, dy = routeY[k + 1] - ay;
      float l2 = dx * dx + dy * dy;
      float t = l2 > 0 ? ((wx - ax) * dx + (wy - ay) * dy) / l2 : 0;
      t = constrain(t, 0.0f, 1.0f);
      float ex = ax + t * dx - wx, ey = ay + t * dy - wy;
      float d = sqrtf(ex * ex + ey * ey);
      if (d < best && l2 > 0) { best = d; float l = sqrtf(l2); ux = dx / l; uy = dy / l; }
    }
  }
  return best;
}

// v3.8 → v4.1 画所有电子眼：
//   1. 路上（电子眼的实际位置）画一个蓝色小摄像头；挨得很近的几个只画一个
//   2. 类型图标（测速牌、闯红灯、违章拍照……）不放在路上，而是摆在小摄像头的旁边：
//      优先垂直于道路的两侧，图标之间、图标和路线/红绿灯/车速等都不重叠；普通监控只有路上的小摄像头
//   3. 位置选好就锁住不动；转了弯（地图朝向和锁定时差 15 度以上）并且已经转稳了，才重新选一次
void drawCamIcons(float cx, float cy, float sc, float cs, float sn) {
  bool turning = angDiff(viewTargetHdg, viewHdg) > 4 || fabsf(hdgRate) > 8;   // 地图还在跟着转（v6.7 含正在转弯）
  const float MKR = 10, R = 15;                                           // 小摄像头半径、类型图标半径
  const float D = MKR + 3 + R;                                            // 类型图标离小摄像头中心的距离（边缘相隔 3 像素）
  // ---- 1. 路上的小摄像头 ----
  float mkX[MCAM_MAX], mkY[MCAM_MAX];
  int mkN = 0, camMk[MCAM_MAX];
  for (int i = 0; i < mapCamN && i < MCAM_MAX; i++) {
    camMk[i] = -1;
    if (!camShown[i]) continue;
    for (int q = 0; q < mkN; q++)
      if (fabsf(mkX[q] - camPx[i]) < 9 && fabsf(mkY[q] - camPy[i]) < 9) { camMk[i] = q; break; }
    if (camMk[i] < 0) { mkX[mkN] = camPx[i]; mkY[mkN] = camPy[i]; camMk[i] = mkN++; }
  }
  // v4.7 小摄像头和类型图标都是圆的：彼此之间按圆来判断重叠（以前按方框算，斜着放时会误判成“挡住了”，
  //      结果图标被推到远处、拉一根长线）
  float ciX[3 * MCAM_MAX], ciY[3 * MCAM_MAX], ciR[3 * MCAM_MAX];
  int ciN = 0;
  for (int q = 0; q < mkN; q++) {
    bool cam = false;                                                    // v4.8 这一处有没有电子眼（道路安全提示不算）
    for (int i = 0; i < mapCamN && i < MCAM_MAX; i++) if (camMk[i] == q && mapCamTyp[i] < 16) cam = true;
    if (cam) {
      blitRGBA(CAM_MARK, CAMM_S, (int)lroundf(mkX[q]), (int)lroundf(mkY[q]));
      ciX[ciN] = mkX[q]; ciY[ciN] = mkY[q]; ciR[ciN] = MKR; ciN++;
    } else {                                                             // 只有安全提示：路上画个小黄点
      aaDisc(mkX[q], mkY[q], 4.5f, C_BLACK);
      aaDisc(mkX[q], mkY[q], 3, 0xFE40);
      ciX[ciN] = mkX[q]; ciY[ciN] = mkY[q]; ciR[ciN] = 5; ciN++;
    }
  }
  // ---- 2. 旁边的类型图标：v4.3 同一处的几个排成一列（和道路平行），整列放在道路的一侧 ----
  int clN[MCAM_MAX], clCam[MCAM_MAX][MCAM_MAX];
  for (int q = 0; q < mkN; q++) clN[q] = 0;
  for (int i = 0; i < mapCamN && i < MCAM_MAX; i++) {
    if (camMk[i] < 0) continue;
    int t = mapCamTyp[i];
    if (t == 0 || t == 1) continue;                                      // 普通监控：路上的小摄像头就够了
    int q = camMk[i];
    bool dup = false;                                                    // 同一处同一种只标一次
    for (int j = 0; j < clN[q]; j++) if (mapCamTyp[clCam[q][j]] == t) { dup = true; break; }
    if (!dup) clCam[q][clN[q]++] = i;
  }
  // 每一列里按类型排序，顺序固定
  for (int q = 0; q < mkN; q++)
    for (int a = 0; a < clN[q]; a++)
      for (int b = a + 1; b < clN[q]; b++)
        if (mapCamTyp[clCam[q][b]] < mapCamTyp[clCam[q][a]]) { int t = clCam[q][a]; clCam[q][a] = clCam[q][b]; clCam[q][b] = t; }
  // 锁住：这一处的每个电子眼都放过、个数没变、地图没转过弯
  bool keep[MCAM_MAX];
  int order[MCAM_MAX], n = 0;
  for (int q = 0; q < mkN; q++) {
    if (clN[q] == 0) continue;
    bool k = true;
    for (int j = 0; j < clN[q]; j++) {
      int i = clCam[q][j];
      if (mapCamPref[i] <= 0 || mapCamPrefK[i] != clN[q] || !(turning || angDiff(viewHdg, mapCamLockH[i]) < 15)) k = false;
    }
    keep[q] = k;
    order[n++] = q;
  }
  for (int a = 0; a < n; a++)                                            // 锁住的在前，再按离车头的距离
    for (int b = a + 1; b < n; b++) {
      int qa = order[a], qb = order[b];
      float da = fabsf(mkX[qa] - CX) + fabsf(mkY[qa] - MAP_PY) + (keep[qa] ? 0 : 10000);
      float db = fabsf(mkX[qb] - CX) + fabsf(mkY[qb] - MAP_PY) + (keep[qb] ? 0 : 10000);
      if (db < da) { order[a] = qb; order[b] = qa; }
    }
  const float GAP = 2 * R + 3;                                           // 一列里相邻两个图标的间距
  for (int o = 0; o < n; o++) {
    int q = order[o], cnt = clN[q], i0 = clCam[q][0];
    float ax = mkX[q], ay = mkY[q];
    float A = mapCamPrefA[i0];
    int lvl = mapCamPref[i0];
    if (!keep[q]) {
      // 道路在屏幕上的方向（没有路线就当作竖直向上）
      float sdx = 0, sdy = -1, ux, uy;
      if (routeDistW(mapCamX[i0], mapCamY[i0], 60, ux, uy) < 60) { sdx = ux * cs - uy * sn; sdy = -(ux * sn + uy * cs); }
      float perp = atan2f(sdy, sdx) + PI / 2;
      // 候选：道路右侧/左侧，先贴近，再远一列
      long bestScore = 0x7FFFFFFF; float bestA = perp; int bestL = 1;
      for (int c = 0; c < 4; c++) {
        float cA = perp + ((c & 1) ? PI : 0);
        int cL = c < 2 ? 1 : 2;
        float d = cL == 1 ? D : D + GAP;
        float px = cosf(cA), py = sinf(cA), tx = -py, ty = px;
        long score = 0;
        for (int j = 0; j < cnt; j++) {
          float off = (j - (cnt - 1) / 2.0f) * GAP;
          float x = ax + px * d + tx * off, y = ay + py * d + ty * off;
          float ex = x - CX, ey = y - CX;
          if (ex * ex + ey * ey > 186.0f * 186.0f) score += 100000;      // 要整个在雷达里
          score += obsOverlap((int)(x - R - 2), (int)(y - R - 2), (int)(x + R + 2), (int)(y + R + 2));
          for (int m = 0; m < ciN; m++) {                                // 和小摄像头、别的图标：按圆算
            float ddx = x - ciX[m], ddy = y - ciY[m], need = R + ciR[m] + 2;
            float dd2 = ddx * ddx + ddy * ddy;
            if (dd2 < need * need) score += (long)((need - sqrtf(dd2)) * 60) + 1;
          }
          float sx = x - CX, sy = MAP_PY - y, rux, ruy;
          float wx = cx + (sx * cs + sy * sn) / sc, wy = cy + (-sx * sn + sy * cs) / sc;
          if (routeDistW(wx, wy, (R + 4) / sc, rux, ruy) < (R + 4) / sc) score += 600;   // 别压在路线上
        }
        if (score < bestScore) { bestScore = score; bestA = cA; bestL = cL; }
        if (score == 0) break;
      }
      A = bestA; lvl = bestL;
      for (int j = 0; j < cnt; j++) {                                    // 整列一起锁住
        int i = clCam[q][j];
        mapCamPref[i] = lvl; mapCamPrefA[i] = A; mapCamPrefK[i] = cnt; mapCamLockH[i] = viewHdg;
      }
    }
    float d = lvl == 2 ? D + GAP : D;
    float px = cosf(A), py = sinf(A), tx = -py, ty = px;
    if (lvl == 2) {                                                      // 远一列时，细线连到小摄像头
      gfx->drawLine((int)(ax + px * (MKR + 1)), (int)(ay + py * (MKR + 1)), (int)(ax + px * (d - R)), (int)(ay + py * (d - R)), C_BLACK);
    }
    for (int j = 0; j < cnt; j++) {
      float off = (j - (cnt - 1) / 2.0f) * GAP;
      float x = ax + px * d + tx * off, y = ay + py * d + ty * off;
      drawOneCam(clCam[q][j], x, y);
      if (ciN < 3 * MCAM_MAX) { ciX[ciN] = x; ciY[ciN] = y; ciR[ciN] = R; ciN++; }
    }
  }
}
