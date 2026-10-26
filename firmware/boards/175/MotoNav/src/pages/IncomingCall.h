#pragma once
// 触摸初始化与来电页面。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// ================= v5.1 来电显示 =================
// 手机来电时整屏显示：上面 “INCOMING CALL”，中间绿色电话图标带往外扩散的波纹，下面是名字（手机渲染好的
// 文字图片，图片栏 2）和号码；接听或挂断后回到原来的画面（地图 / 大箭头 / 首页）
bool callOn = false;
String callNum = "";
unsigned long callAt = 0, callSeenAt = 0;

// v5.2 触屏接电话：按住任意位置 HOLD_MS 毫秒 → 发 "T,A" 给手机（v6.9 800 → 200）
#define HOLD_MS 200
TouchDrvCST92xx touch;
bool touchOK = false;
unsigned long touchHoldAt = 0, touchLastSeen = 0;
float touchHold = 0;                 // 0~1 按住的进度
bool callAnswerSent = false;
unsigned long callAnswerAt = 0;
String pendingProf = "";             // v5.8 分步用时
String pendingEvt = "";              // 待发给手机的触摸事件

// v5.3 触摸芯片一有动静就拉低中断脚；平时只看这个标记，不用一直读 I2C
volatile bool touchIrq = false;
void IRAM_ATTR onTouchIrq() { touchIrq = true; }

void touchInit() {
  touch.setPins(TP_RESET, TP_INT);
  touchOK = touch.begin(Wire, 0x5A, IIC_SDA, IIC_SCL);
  Serial.println(touchOK ? "touch ok" : "touch not found");
  if (touchOK) attachInterrupt(TP_INT, onTouchIrq, FALLING);   // v5.3
}

bool touchDown = false;
int16_t touchX0 = 0, touchY0 = 0, touchX1 = 0, touchY1 = 0;
void dragMove(int dx, int dy, unsigned long now);   // v5.5 跟手换页
int musicButtonAt(int x, int y);
void musicTap(int x, int y);
void dragEnd();
void dragAbort();

// 读触摸：按下 → 报坐标；来电时按住够久 → 接听；拖动 → 跟手换页（v5.5）
// 手指离开超过 0.15 秒才算松开（防止读数偶尔丢一下）
void touchPoll() {
  static unsigned long lastPoll = 0;
  unsigned long now = millis();
  if (!touchOK) return;
  if (!touchIrq && !touchDown && !callOn) return;
  if (now - lastPoll < 12) return;
  lastPoll = now;
  touchIrq = false;
  if (callAnswerSent && now - callAnswerAt > 4000) callAnswerSent = false;   // 4 秒还在响：允许再按一次
  int16_t x[5] = {0}, y[5] = {0};
  uint8_t n = touch.getPoint(x, y, 1);
  if (n > 0) {
    if (!touchDown) {
      touchDown = true;
      touchHoldAt = now;
      touchX0 = touchX1 = x[0]; touchY0 = touchY1 = y[0];
      pendingEvt = "T,P," + String(x[0]) + "," + String(y[0]);
      if (page == 2 && !callOn && !pairOn) { musicPressed = musicButtonAt(x[0], y[0]); musicDirty = true; }
      if (fxOn && !callOn) { stopArriveFx(); touchEat = true; }          // v7.1 点一下结束到达特效
    }
    touchX1 = x[0]; touchY1 = y[0];
    touchLastSeen = now;
    if (callOn || pairOn || touchEat || fxOn) dragAbort();
    else dragMove(touchX1 - touchX0, touchY1 - touchY0, now);
  } else if (touchDown && now - touchLastSeen > 80) {
    touchDown = false;
    touchHoldAt = 0;
    if (callOn || pairOn || touchEat || fxOn) dragAbort();
    else {
      if (page == 2 && abs(touchX1 - touchX0) < 36 && abs(touchY1 - touchY0) < 36) musicTap(touchX0, touchY0);
      if (page == 1 && abs(touchX1 - touchX0) < 36 && abs(touchY1 - touchY0) < 36) weatherTap();
      dragEnd();
    }
    touchEat = false;
    if (musicPressed >= 0) { musicPressed = -1; musicDirty = true; }
  }
  // 来电：按住不动才算（滑动不算）
  bool still = abs(touchX1 - touchX0) < 60 && abs(touchY1 - touchY0) < 60;
  touchHold = (callOn && touchDown && still) ? min(1.0f, (now - touchHoldAt) / (float)HOLD_MS) : 0;
  if (touchHold >= 1 && !callAnswerSent) {
    callAnswerSent = true;
    callAnswerAt = now;
    touchHold = 0;
    pendingEvt = "T,A";
  }
}

// 电话听筒：一段拱形的圆点串当手柄，两头各一个大点当听筒和话筒，整体斜过来（常见的电话图标）
void drawHandset(float cx, float cy, float r, uint16_t c) {
  const float ROT = -35 * PI / 180, cr = cosf(ROT), sr = sinf(ROT);
  auto dot = [&](float x, float y, float rr) {
    aaDisc(cx + x * cr - y * sr, cy + x * sr + y * cr, rr, c);
  };
  for (int a = 25; a <= 155; a += 5) {
    float rad = a * PI / 180;
    dot(cosf(rad) * r, -sinf(rad) * r * 0.55f + r * 0.25f, r * 0.22f);
  }
  for (int k = 0; k < 2; k++) {
    float rad = (k ? 155 : 25) * PI / 180;
    dot(cosf(rad) * r + (k ? -1 : 1) * r * 0.04f, -sinf(rad) * r * 0.55f + r * 0.25f + r * 0.21f, r * 0.32f);
  }
}

void drawCallScreen() {
  onWaitScreen = false;
  frameIsMap = false;
  gfx->fillScreen(C_BLACK);
  unsigned long t = millis() - callAt;
  const float icy = 168;
  // 往外扩散的两圈波纹
  for (int k = 0; k < 2; k++) {
    float ph = fmodf(t / 1400.0f + k * 0.5f, 1.0f);
    float r = 52 + ph * 110;
    uint16_t c = blend565(C_GREEN, C_BLACK, (int)(ph * 255));
    for (int w = 0; w < 3; w++) gfx->drawCircle(CX, (int)icy, (int)r + w, c);
  }
  aaDisc(CX, icy, 50, 0x2DC6);
  aaDisc(CX, icy, 46, C_GREEN);
  // 听筒轻轻左右晃
  float wob = ((t / 120) % 4 < 2) ? -2.5f : 2.5f;
  drawHandset(CX + wob, icy, 26, C_WHITE);
  printCentered("INCOMING CALL", CX, 58, 2, C_GREEN);
  bool hasName = imgs[2].show && imgs[2].showW > 0;
  if (hasName) {
    drawTextImg(2, 252, 0xFF);
    if (callNum.length()) printCentered(callNum, CX, 318, 2, C_GREY);
  } else if (callNum.length()) {
    printCentered(callNum, CX, 262, callNum.length() > 12 ? 3 : 4, C_WHITE);
  } else {
    printCentered("UNKNOWN", CX, 262, 4, C_WHITE);
  }
  // v5.2 触屏接听提示；按住时绿圈外面一圈白点从正上方顺时针走满
  if (touchOK) {
    if (callAnswerSent) printCentered("ANSWERING...", CX, 392, 2, C_GREEN);
    else printCentered("TAP & HOLD", CX, 392, 2, C_GREY);      // v6.9
    if (touchHold > 0 && !callAnswerSent) {
      int steps = (int)(touchHold * 72);
      for (int k = 0; k <= steps; k++) {
        float a = k * 5 * PI / 180;
        aaDisc(CX + sinf(a) * 64, icy - cosf(a) * 64, 4, C_WHITE);
      }
    }
  }
  needFlush = true;
}
