#pragma once
// 地图帧组合及帧率统计。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// v5.8 分步计时（微秒累计，每 2 秒发一次）
#define PROF_N 11
uint32_t profAcc[PROF_N];
uint32_t profT = 0;
static inline void profMark(int i) { uint32_t n = micros(); profAcc[i] += n - profT; profT = n; }

void drawMapFrame() {
  unsigned long t = millis();
  lastMapDraw = t;
  frameIsMap = true;
  // 位置：按车速和转弯速度往前推算（v6.7 沿弧线，再加一点提前量），让画面连续移动、少“慢半拍”
  float cx, cy, hPred;
  predictPose(t, leadMs / 1000.0f, cx, cy, hPred);
  viewTargetHdg = hPred;
  if (corrAt) {                                         // v2.6 平滑修正
    float f = fmaxf(0, 1 - (t - corrAt) / 400.0f);
    cx += corrX * f; cy += corrY * f;
  }
  // 朝向平滑转动，缩放跟车速（越快看得越远）
  if (!viewInit) { viewHdg = hPred; viewInit = true; }
  float dh = hPred - viewHdg;
  while (dh > 180) dh -= 360;
  while (dh < -180) dh += 360;
  // v3.7 按时间平滑转动（和帧率无关）：时间常数 0.35 秒，最快每秒转 70 度。
  //      每一帧之间画面变化更小、更均匀，转弯时的撕裂感会轻很多（这块板子没接屏幕的刷新同步信号，
  //      没法从硬件上完全消除撕裂，只能让相邻两帧尽量接近）
  static unsigned long lastViewAt = 0;
  float vdt = lastViewAt ? (t - lastViewAt) / 1000.0f : 0.05f;
  lastViewAt = t;
  if (vdt > 0.25f) vdt = 0.25f;
  float hstep = dh * (1.0f - expf(-vdt / 0.22f));       // v6.7 0.35 → 0.22 秒：追得更紧
  float hmax = fminf(70.0f * vdt, 3.5f);                // v6.0 每帧最多转 3.5°（防撕裂）
  if (hstep > hmax) hstep = hmax;
  if (hstep < -hmax) hstep = -hmax;
  viewHdg += hstep;
  const bool dragon = styleIdx() == ST_DRAGON;
  bool vecOK = !dragon && mapValid && mapFrame == posFrame;
  bool rasOK = !dragon && rasterValid && rasterFrame == posFrame;
  bool useRaster = rasOK && (!vecOK || rasterAt >= vectorAt);   // v2.5 两种都有时用最新收到的
  // 图片地图每像素 2 米，放太大会显得粗糙，所以最大放大倍数小一些
  float targetS = constrain(0.85f - posKmh * 0.006f, 0.38f, useRaster ? 0.6f : 0.85f);
  viewScale += (targetS - viewScale) * (1.0f - expf(-vdt / 1.2f));   // v3.7 缩放也按时间平滑
  float sc = viewScale, hs = viewHdg * PI / 180, cs = cosf(hs), sn = sinf(hs);
  float viewR = (CX + (MAP_PY - CX) + 30) / sc;

  float wk = sc / 0.6f;
  profT = micros();
  if (useRaster) drawRasterMap(cx, cy, sc, cs, sn);     // v2.5 天地图图片地图
  else gfx->fillScreen(C_BLOCK);
  profMark(0);
  // 地图块里的要素（文件里已按：水面、绿地、细路 → 粗路 排好序）
  float lx = cx - mapOx, ly = cy - mapOy;
  const float roadW[6] = {0, 15, 12, 10, 7, 5};
  for (int i = 0; i < ((dragon || useRaster) ? 0 : mapFeatN); i++) {
    const int16_t *bx = featBox + 4 * i;
    if (bx[2] < lx - viewR || bx[0] > lx + viewR || bx[3] < ly - viewR || bx[1] > ly + viewR) continue;
    int np = featNp[i];
    if (np > POLY_PTS) np = POLY_PTS;
    const uint8_t *pp = mapShow + featOff[i];
    for (int k = 0; k < np; k++) {
      float dx = rd16(pp + 4 * k) - lx, dy = rd16(pp + 4 * k + 2) - ly;
      tmpX[k] = CX + sc * (dx * cs - dy * sn);
      tmpY[k] = MAP_PY - sc * (dx * sn + dy * cs);
    }
    uint8_t cls = featCls[i];
    if (cls & 0x80) {
      mapFillPoly(tmpX, tmpY, np, (cls & 0x7F) == 11 ? C_WATER : C_PARK);
    } else {
      int c = cls & 0x7F;
      if (c < 1 || c > 5) c = 5;
      mapPolyline(tmpX, tmpY, np, fmaxf(2.0f, roadW[c] * wk), C_ROADM, false);
    }
  }
  {                                                     // v6.1 风格特效（画在路线下面）
    int st = styleIdx();
    if (st == ST_DRAGON) DragonRadar::drawGrid(gfx, LCD_WIDTH, LCD_HEIGHT, nightOn);
    else if (st == ST_BLUEPRINT) drawBlueprintGrid(cx, cy, sc, cs, sn, viewR);
    else if (st == ST_RADAR) drawRadarSweep();
    else if (st == ST_COMPASS) drawHexGrid();
  }
  profMark(1);
  // 导航路线（带深色描边）；v3.0 车已经走过的部分变灰或不画
  // v7.6 巡航没有路线：上一次导航留下的路线、原路线、路况、红绿灯、终点都不画，道路保持原色
  if (!dragon && !cruiseOn) drawGhost(cx, cy, sc, cs, sn, viewR, fmaxf(3, 5 * wk));   // v4.4 原路线（导入的分享路线）
  if (!cruiseOn && routeN > 1 && routeFrame == posFrame) {
    updateRouteProgress(cx, cy);
    float split = routeProg >= 0 ? routeProg : 0;
    float w0 = fmaxf(6, 13 * wk), w1 = fmaxf(4, 9 * wk);
    if (split > 0 && trailMode == 1)
      drawRouteRange(0, split, cx, cy, sc, cs, sn, viewR, w0 * 0.8f, w1 * 0.75f, TH.done2, TH.done);
    drawRouteRange(split, (float)(routeN - 1), cx, cy, sc, cs, sn, viewR, w0, w1, C_ROUTE2, C_ROUTE);
    if (!dragon && styleIdx()!=ST_MINIMAL && curveOn && routeCurv) drawRouteCurved(split, (float)(routeN - 1), cx, cy, sc, cs, sn, viewR, w1);   // v6.1
    if (!dragon && styleIdx()!=ST_MINIMAL) drawTraffic(cx, cy, sc, cs, sn, viewR, w0, w1, split);                       // v5.0 路况颜色
  }
  profMark(2);
  obsN = 0;                                             // v3.8 占位框从头登记
  if (dragon) {
    bool hudCam = hasCam; hasCam = false; // This style hides the camera badge.
    addHudObstacles(); hasCam = hudCam;
  } else addHudObstacles();
  if (!dragon && !cleanOn && !cruiseOn) drawMapLights(cx, cy, sc, cs, sn);   // v3.2 路线上的红绿灯（v4.9 清爽模式不画；v7.6 巡航不画）
  if (!dragon) drawMapCams(cx, cy, sc, cs, sn);                      // v3.5 电子眼
  if (!cruiseOn) drawDestBlip(cx, cy, sc, cs, sn);      // v2.9 目的地标记（v7.6 巡航没有目的地）
  drawPlayer(CX, MAP_PY);
  profMark(3);
  {
    int st = styleIdx();                                // v6.1 纸地图、蓝图、掌机像素不压暗边缘
    if (st != 0 && st != ST_MINIMAL && st != ST_DRAGON && st != ST_PAPER && st != ST_BLUEPRINT && st != ST_PIXEL) drawVignette(178, 211, 150);   // v2.9 雷达边缘渐暗
  }
  profMark(4);
  if (styleIdx() == ST_COMPASS) drawCompassRose(viewHdg);   // v6.1 罗盘刻度
  else if (!dragon) drawNorth(viewHdg);                              // v2.9 北向标记
  if (!dragon && !cleanOn) drawCamIcons(cx, cy, sc, cs, sn);       // v3.8 电子眼图标（v4.9 清爽模式不画）
  profMark(5);
  unsigned long tMapDone = millis();                   // v2.6 统计：画地图用时
  mapMsAcc += tMapDone - t;

  // 边框和信息
  // v4.9 清爽模式：测速牌、超速红圈、底部行程信息（到达时间/里程/红绿灯数）都不显示
  bool saveCam = hasCam, saveTrip = tripOn;
  if (cleanOn || dragon) { hasCam = false; tripOn = false; }
  bool over = hasCam && camLimit > 0 && speedKmh > camLimit;
  {                                                     // v5.9 外圈一次画完
    bool isNear = navDist <= STAGE_NEAR_M;
    // v7.4 平时外圈是行程环；出现转弯倒计时就换成倒计时环（盖掉行程环）
    float tp = ringActive ? -1 : tripFrac();
    drawRingLayer(navDir != "S" && tp < 0, ringActive ? ringShown : 0, isNear ? 14 : 10, isNear ? C_ORANGE : C_YELLOW, over, tp);
  }
  profMark(6);
  drawMapSpeed(over);
  profMark(7);
  drawMapLanes();                                       // v3.3 车道（车速下面）
  profMark(8);
  drawMapBadge();
  profMark(9);
  hasCam = saveCam; tripOn = saveTrip;
  drawNavHints(300);                                    // v5.0 重新规划中 / 更快路线 / GPS 信号弱
  drawBattery(CX - 16, 18, true);                         // v6.8 电量（车速上方）；v7.2 带黑底
  profMark(10);
  // v2.9 小地图里不画顶部的蓝牙小圆点（会和 N 标记重叠；能收到地图就说明连着）
  needFlush = true;
  mapFrames++;
  mapDrawMsAcc += millis() - t;
  ovlMsAcc += millis() - tMapDone;
}

// v2.2 每 2 秒统计一次帧率，发给手机显示
void updateStats() {
  unsigned long now = millis();
  if (now - statAt < 2000) return;
  float secs = (now - statAt) / 1000.0f;
  if (statAt != 0 && mapFrames > 0) {
    uint32_t fc = flushCnt, fm = flushMsAcc;
    pendingStat = "S," + String(mapFrames / secs, 1) + "," + String(mapDrawMsAcc / mapFrames) + "," +
                  String(fc ? fm / fc : 0) + "," + String(qspiMHz) + "," +
                  String(mapMsAcc / mapFrames) + "," + String(ovlMsAcc / mapFrames) + "," + String(fastDev ? 1 : 0) + "," +
                  String(rasMsAcc / mapFrames) + "," + String(helperTask ? 2 : 1);   // v5.7 底图用时、几个核心在画
    String q = "Q";                                    // v5.8 分步用时（0.1 毫秒/帧）
    for (int i = 0; i < PROF_N; i++) q += "," + String(profAcc[i] / 100 / mapFrames);
    pendingProf = q;
#if FPS_SERIAL_DIAGNOSTICS
    Serial.printf("FPSSTAT %s | %s | deg=%d\n",pendingStat.c_str(),q.c_str(),screenDeg);
#endif
  }
  for (int i = 0; i < PROF_N; i++) profAcc[i] = 0;
  statAt = now; mapFrames = 0; mapDrawMsAcc = 0; mapMsAcc = 0; ovlMsAcc = 0; flushCnt = 0; flushMsAcc = 0; rasMsAcc = 0;
}
