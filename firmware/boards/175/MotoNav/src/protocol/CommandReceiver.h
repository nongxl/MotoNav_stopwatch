#pragma once
// 导航状态重置、文字命令和总消息入口。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
void resetNavState() {
  trafficLightFrame.clear();trafficLightLab.clear();
  if (idlePowerOffNavigationActive(true, inNav, cruiseOn, navDir == "A"))
    idlePowerOffReset(&idlePowerOffTimer, millis());
  inNav = false;
  tripOn = false; lightDist = -1;                       // v3.1
  mapCamN = 0;                                          // v3.5
  ghostN = 0;                                           // v4.4
  cruiseOn = false;                                     // v4.8
  trafN = 0; rerouteAt = 0; fasterAt = 0; gpsWeak = false;   // v5.0
  speedKmh = -1;
  hasCam = false;
  laneN = 0;
  ringActive = false; ringDir = "";
  for (int i = 0; i < 3; i++) { imgs[i].showW = 0; imgs[i].showH = 0; imgs[i].recvId = -1; }   // v5.3 天气栏 3、4 留着
}

int hexVal(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return 0xF;
}

// 处理文字类消息
void handleText(const String &m) {
  if(m.startsWith("#TLA,")) {
    const int previous=trafficLightFrame.key;
    const bool accepted=trafficLightFrame.receive(m.c_str(),millis());
    if(!accepted || !trafficLightFrame.valid(millis()) || previous!=trafficLightFrame.key)trafficLightLab.clear();
    return;
  }
  if(m=="#TLP,1") {
    trafficLightPreviewUntil=millis()+5000;
    Serial.println("TL LAYOUT PREVIEW placeholders-only duration=5000");
    return;
  }
  if (m.startsWith("#TL,") || m.startsWith("#TLS,")) {
    Serial.printf("TL WIRE at=%lu %s\n",millis(),m.c_str());
    bool accepted=trafficLightLab.receive(m.c_str(), millis());
    Serial.printf("TL JUNCTION RX accepted=%d source=%d count=%u left=%d straight=%d right=%d other=%d\n",
      accepted,trafficLightLab.source,trafficLightLab.count(millis()),trafficLightLab.phases[0].valid(millis()),
      trafficLightLab.phases[1].valid(millis()),trafficLightLab.phases[2].valid(millis()),trafficLightLab.phases[3].valid(millis()));
    return;
  }
  if (m.startsWith("#LY,")) {
    unsigned long gen = 0; unsigned title = 0, artist = 0;
    if (sscanf(m.c_str(), "#LY,%lu,%u,%u", &gen, &title, &artist) == 3 && gen == appleMedia.generation &&
        title == appleMedia.values[0].revision && artist == appleMedia.values[1].revision) {
      lyricImageGeneration = gen; lyricImageTitleRevision = title; lyricImageArtistRevision = artist;
    } else { lyricImageGeneration = 0; imgs[12].showW = 0; imgs[12].showH = 0; imgs[12].recvId = -1; }
    return;
  }
  if (m == "#ML,0" || m == "#ML,1") {
    bool wasEnabled = musicLyrics;
    musicLyrics = m == "#ML,1";
    if (!musicLyrics) {
      lyricImageGeneration = 0; imgs[12].showW = 0; imgs[12].showH = 0; imgs[12].recvId = -1; imgs[12].recvMask = 0; imgs[12].recvParts = 0;
      // Older companion renderers may have put lyrics in the artist bitmap.
      // Ask for a fresh title/artist once per connection or on an actual OFF transition.
      static uint32_t refreshedEpoch = 0xffffffff;
      if (wasEnabled || refreshedEpoch != mediaConnectionEpoch) {
        refreshedEpoch = mediaConnectionEpoch;
        imgs[6].showW = 0; imgs[6].showH = 0; imgs[6].recvId = -1; pendingEvt = "T,M,Q";
      }
    }
    pendingLyricsStatus = String("ML,") + (musicLyrics ? 1 : 0);
    Serial.printf("MEDIA LYRICS enabled=%d\n", musicLyrics);
    musicDirty = true; return;
  }
  if (m == "#HM,0" || m == "#HM,1") {
    bool enabled = m == "#HM,1";
    if (enabled != hidMusicMode) {
      hidMusicMode = enabled; hidPulses.cancelQueued();
      Preferences pr; pr.begin("motonav", false); pr.putBool("mediahid", enabled); pr.end();
    }
    if (!hidMusicMode) { musicLyrics = false; imgs[12].showW = 0; imgs[12].showH = 0; imgs[12].recvId = -1; }
    musicAck = 0; musicAckUntil = 0; musicDirty = true;
    pendingHidStatus = String("HID,") + (hidMusicMode ? 1 : 0) + "," + (hidMediaReady() ? 1 : 0);
    return;
  }
  // A dedicated command: no navigation state or screen invalidation.
  if (m.startsWith("#BOOT,")) {
    int id = BootAnimation::parse(m.c_str() + 6);
    if (id < 0) return;
    bool saved = true;
    if (id != bootBrand) {
      Preferences pr;
      saved = pr.begin("motonav", false);
      if (saved) {
        saved = pr.putInt("bootbrand", id) == sizeof(int32_t);
        pr.end();
      }
      if (saved) bootBrand = id;
    }
    pendingBootAck = "BOOT," + String(saved ? bootBrand : -1);
    return;
  }
  if (m == "#AMS?") { appleMedia.resend(); return; }
  if (m == "#GB?") { pendingBuildingCapability=true; return; }
  if (m.startsWith("#ZACK,")) {
    if (hidMusicMode) return;
    musicAck = constrain(m.substring(6).toInt(), 0, 5);
    musicAckUntil = millis() + 2500; musicDirty = true; return;
  }
  if (m.startsWith("#Z,")) {
    int have = 0, play = 0, actions = 0;
    if (sscanf(m.c_str(), "#Z,%d,%d,%d", &have, &play, &actions) == 3) {
      if (!hidMusicMode) { musicHave = have != 0; musicPlaying = play != 0; musicActions = actions & 7; } musicDirty = true;
    }
    return;
  }
  if (m == "P") return;                                 // 心跳
  if (m == "C") { stopArriveFx(); progReset(); resetNavState(); drawWaiting(WAIT_READY); return; }

  if (m.startsWith("#v,")) { speedKmh = m.substring(3).toInt(); navDirty = true; return; }
  if (m.startsWith("#k")) {
    int c1 = m.indexOf(','), c2 = m.indexOf(',', c1 + 1);
    if (c1 > 0 && c2 > c1) {
      hasCam = true;
      camLimit = m.substring(c1 + 1, c2).toInt();
      camDist = m.substring(c2 + 1).toInt();
    } else {
      hasCam = false;
    }
    navDirty = true;
    return;
  }
  if (m.startsWith("#j")) {
    laneN = 0;
    if (m.length() > 3) {
      String s = m.substring(3);
      int n = s.length() / 2;
      if (n > 8) n = 8;
      for (int i = 0; i < n; i++) { laneBg[i] = hexVal(s[2 * i]); laneFr[i] = hexVal(s[2 * i + 1]); }
      laneN = n;
    }
    navDirty = true;
    return;
  }
  if (m.startsWith("#p,")) {                             // v2.0 位置
    int fr = 0, hd = 0, kmh = 0; long xd = 0, yd = 0;
    if (sscanf(m.c_str(), "#p,%d,%ld,%ld,%d,%d", &fr, &xd, &yd, &hd, &kmh) == 5) {
      // v2.6 记下“按旧数据推算的当前位置”和新位置的偏差，画面在 0.4 秒内平滑修正过去
      unsigned long nowMs = millis();
      // v6.7 转弯速度：和上一次定位的朝向差 / 时间（太慢、隔太久、变化太离谱就当没在转）
      float newRate = 0;
      if (fr == posFrame && posAt) {
        float fdt = (nowMs - posAt) / 1000.0f;
        float dhd = hd - posHdg;
        while (dhd > 180) dhd -= 360;
        while (dhd < -180) dhd += 360;
        if (fdt > 0.3f && fdt < 2.5f && kmh >= 6 && fabsf(dhd) < 120) newRate = constrain(dhd / fdt, -45.0f, 45.0f);
      }
      if (fr == posFrame && posAt) {
        float px, py, ph;
        predictPose(nowMs, 0, px, py, ph);              // v6.7 按旧数据（含转弯）推算的现在位置
        float ex = px - xd / 10.0f, ey = py - yd / 10.0f;
        float rem = corrAt ? fmaxf(0, 1 - (nowMs - corrAt) / 400.0f) : 0;   // 上一次还没修正完的部分
        ex += corrX * rem; ey += corrY * rem;
        if (ex * ex + ey * ey < 60 * 60) { corrX = ex; corrY = ey; corrAt = nowMs; }
        else { corrX = corrY = 0; corrAt = 0; }
      }
      posFrame = fr; posX = xd / 10.0f; posY = yd / 10.0f; posHdg = hd; posKmh = kmh; posAt = nowMs;
      hdgRate = hdgRate * 0.3f + newRate * 0.7f;          // v6.7 稍微平滑一下，GPS 朝向偶尔抖一下不至于乱转
      if (inNav) navDirty = true;
    }
    return;
  }
  if (m.startsWith("#q,")) {                             // v2.3 屏幕数据线速度：保存并重启
    int q = m.substring(3).toInt();
    if ((q == 40 || q == 60 || q == 80) && q != qspiMHz) {
      Preferences pr;
      pr.begin("motonav", false);
      pr.putInt("qspi", q);
      pr.end();
      delay(100);
      ESP.restart();
    }
    return;
  }
  if (m.startsWith("#f,")) {                             // v2.6 快速刷屏开关：保存并重启
    bool on = m.substring(3).toInt() != 0;
    if (on != fastFlushOn) {
      Preferences pr;
      pr.begin("motonav", false);
      pr.putBool("fastf", on);
      pr.end();
      delay(100);
      ESP.restart();
    }
    return;
  }
  if (m.startsWith("#t,")) {                             // v2.9 地图配色主题
    int t = m.substring(3).toInt();
    if (t >= 0 && t < THEME_N) {
      // App 8.3.23 adds a third field: each style now owns its map data.
      // Old Android/iOS #t,t commands keep their legacy recoloring behavior.
      if (t != themeIdx && m.indexOf(',',3)>=0) {
        rasterValid=false; rasterBackValid=false; mapValid=false;
        for(int kind : {1,3}) {
          Bulk &b=bulks[kind];b.id=-1;b.total=0;b.got=0;b.done=false;
          memset(b.mask,0,sizeof(b.mask));
        }
      }
      themeIdx = t; navDirty = true;
    }   // v7.8.11 0~11
    return;
  }
  if (m.startsWith("#Q,")) { curveOn = m.substring(3).toInt() != 0; navDirty = true; return; }   // v6.2 弯道上色
  if (m.startsWith("#R,") || m.startsWith("#D")) {
    // iOS sends #R,nearestQuarter,exactDegrees; Android sends #R then #Ddegrees.
    // Preserve legacy #R,quarter while applying the exact angle when supplied.
    int d;
    if (ScreenRotationCommand::parse(m.c_str(), d) && d != screenDeg) {
      setScreenDeg(d, true); navDirty = true; needFlush = true;
    }
    return;
  }
  if (m.startsWith("#L")) {                              // v6.7 预判提前量："#L300"
    leadMs = constrain(m.substring(2).toInt(), 0, 800);
    return;
  }
  if (m == "#UNPAIR") {                                  // v6.5 忘掉配对过的手机；v6.6 只忘“别的”手机，正在用的这台保留
#if defined(CONFIG_NIMBLE_ENABLED)
    ble_addr_t keep;
    bool haveKeep = false;
    ble_gap_conn_desc d;
    if (bleConnHandle != 0xFFFF && ble_gap_conn_find(bleConnHandle, &d) == 0) { keep = d.peer_id_addr; haveKeep = true; }
    ble_addr_t peers[8];
    int n = 0;
    if (ble_store_util_bonded_peers(peers, &n, 8) == 0) {
      for (int i = 0; i < n; i++) {
        if (haveKeep && ble_addr_cmp(&peers[i], &keep) == 0) continue;
        ble_gap_unpair(&peers[i]);
      }
    }
#endif
    return;
  }
  if (m.startsWith("#e")) {                              // v3.1 行程信息：剩余米,到达时间HHMM,红绿灯数
    int rm = 0, eta = 0, lt = 0;
    if (sscanf(m.c_str(), "#e,%d,%d,%d", &rm, &eta, &lt) == 3) {
      // v7.3 路程进度：剩余变少就算走掉了（一次少太多的当成重新规划，不算）
      unsigned long nowMs = millis();
      if (progLastRm >= 0) {
        long d = (long)progLastRm - rm;
        long cap = 200 + (long)((nowMs - progLastAt) / 1000) * 60;     // 最快按 60 米/秒算
        if (d > 0 && d <= cap) progDone += d;
      }
      progLastRm = rm; progLastAt = nowMs;
      tripRemainM = rm; tripEta = eta; tripLights = lt; tripOn = true;
      long total=progDone+tripRemainM;
      if(total>0)tripProgress.sample(progDone/(float)total,nowMs);
    }
    else tripOn = false;
    navDirty = true;
    return;
  }
  if (m.startsWith("#c")) {                              // v3.5 小地图上的电子眼位置；只有 #c 表示清空
    int oldN = mapCamN;
    int32_t oldX[MCAM_MAX], oldY[MCAM_MAX]; int oldT[MCAM_MAX]; int8_t oldP[MCAM_MAX], oldK[MCAM_MAX]; float oldH[MCAM_MAX], oldA[MCAM_MAX];
    for (int k = 0; k < oldN; k++) { oldX[k] = mapCamX[k]; oldY[k] = mapCamY[k]; oldT[k] = mapCamTyp[k]; oldP[k] = mapCamPref[k]; oldH[k] = mapCamLockH[k]; oldA[k] = mapCamPrefA[k]; oldK[k] = mapCamPrefK[k]; }
    mapCamN = 0;
    const char *q = m.c_str() + 2;
    if (*q == ',') {
      char *e;
      mapCamFrame = strtol(q + 1, &e, 10); q = e;
      while (*q == ',' && mapCamN < MCAM_MAX) {
        long v[4]; bool ok = true;
        for (int k = 0; k < 4; k++) {
          if (*q != ',') { ok = false; break; }
          v[k] = strtol(q + 1, &e, 10); q = e;
        }
        if (!ok) break;
        mapCamX[mapCamN] = v[0]; mapCamY[mapCamN] = v[1]; mapCamSpd[mapCamN] = v[2]; mapCamTyp[mapCamN] = v[3];
        // v3.8 同一个电子眼（坐标一样）沿用上次的图标位置和锁定朝向（v4.0）
        int8_t pref = -1;
        float lockH = 0, prefA = 0; int8_t prefK = 0;
        for (int k = 0; k < oldN; k++)
          if (oldX[k] == v[0] && oldY[k] == v[1] && oldT[k] == v[3]) { pref = oldP[k]; lockH = oldH[k]; prefA = oldA[k]; prefK = oldK[k]; break; }
        mapCamPref[mapCamN] = pref; mapCamLockH[mapCamN] = lockH; mapCamPrefA[mapCamN] = prefA; mapCamPrefK[mapCamN] = prefK;
        mapCamN++;
      }
    }
    navDirty = true;
    return;
  }
  if (m.startsWith("#g")) {                              // v3.1 前方红绿灯距离；只有 #g 表示隐藏
    int d = -1;
    lightDist = (sscanf(m.c_str(), "#g,%d", &d) == 1) ? d : -1;
    navDirty = true;
    return;
  }
  if (m.startsWith("#n,")) { nightOn = m.substring(3).toInt() != 0; navDirty = true; return; }   // v3.0 夜间配色
  if (m == "#o") { ghostN = 0; navDirty = true; return; }   // v4.4 不显示原路线
  if (m == "#WXRESET") { wxHave = false; wxExtra = false; wxDetail = 0; wxDirty = true; return; }
  if (m.startsWith("#WX,")) { parseWeatherExtra(m); return; }
  if (m.startsWith("#W,")) { parseWeather(m); return; }   // v5.3 天气
  if (m.startsWith("#i,")) {                             // v5.1 来电："#i,1,号码" / "#i,0"
    bool on = m.charAt(3) == '1';
    if (on) {
      if (!callOn) { callAt = millis(); callAnswerSent = false; touchHoldAt = millis(); touchHold = 0; }
      callSeenAt = millis();
      int c2 = m.indexOf(',', 4);
      callNum = c2 > 0 ? m.substring(c2 + 1) : "";
      if (callNum.length() > 16) callNum = callNum.substring(0, 16);
    }
    callOn = on;
    if (!on) navDirty = true;
    return;
  }
  if (m.startsWith("#y,")) {                             // v5.0 重新规划中
    rerouteAt = m.substring(3).toInt() ? max(1UL, millis()) : 0;
    if (rerouteAt) fasterAt = 0;
    navDirty = true; return;
  }
  if (m.startsWith("#h")) {                              // v5.0 更快路线：省几分钟；只有 #h 表示收起
    if (m.length() > 3) { fasterMin = m.substring(3).toInt(); fasterAt = max(1UL, millis()); } else fasterAt = 0;
    navDirty = true; return;
  }
  if (m.startsWith("#x,")) { gpsWeak = m.substring(3).toInt() != 0; navDirty = true; return; }   // v5.0 GPS 信号弱
  if (m.startsWith("#u")) {                              // v5.0 路况：起,止,状态,...；只有 #u 表示清空
    trafN = 0;
    const char *q = m.c_str() + 2;
    while (*q == ',' && trafN < TRAF_MAX) {
      char *e; long a = strtol(q + 1, &e, 10); q = e; if (*q != ',') break;
      long b = strtol(q + 1, &e, 10); q = e; if (*q != ',') break;
      long st = strtol(q + 1, &e, 10); q = e;
      trafA[trafN] = a; trafB[trafN] = b; trafS[trafN] = (uint8_t)st; trafN++;
    }
    navDirty = true; return;
  }
  if (m.startsWith("#w,")) { cleanOn = m.substring(3).toInt() != 0; navDirty = true; return; }   // v4.9 清爽模式
  if (m.startsWith("#z,")) {                             // v4.8 巡航模式开关
    cruiseOn = m.substring(3).toInt() != 0;
    if (cruiseOn) { inNav = true; navDir = "S"; navDist = 0; ringActive = false; tripOn = false; laneN = 0; }
    navDirty = true;
    return;
  }
  if (m.startsWith("#r,")) {                             // v3.0 走过的路线：1 变灰，2 擦除
    int r = m.substring(3).toInt();
    if (r == 1 || r == 2) { trailMode = r; navDirty = true; }
    return;
  }
  if (m.startsWith("#m,")) { mapOn = m.substring(3).toInt() != 0; navDirty = true; return; }
  if (m.startsWith("#b,")) {
    int b = constrain(m.substring(3).toInt(), 5, 255);
    if (flushDone) {                                    // v2.2 刷屏任务在用总线时先等它推完
      xSemaphoreTake(flushDone, portMAX_DELAY);
      panel->setBrightness(b);
      xSemaphoreGive(flushDone);
    } else {
      panel->setBrightness(b);
    }
    return;
  }

  if (m.startsWith("#O")) {                              // v7.1 行程统计 "#O米/分钟/最高车速"
    long a = -1; int b = -1, c = -1;
    if (sscanf(m.c_str() + 2, "%ld/%d/%d", &a, &b, &c) >= 2) { tripM = a; tripMin = b; tripMaxKmh = c; tripAt = millis(); }
    return;
  }
  if (m == "#M1" || m == "#M0") { progOn = m[2] == '1'; navDirty = true; return; }   // v7.3 路程进度开关
  if (m == "#J") { if (page == 0) startArriveFx(true); return; }   // v7.1 到达途经点
  if (m.startsWith("#")) return;                        // v6.7 不认识的 "#..." 指令：忽略（别当成导航方向）
  int comma = m.indexOf(',');                           // 导航：方向,距离
  if (comma > 0) {
    String prevDir = navDir;
    navDir = m.substring(0, comma);
    // v7.1 导航中 → 到达：放特效（重连补发的 "A,0"、圆屏刚开机收到的都不放）
    if (navDir == "A" && prevDir.length() && prevDir != "A" && page == 0) startArriveFx(false);
    if (prevDir == "A" && navDir != "A") progReset();       // v7.3 到达后又开始新的导航：进度从头算
    else if (navDir != "A" && fxOn) stopArriveFx();
    int comma2 = m.indexOf(',', comma + 1);
    if (comma2 > 0) {
      navDist = m.substring(comma + 1, comma2).toInt();
      navAngle = m.substring(comma2 + 1).toFloat();
    } else {
      navDist = m.substring(comma + 1).toInt();
      navAngle = NAN;
    }
    inNav = true;
    if (navDir != "A") idlePowerOffReset(&idlePowerOffTimer, millis());
    updateRingTarget();
    navDirty = true;
  }
}

void handleMessage(const uint8_t *d, int n) {
  lastRxMs = millis();
  if (staleShown) { staleShown = false; navDirty = true; }   // 收到任何消息，清掉数据中断提示
  if (d[0] == 'B') { handleBitmap(d, n); return; }
  if (d[0] == 'M') { handleBulk(d, n); return; }     // v2.0 地图块 / 路线
  char buf[512];                                      // v4.8 64 → 512：电子眼列表长了会被截断
  int len = n < 511 ? n : 511;
  memcpy(buf, d, len);
  buf[len] = 0;
  String m = String(buf);
  m.trim();
#if DEBUG_SERIAL
  Serial.println("recv: " + m);
#endif
  handleText(m);
}
