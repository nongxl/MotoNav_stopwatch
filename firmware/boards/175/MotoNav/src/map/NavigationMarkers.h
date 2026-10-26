#pragma once
// 信号灯标记、转向提示、北向和目的地。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// v3.2 红绿灯直接画在地图上的实际位置（图标始终竖直）；已经开过的不画；
//      下一个红绿灯在 300 米内时，旁边标出距离
void drawMapLights(float cx, float cy, float sc, float cs, float sn) {
  if (lightN == 0 || routeFrame != posFrame) return;
  bool first = true;
  for (int i = 0; i < lightN; i++) {
    if (routeProg >= 0 && lightU[i] <= routeProg + 0.02f) continue;     // 已经过了
    float dx = lightX[i] - cx, dy = lightY[i] - cy;
    float px = CX + sc * (dx * cs - dy * sn), py = MAP_PY - sc * (dx * sn + dy * cs);
    bool isNext = first;
    first = false;
    float ex = px - CX, ey = py - CX;
    if (ex * ex + ey * ey > 196.0f * 196.0f) continue;                  // 雷达外面不画
    drawTrafficLight(px, py - 17, 30);
    obsAdd(px - 9, py - 34, px + 9, py);                                // v3.8 电子眼图标别压住红绿灯
    if (isNext && lightDist >= 0) {
      String t = String(lightDist) + "m";
      printOutlined(t, (int)px + 36, (int)py - 25, 2, C_WHITE);
      float tw = t.length() * 12;
      obsAdd(px + 36 - tw / 2 - 3, py - 27, px + 36 + tw / 2 + 3, py - 7);
    }
  }
}

// 转向箭头 + 距离（浮在底部）；离路口 150 米内变大，车道放在半透明深色底上
void drawMapBadge() {
  if (cruiseOn) {                                      // v4.8 巡航：底部只写个“巡航”标记
    printOutlined("CRUISE", CX, 392, 2, C_CYAN);
    return;
  }
  bool isS = navDir == "S";
  bool far = !isS && navDist > STAGE_FAR_M;           // v2.4 远处：先显示直行
  bool nearJ = !isS && navDist <= MAP_NEAR_M;
  bool veryNear = !isS && navDist <= STAGE_NEAR_M;
  uint16_t col = veryNear ? C_ORANGE : C_WHITE;
  String dt = distText(navDist);
  iconHalo = true;
  if (!nearJ) {
    int up = tripOn ? 24 : 0;                          // v3.1 有行程信息时整体上移，下面留一行
    drawBigIconAt((isS || far) ? String("S") : navDir, C_WHITE, 395 - up, 1.0f, CX - 48, 368 - up, 418 - up, 60);
    int sz = dt.length() <= 4 ? 4 : 3;
    printOutlined(dt, CX + 46, (sz == 4 ? 378 : 382) - up, sz, C_WHITE);
    if (tripOn) drawTripLine(404);
  } else {
    const int top = 344;                               // v3.3 车道移到了上面，底部只放箭头和距离
    drawBigIconAt(navDir, col, top + 30, 1.0f, CX - 58, top, top + 60, 84);
    printOutlined(dt, CX + 54, top + 14, dt.length() <= 4 ? 5 : 4, col);
  }
  iconHalo = false;
}

// ---- v2.9 GTA 雷达元素 ----

// 雷达边缘渐暗：半径 r0~r1 之间越往外越暗（最多保留 maxDark/255 的亮度损失）
// v5.7 渐暗查表：每项 = 像素位置 << 8 | 保留亮度（0~255）
uint32_t *vigLut = nullptr;
int vigN = 0;
float vigR0 = -1, vigR1 = -1; int vigMax = -1;

bool buildVignette(float r0, float r1, int maxDark) {
  int cap = (int)(PI * (r1 * r1 - r0 * r0)) + 16 * (int)r1 + 64;
  if (!vigLut) vigLut = (uint32_t *)ps_malloc(sizeof(uint32_t) * cap);
  if (!vigLut) return false;
  vigN = 0;
  float ro2 = r1 * r1;
  for (int y = (int)(CX - r1); y <= (int)(CX + r1); y++) {
    if (y < 0 || y >= LCD_HEIGHT) continue;
    float dy = y + 0.5f - CX, dy2 = dy * dy;
    if (dy2 >= ro2) continue;
    for (int x = max(0, (int)(CX - r1) - 1); x <= min(LCD_WIDTH - 1, (int)(CX + r1) + 1); x++) {
      float dx = x + 0.5f - CX;
      float r = sqrtf(dx * dx + dy2);
      if (r < r0 || r > r1) continue;
      float k = (r - r0) / (r1 - r0);
      int dark = (int)(k * k * maxDark);
      if (dark <= 0 || vigN >= cap) continue;
      vigLut[vigN++] = ((uint32_t)(y * LCD_WIDTH + x) << 8) | (uint32_t)(255 - dark);
    }
  }
  vigR0 = r0; vigR1 = r1; vigMax = maxDark;
  return true;
}

void radarVignette(float r0, float r1, int maxDark) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  if ((vigR0 == r0 && vigR1 == r1 && vigMax == maxDark) || buildVignette(r0, r1, maxDark)) {
    for (int i = 0; i < vigN; i++) {                     // 查表：位置 + 保留多少亮度
      uint32_t e = vigLut[i];
      uint16_t &px = fb[e >> 8];
      uint32_t k = (e & 0xFF) + 1, c = px;
      px = (uint16_t)((((c >> 11) * k >> 8) << 11) | ((((c >> 5) & 63) * k >> 8) << 5) | ((c & 31) * k >> 8));
    }
    return;
  }
  // 内存不够时的老办法
  float ro2 = r1 * r1, ri2 = r0 * r0;
  for (int y = (int)(CX - r1); y <= (int)(CX + r1); y++) {
    if (y < 0 || y >= LCD_HEIGHT) continue;
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
        float r = sqrtf(dx * dx + dy2);
        if (r < r0 || r > r1) continue;
        float k = (r - r0) / (r1 - r0);
        int dark = (int)(k * k * maxDark);
        if (dark > 0) row[x] = blend565(row[x], C_BLACK, dark);
      }
    }
  }
}

// 北向标记：雷达边缘的黑底白色 “N”，位置随车头方向转动
void drawNorth(float hdgDeg) {
  float a = hdgDeg * PI / 180;
  float nx = CX - sinf(a) * 196, ny = CX - cosf(a) * 196;
  obsAdd(nx - 14, ny - 14, nx + 14, ny + 14);                          // v3.8
  aaDisc(nx, ny, 13, C_BLACK);
  aaDisc(nx, ny, 11, 0x2945);
  printCentered("N", (int)nx + 1, (int)ny - 7, 2, C_WHITE);
}

// 目的地标记：在雷达里就画在实际位置，在外面就贴着边缘指出方向（和游戏里的标记一样）
void drawDestBlip(float cx, float cy, float sc, float cs, float sn) {
  if (routeN < 2 || routeFrame != posFrame) return;
  float dx = routeX[routeN - 1] - cx, dy = routeY[routeN - 1] - cy;
  float sx = sc * (dx * cs - dy * sn), sy = -sc * (dx * sn + dy * cs);   // 相对车头位置
  float px = CX + sx, py = MAP_PY + sy;
  float ex = px - CX, ey = py - CX, er = sqrtf(ex * ex + ey * ey);
  const float RMAX = styleIdx() == ST_DRAGON ? 183 : 192;
  bool outside = er > RMAX;
  if (outside) { px = CX + ex / er * RMAX; py = CX + ey / er * RMAX; }
  if (styleIdx() == ST_DRAGON) {
    if (outside) DragonRadar::placeEdgeBall(px, py, CX, RMAX,
      [](float x0, float y0, float x1, float y1) { return obsOverlap((int)x0, (int)y0, (int)x1, (int)y1); });
    DragonRadar::drawBall(gfx, (int)px, (int)py);
    obsAdd(px - 21, py - 21, px + 21, py + 21); return;
  }
  if (styleIdx() == ST_COMPASS) { drawCompassBlip(px, py, outside); obsAdd(px - 14, py - 14, px + 14, py + 14); return; }
  aaDisc(px, py, outside ? 9 : 11, C_BLACK);
  aaDisc(px, py, outside ? 7 : 9, TH.blip);
  obsAdd(px - 12, py - 12, px + 12, py + 12);                         // v3.8
  if (!outside) aaDisc(px, py, 3, C_BLACK);
}

// v5.0 路程（米）→ 路线上的位置（段号 + 段内比例）
float routeUAt(float m) {
  if (!routeCum || routeN < 2) return 0;
  if (m <= 0) return 0;
  if (m >= routeCum[routeN - 1]) return (float)(routeN - 1);
  int lo = 0, hi = routeN - 1;
  while (hi - lo > 1) { int mid = (lo + hi) / 2; if (routeCum[mid] <= m) lo = mid; else hi = mid; }
  float seg = routeCum[hi] - routeCum[lo];
  return lo + (seg > 0 ? (m - routeCum[lo]) / seg : 0);
}

// v5.0 路况颜色：缓行黄、拥堵红、严重拥堵深红，盖在导航路线上（已经走过的部分不画）
void drawTraffic(float cx, float cy, float sc, float cs, float sn, float viewR, float w0, float w1, float from) {
  if (trafN == 0 || routeN < 2 || routeFrame != posFrame) return;
  for (int i = 0; i < trafN; i++) {
    float a = fmaxf(routeUAt(trafA[i]), from), b = routeUAt(trafB[i]);
    if (b <= a) continue;
    uint16_t c = trafS[i] >= 4 ? 0x9000 : trafS[i] == 3 ? 0xE8E4 : 0xFDA0;
    drawRouteRange(a, b, cx, cy, sc, cs, sn, viewR, w0, w1, C_ROUTE2, c);
  }
}

// v5.0 地图上方/下方的提示条：半透明深色底 + 文字
void drawBanner(const String &t, int y, uint16_t col) {
  int w = t.length() * 12 + 24;
  dimRect(CX - w / 2, y - 6, w, 28, 13, 60);
  printCentered(t, CX, y, 2, col);
}

// v5.0 重新规划中 / 更快路线 / GPS 信号弱（地图模式和大箭头模式都用）
void drawNavHints(int bannerY) {
  unsigned long now = millis();
  if (rerouteAt && now - rerouteAt > 20000) rerouteAt = 0;        // 最多显示 20 秒
  if (fasterAt && now - fasterAt > 30000) fasterAt = 0;           // 更快路线提示 30 秒
  if (rerouteAt) {
    String dots = String("...").substring(0, 1 + (now / 400) % 3);
    drawBanner("REROUTING" + dots, bannerY, C_ORANGE);
  } else if (fasterAt) {
    drawBanner("FASTER ROUTE  -" + String(fasterMin) + "min", bannerY, C_GREEN);
  }
  if (gpsWeak) {                                                  // 左上角：橙色 “GPS” 加一条斜杠
    const int gx = CX - 128, gy = 150;
    aaDisc(gx, gy, 19, C_BLACK);
    aaDisc(gx, gy, 17, 0x4208);
    printCentered("GPS", gx + 1, gy - 7, 1, C_ORANGE);
    printCentered("GPS", gx, gy - 7, 1, C_ORANGE);
    gfx->drawLine(gx - 11, gy + 9, gx + 11, gy - 9, C_RED);
    gfx->drawLine(gx - 11, gy + 10, gx + 11, gy - 8, C_RED);
  }
}

// v3.8 顶部车速/测速牌/车道、底部箭头和行程信息、车头箭头占的地方（这些后画，电子眼图标要提前让开）
void addHudObstacles() {
  obsAdd(CX - 15, MAP_PY - 18, CX + 15, MAP_PY + 16);                 // 车头箭头
  if (speedKmh >= 0) {
    int sx = hasCam ? CX - 40 : CX;
    obsAdd(sx - 60, 36, sx + 60, 92);                                   // 车速数字 + km/h
    if (hasCam) obsAdd(CX + 52 - 34, 30, CX + 52 + 34, 98);             // 测速牌 + 距离
  }
  if (laneN > 0 && ringActive) {                                    // 与倒计时及 drawMapLanes 同步
    const int H = 30, MAXW = 210;
    int lw = (int)(fminf(36.0f * H / 32, (float)MAXW / laneN) * laneN);
    int y = hasCam ? 114 : 100;
    obsAdd(CX - lw / 2 - 9, y - 5, CX + lw / 2 + 9, y + H + 5);
  }
  obsAdd(0, 340, LCD_WIDTH, LCD_HEIGHT);                                // 底部箭头、距离、行程信息
}
