#pragma once
#include <Arduino.h>

// 拖动状态
bool dragging = false;
int dragTarget = 0, dragSign = 1;
bool dragHoriz = true;
float dragP = 0, dragV = 0;                 // 新页面已经进来多少像素；速度（像素/毫秒）
unsigned long dragT = 0;

uint16_t *shownFrame() {
  if (fbBuf[1] && flushSrc) return flushSrc;
  return gfx->getFramebuffer();
}

static inline uint16_t dim565(uint16_t c, uint32_t k) {   // k: 0~32，32 = 原样
  uint32_t r = (c >> 11) & 31, g = (c >> 5) & 63, b = c & 31;
  return (uint16_t)(((r * k >> 5) << 11) | ((g * k >> 5) << 5) | (b * k >> 5));
}

// 画一个页面到画布（不改当前页）
void renderPage(int p) {
  int keep = page;
  bool ks = sliding;
  page = p; sliding = false;
  if (p == 2) drawMusic();
  else if (p == 1) drawWeather();
  else if (p == 3) drawLeanMeter();
  else if (p == 4) CompanionMode::drawInitialFrame();
  else if (inNav) drawNavFrame();
  else drawWaiting(bleConnected ? WAIT_READY : WAIT_NOLINK);
  page = keep; sliding = ks;
}

// 准备滑动：存下屏幕上的旧画面，在画布上画好目标页面存起来
bool slidePrepare(int target) {
  const size_t BYTES = (size_t)LCD_WIDTH * LCD_HEIGHT * 2;
  if (!slideOld) slideOld = (uint16_t *)heap_caps_malloc(BYTES, MALLOC_CAP_SPIRAM);
  if (!slideNew) slideNew = (uint16_t *)heap_caps_malloc(BYTES, MALLOC_CAP_SPIRAM);
  if (!slideOld || !slideNew) return false;
  if (!chordReady) {
    for (int y = 0; y < LCD_HEIGHT; y++) {
      float d = y + 0.5f - LCD_HEIGHT / 2.0f, r = LCD_WIDTH / 2.0f + 1;
      chordHalf[y] = d * d < r * r ? (int16_t)ceilf(sqrtf(r * r - d * d)) : 0;
    }
    chordReady = true;
  }
  if (fbBuf[1]) { flushWait(); xSemaphoreGive(flushDone); }   // 等正在推的那帧推完，旧画面才完整
  memcpy(slideOld, shownFrame(), BYTES);
  renderPage(target);
  memcpy(slideNew, gfx->getFramebuffer(), BYTES);
  sliding = true;
  needFlush = false; frameIsMap = false;
  return true;
}

// 合成一帧：新页面已经进来 p 像素（盖在上面），旧页面往同方向退 30%、变暗，交界处一道阴影
void slideCompose(int p) {
  const int W = LCD_WIDTH, H = LCD_HEIGHT;
  if (p < 0) p = 0;
  if (p > (dragHoriz ? W : H)) p = dragHoriz ? W : H;
  uint16_t *fb = gfx->getFramebuffer();
  int full = dragHoriz ? W : H;
  int q = p * 3 / 10;                                   // 旧页面退后的距离
  uint32_t k0 = 32 - (uint32_t)(p * 13 / full);         // 旧页面整体亮度：100% → 约 60%
  const int SH = 22;                                    // 阴影宽度
  if (dragHoriz) {
    static uint8_t colK[LCD_WIDTH];
    for (int x = 0; x < W; x++) {
      int dist = dragSign > 0 ? x - p : (W - p - 1) - x;   // 离交界多远（旧页面这一侧）
      uint32_t k = k0;
      if (p > 0 && dist < SH) k = k * (14 + dist * 18 / SH) / 32;
      colK[x] = (uint8_t)k;
    }
    for (int y = 0; y < H; y++) {
      int h = chordHalf[y];
      if (!h) continue;
      int xa = max(0, W / 2 - h), xb = min(W, W / 2 + h);
      uint16_t *row = fb + (size_t)y * W;
      const uint16_t *o = slideOld + (size_t)y * W, *n = slideNew + (size_t)y * W;
      if (dragSign > 0) {                               // 新页面从左边进来
        int a = xa, b = min(p, xb);
        if (b > a) memcpy(row + a, n + (W - p) + a, (b - a) * 2);
        for (int x = max(p, xa); x < xb; x++) row[x] = colK[x] >= 32 ? o[x - q] : dim565(o[x - q], colK[x]);
      } else {                                          // 新页面从右边进来
        int a = max(W - p, xa), b = xb;
        if (b > a) memcpy(row + a, n + (a - (W - p)), (b - a) * 2);
        for (int x = xa; x < min(W - p, xb); x++) row[x] = colK[x] >= 32 ? o[x + q] : dim565(o[x + q], colK[x]);
      }
      if (p > 1 && p < W - 1) {
        int ex = dragSign > 0 ? p - 1 : W - p;
        if (ex >= xa && ex < xb) row[ex] = 0x4A69;
        int ex2 = ex + (dragSign > 0 ? -1 : 1);
        if (ex2 >= xa && ex2 < xb) row[ex2] = 0x2945;
      }
    }
  } else {
    for (int y = 0; y < H; y++) {
      int h = chordHalf[y];
      if (!h) continue;
      int xa = max(0, W / 2 - h), xb = min(W, W / 2 + h);
      uint16_t *row = fb + (size_t)y * W;
      bool isNew = dragSign > 0 ? y < p : y >= H - p;
      if (isNew) {
        int sy = dragSign > 0 ? y + (H - p) : y - (H - p);
        memcpy(row + xa, slideNew + (size_t)sy * W + xa, (xb - xa) * 2);
        int edge = dragSign > 0 ? p - 1 : H - p;
        if (p > 1 && p < H - 1 && (y == edge || y == edge + (dragSign > 0 ? -1 : 1))) {
          uint16_t ec = y == edge ? 0x4A69 : 0x2945;
          for (int x = xa; x < xb; x++) row[x] = ec;
        }
      } else {
        int sy = dragSign > 0 ? y - q : y + q;
        int dist = dragSign > 0 ? y - p : (H - p - 1) - y;
        uint32_t k = k0;
        if (p > 0 && dist < SH) k = k * (14 + dist * 18 / SH) / 32;
        const uint16_t *o = slideOld + (size_t)sy * W;
        for (int x = xa; x < xb; x++) row[x] = k >= 32 ? o[x] : dim565(o[x], k);
      }
    }
  }
  flushAsync();
  needFlush = false; frameIsMap = false;
}

// 换页完成
void slideCommit() {
  const size_t BYTES = (size_t)LCD_WIDTH * LCD_HEIGHT * 2;
  sliding = false; dragging = false;
  page = dragTarget;
  pageAt = millis();
  memcpy(gfx->getFramebuffer(), slideNew, BYTES);
  needFlush = true; frameIsMap = false;
  if (page == 2) {
    onWaitScreen = false; musicDirty = true; pendingEvt = "T,M,Q";
  } else if (page == 1) {
    lastSlideSx = dragHoriz ? dragSign : 0; lastSlideSy = dragHoriz ? 0 : dragSign;
    onWaitScreen = false;
    wxDirty = false;
    if (!wxHave || millis() - wxAt > 15UL * 60 * 1000) { pendingEvt = "T,W"; wxAskAt = millis(); }
  } else if (page == 3) {
    onWaitScreen = false;
    leanDirty = true;
  } else if (page == 4) {
    onWaitScreen = false;
    CompanionMode::enter();
  } else {
    if (inNav) navDirty = true;
    else { riderPrevF = -1; riderPrevZ = -2; catForce = true; }
  }
}

// 没换成：回到原页面
void slideCancel() {
  sliding = false; dragging = false;
  renderPage(page);
  if (page == 2) musicDirty = true;
  else if (page == 1) wxDirty = false;
  else if (page == 3) leanDirty = true;
  else if (page == 4) { onWaitScreen = false; CompanionMode::drawInitialFrame(); }
  else if (inNav) navDirty = true;
  else { onWaitScreen = true; catForce = true; }
  needFlush = true;
}

// 从 p0 顺势滑到 p1（先快后慢）；ms = 时长
void slideAnimate(float p0, float p1, int ms) {
  unsigned long t0 = millis();
  for (;;) {
    float t = (millis() - t0) / (float)ms;
    if (t >= 1) break;
    float e = 1 - (1 - t) * (1 - t) * (1 - t);         // ease-out
    slideCompose((int)(p0 + (p1 - p0) * e));
  }
  slideCompose((int)p1);
}

// 触摸向量统一换算：先修正芯片坐标的镜像，再按刷屏的逆旋转映射到画布。
void touchVectorToCanvas(float &dx, float &dy) {
  if (TOUCH_FLIP_X) dx = -dx;
  if (TOUCH_FLIP_Y) dy = -dy;
  float ox = dx, oy = dy;
  if (rotFree) { dx = ox * rotCosF + oy * rotSinF; dy = -ox * rotSinF + oy * rotCosF; }
  else if (screenRot == 1) { dx = oy; dy = -ox; }
  else if (screenRot == 2) { dx = -ox; dy = -oy; }
  else if (screenRot == 3) { dx = -oy; dy = ox; }
}

// 手指在拖：(dx, dy) = 从按下到现在的位移
void dragMove(int dx, int dy, unsigned long now) {
  float vx = dx, vy = dy;
  touchVectorToCanvas(vx, vy);
  dx = (int)lroundf(vx); dy = (int)lroundf(vy);
  if (!dragging) {
    int threshold = page == 2 ? 36 : DRAG_START;
    if (abs(dx) < threshold && abs(dy) < threshold) return;
    dragHoriz = abs(dx) >= abs(dy);
    int d = dragHoriz ? dx : dy;
    dragSign = d > 0 ? 1 : -1;
    if (page == 2) { if (!dragHoriz || dragSign > 0) return; dragTarget = 0; }
    else if (page == 1) dragTarget = 0;
    else if (page == 3) {
      // 倾角仪表在主界面下方：如果垂直向下滑动 (dy > 0, dragSign > 0)，返回主页 0
      if (dragHoriz || dragSign < 0) return;
      dragTarget = 0;
    }
    else if (page == 4) {
      // 伴侣模式在屏幕上方：由于长按 AB 键返回，这里不进行常规滑动换页，避免与表情互动冲突
      return;
    }
    else if (dragHoriz) dragTarget = dragSign < 0 ? 1 : 2;
    else dragTarget = dragSign < 0 ? 3 : 4; // 手指上滑 dy < 0 进入 page 3(倾角)，从上往下滑动 dy > 0 进入 page 4(伴侣模式)
    if (!slidePrepare(dragTarget)) return;
    dragging = true;
    dragP = 0; dragV = 0; dragT = now;
  }
  float p = (dragHoriz ? dx : dy) * dragSign;
  if (p < 0) p = 0;
  float dt = now - dragT;
  if (dt > 0) dragV = dragV * 0.5f + ((p - dragP) / dt) * 0.5f;
  dragP = p; dragT = now;
  slideCompose((int)p);
}

// 松手：拖过 30% 或者往前甩出去 → 顺势滑完；否则弹回
void dragEnd() {
  if (!dragging) return;
  int full = dragHoriz ? LCD_WIDTH : LCD_HEIGHT;
  bool go = dragP > full * 0.3f || (dragV > 0.35f && dragP > 24);
  if (dragV < -0.35f) go = false;                       // 往回甩：取消
  float target = go ? full : 0;
  float left = fabsf(target - dragP);
  int ms = 140 + (int)(260 * left / full);
  if (go && dragV > 0.5f) ms = max(140, (int)(ms * 0.75f));   // 甩得快就滑得快
  slideAnimate(dragP, target, ms);
  if (go) slideCommit(); else slideCancel();
}

// 来电等情况：直接放弃拖动
void dragAbort() {
  if (!dragging) return;
  sliding = false; dragging = false;
  if (page == 2) musicDirty = true; else if (page == 1) wxDirty = true; else if (page == 3) leanDirty = true; else if (inNav) navDirty = true;
}

// 将触摸点换算到画布坐标，按钮使用较大的点按范围。
int musicButtonAt(int x, int y) {
  float dx = x - CX, dy = y - CX;
  touchVectorToCanvas(dx, dy);
  x = lroundf(CX + dx); y = lroundf(CX + dy);
  for (int i = 0; i < 3; i++) {
    int bx = CX + (i - 1) * 100;
    if ((x - bx) * (x - bx) + (y - 335) * (y - 335) <= 49 * 49) return i;
  }
  return -1;
}

void musicTap(int x, int y) {
  if (dragging || page != 2 || callOn || pairOn || !bleConnected || !bleAuthed) return;
  int button = musicButtonAt(x, y);
  if (hidMusicMode) {
    if (button < 0) return;
    bool hidReady = hidMediaReady();
    if (!hidReady && !appleMedia.controlReady(button)) { musicAck = 7; musicAckUntil = millis() + 2500; musicDirty = true; return; }
    if (hidTapAt && millis() - hidTapAt < 120) return;
    hidTapAt = millis();
    if (!hidReady) {
      musicAck = appleMedia.control(button) ? 11 : 10; musicAckUntil = millis() + 2500; musicDirty = true; return;
    }
    if (!hidPulses.enqueue(button)) { musicAck = 8; musicAckUntil = millis() + 2500; musicDirty = true; }
    return; // no T,M,P/T/N: do not also control Apple Music through the App
  }
  if (button < 0 || !musicHave) {
    pendingEvt = String("T,M,H,") + x + "," + y + "," + button;
    return;
  }
  pendingEvt = String("T,M,") + (button == 0 ? "P" : button == 1 ? "T" : "N") + "," + x + "," + y + "," + button;
  musicAck = 1; musicAckUntil = millis() + 2500; musicDirty = true;
}

// 自动换页（导航中看天气超时 / 快到路口）：整段动画
void showPageAnimated(int target, int sx, int sy) {
  if (page == target) return;
  dragHoriz = sx != 0 || sy == 0;
  dragSign = dragHoriz ? (sx ? sx : 1) : sy;
  dragTarget = target;
  if (!slidePrepare(target)) { page = target; renderPage(target); needFlush = true; return; }
  dragging = true;
  slideAnimate(0, dragHoriz ? LCD_WIDTH : LCD_HEIGHT, 420);
  slideCommit();
}

// 音乐页在路口前回导航，其余时候保持显示，便于连续切歌。
void musicTick() {
  if (musicAckUntil && (long)(millis() - musicAckUntil) >= 0) {
    if (musicAck == 1) { musicAck = 4; musicAckUntil = millis() + 2500; }
    else { musicAck = 0; musicAckUntil = 0; }
    musicDirty = true;
  }
  if (page != 2 || callOn || pairOn || sliding) return;
  if (inNav && !cruiseOn && navDir != "S" && navDir.length() && navDist > 0 && navDist < 200 && millis() - pageAt > 3000) {
    showPageAnimated(0, -1, 0);
    return;
  }
  if (musicDirty) { musicDirty = false; drawMusic(); }
}

// 每圈调用：天气页要不要重画、要不要回导航
void weatherTick() {
  if (page != 1 || callOn || sliding) return;
  unsigned long now = millis();
  // 导航中：看 15 秒自动回去；快到路口也立刻回去
  if (inNav && (now - pageAt > 15000 || (navDir != "S" && navDir.length() && navDist > 0 && navDist < 200))) {
    showPageAnimated(0, -lastSlideSx, -lastSlideSy);   // 按进来的方向反着滑回去
    return;
  }
  // 还没数据或数据旧了：30 秒再要一次
  if (bleConnected && (!wxHave || now - wxAt > 15UL * 60 * 1000) && now - wxAskAt > 30000) { pendingEvt = "T,W"; wxAskAt = now; }
  if (wxDirty) { wxDirty = false; drawWeather(); }
}

// 每圈调用：倾角与 G 值刷新
void leanTick() {
  LeanAngleMeter::update();
  if (page != 3 || callOn || pairOn || sliding) return;
  static unsigned long lastLeanDraw = 0;
  if (millis() - lastLeanDraw >= 33 || leanDirty) {
    lastLeanDraw = millis();
    leanDirty = false;
    drawLeanMeter();
  }
}

void drawStaleOverlay() {
  drawRedRing();
  printCentered("NO DATA", CX, 412, 3, C_RED);
  needFlush = true;
}

