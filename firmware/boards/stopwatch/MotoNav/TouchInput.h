#pragma once
#include <Arduino.h>
#include "TouchDrvCSTXXX.hpp"
#include "BoardStopWatchInit.h"

// ================= v5.1 来电显示 =================
// 手机来电时整屏显示：上面 “INCOMING CALL”，中间绿色电话图标带往外扩散的波纹，下面是名字（手机渲染好的
// 文字图片，图片栏 2）和号码；接听或挂断后回到原来的画面（地图 / 大箭头 / 首页）
bool callOn = false;
String callNum = "";
unsigned long callAt = 0, callSeenAt = 0;

// v5.2 触屏接电话：按住任意位置 HOLD_MS 毫秒 → 发 "T,A" 给手机（v6.9 800 → 200）
#define HOLD_MS 200
TouchDrvCST816 touch;   // StopWatch：CST820B（chipId 0xB7）属于 CST816 族；微雪 1.75 的 CST9217 才用 CST92xx
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
  StopWatchBoard::tpReset();      // StopWatch：触摸复位在 M5IOE1 上，按官方时序手动来一次
  touch.setPins(TP_RESET, TP_INT);
  touchOK = touch.begin(Wire, 0x15, IIC_SDA, IIC_SCL);   // StopWatch CST820B 地址 0x15（微雪 CST9217 是 0x5A）
  if (!touchOK) {
    // M5IOE1 初始化会把总线提到 400kHz，CST820 保守按 100kHz 跑——降速重试一次（换料批次时留个后手）
    Wire.setClock(100000);
    delay(50);
    StopWatchBoard::tpReset();
    touchOK = touch.begin(Wire, 0x15, IIC_SDA, IIC_SCL);
  }
  Serial.println(touchOK ? "touch ok" : "touch not found");
  if (touchOK) attachInterrupt(TP_INT, onTouchIrq, FALLING);   // v5.3
}

// StopWatch：用户按键换页（与触摸手势等价）
//   面对屏幕看：左侧是 A 键，右侧是 B 键
//   主页单按：A 键(左) → 音乐页(page 2，从左滑入)，B 键(右) → 天气页(page 1，从右滑入)
//   天气/音乐页单按：任意键 → 回主页（按进来方向的反向滑回，和手势一致）
//   同时按住 AB 键：
//     - 若不在倾角/G值表：切换到倾角/G值表(page 3，从下方滑入)
//     - 若在倾角/G值表：返回主界面(向下滑回主界面)
void buttonTick() {
  static bool lastA = true, lastB = true;
  static int pendingKey = 0;             // 1=A 等待确认, 2=B 等待确认
  static unsigned long pendingAt = 0;    // 开始等待的时间戳
  static bool comboActive = false;       // 组合键已触发锁
  static unsigned long lastActionAt = 0; // 上次动作时间（防误触连按）

  bool a = digitalRead(BTN_A);
  bool b = digitalRead(BTN_B);
  unsigned long now = millis();

  // 到达目的地结算界面下：按任意实体键（A键或B键）立即退出结算返回主页
  if (fxOn) {
    if ((!a && lastA) || (!b && lastB)) {
      stopArriveFx();
      CompanionMode::startVibration(140, 50); // 触感震动反馈
      lastA = a; lastB = b;
      return;
    }
    lastA = a; lastB = b;
    return;
  }

  // 伴侣模式专用按键逻辑：长按 AB 键返回主界面，单按 A/B 浏览表情
  if (page == 4) {
    static unsigned long abBothDownAt = 0;
    static bool abHoldTriggered = false;
    if (!a && !b) {
      if (abBothDownAt == 0) abBothDownAt = now;
      if (!abHoldTriggered && now - abBothDownAt >= 800) {
        abHoldTriggered = true;
        CompanionMode::startVibration(180, 70);
        showPageAnimated(0, 0, 1); // 向下滑回主界面
      }
      lastA = a; lastB = b;
      return;
    } else {
      abBothDownAt = 0;
      if (a && b) abHoldTriggered = false;
    }
    if (abHoldTriggered) {
      lastA = a; lastB = b;
      return;
    }
    // 单按 A/B 键切换表情
    if (!a && lastA) {
      CompanionMode::noteInteraction();
      CompanionMode::avatar.previous(now);
      CompanionMode::startVibration(125, 35);
    } else if (!b && lastB) {
      CompanionMode::noteInteraction();
      CompanionMode::avatar.next(now);
      CompanionMode::startVibration(125, 35);
    }
    lastA = a; lastB = b;
    return;
  }

  // 当两个按键都释放时，解除组合键锁定
  if (a && b) {
    comboActive = false;
  }

  // 1. 同时按住 AB 键检测（优先级最高）
  if (!a && !b) {
    pendingKey = 0; // 取消单键等待
    if (!comboActive && (now - lastActionAt > 350)) {
      comboActive = true;
      lastActionAt = now;
      lastA = a; lastB = b;

      if (sliding || callOn || pairOn || fxOn) return;

      // 同时按住 AB 键：切换倾角/G值表 <-> 主界面
      if (page != 3) {
        showPageAnimated(3, 0, -1); // 从下方滑入倾角/G值表
      } else {
        showPageAnimated(0, 0, 1);  // 向下滑回主界面
      }
      return;
    }
    lastA = a; lastB = b;
    return;
  }

  // 若刚刚触发过组合键，且按键尚未完全松开，避免误触发单键
  if (comboActive) {
    lastA = a; lastB = b;
    return;
  }

  // 2. 检测单键刚按下：给予 65ms 判定窗口，确认用户是否意在同时按住 AB 键
  if (!a && lastA && pendingKey == 0) {
    pendingKey = 1;
    pendingAt = now;
  } else if (!b && lastB && pendingKey == 0) {
    pendingKey = 2;
    pendingAt = now;
  }

  // 3. 单键判定超时或按键抬起确认
  if (pendingKey > 0) {
    if (now - pendingAt >= 65 || (pendingKey == 1 && a) || (pendingKey == 2 && b)) {
      int key = pendingKey;
      pendingKey = 0;
      if (now - lastActionAt > 250) {
        lastActionAt = now;
        if (!sliding && !callOn && !pairOn && !fxOn) {
          if (page == 0) {
            // 主页：A 键切音乐(从左滑入)，B 键切天气(从右滑入)
            showPageAnimated(key == 1 ? 2 : 1, key == 1 ? 1 : -1, 0);
          } else if (page == 3) {
            // 倾角仪表页：单按也可退回主页(向下退出)
            showPageAnimated(0, 0, 1);
          } else {
            // 音乐或天气页：单按退回主页
            showPageAnimated(0, page == 2 ? -1 : 1, 0);
          }
        }
      }
    }
  }

  lastA = a;
  lastB = b;
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
  static bool calibTriggered = false;
  unsigned long now = millis();
  if (!touchOK) return;
  if (!touchIrq && !touchDown && !callOn) return;
  if (now - lastPoll < 12) return;
  lastPoll = now;
  touchIrq = false;
  if (callAnswerSent && now - callAnswerAt > 4000) callAnswerSent = false;   // 4 秒还在响：允许再按一次
  int16_t x[5] = {0}, y[5] = {0};
  uint8_t n = touch.getPoint(x, y, 1);
  if (page == 4) {
    if (n > 0) {
      touchDown = true;
      touchLastSeen = now;
      float tdx = x[0] - CX, tdy = y[0] - CX;
      touchVectorToCanvas(tdx, tdy);
      CompanionMode::handleTouch(true, (int16_t)lroundf(CX + tdx), (int16_t)lroundf(CX + tdy));
    } else {
      touchDown = false;
      CompanionMode::handleTouch(false, 0, 0);
    }
    return;
  }
  if (n > 0) {
    if (!touchDown) {
      touchDown = true;
      touchHoldAt = now;
      touchX0 = touchX1 = x[0]; touchY0 = touchY1 = y[0];
      pendingEvt = "T,P," + String(x[0]) + "," + String(y[0]);
      if (page == 2 && !callOn && !pairOn) { musicPressed = musicButtonAt(x[0], y[0]); musicDirty = true; }
      if (fxOn && !callOn) {
        stopArriveFx();
        CompanionMode::startVibration(140, 50); // 触屏轻触退出也给予震动反馈
        touchEat = true;
      }          // v7.1 点一下结束到达特效
    }
    touchX1 = x[0]; touchY1 = y[0];
    touchLastSeen = now;

    // 倾角仪表页面：校准按钮长按 1.5 秒判定，彻底杜绝骑行轻碰误触
    if (page == 3 && touchDown) {
      float tdx = touchX0 - CX, tdy = touchY0 - CX;
      touchVectorToCanvas(tdx, tdy);
      int cy0 = (int)lroundf(CX + tdy);
      int cx0 = (int)lroundf(CX + tdx);
      if (cy0 >= 340 && abs(cx0 - CX) <= 100) {
        if (now - touchHoldAt >= 1500 && !calibTriggered) {
          calibTriggered = true;
          LeanAngleMeter::calibrateZero();
          #ifdef M5PM1_ADDR
          M5PM1.setVibration(120);
          #endif
          leanDirty = true;
        }
      }
    }

    if (callOn || pairOn || touchEat || fxOn) dragAbort();
    else dragMove(touchX1 - touchX0, touchY1 - touchY0, now);
  } else if (touchDown && now - touchLastSeen > 80) {
    touchDown = false;
    touchHoldAt = 0;
    calibTriggered = false;
    if (callOn || pairOn || touchEat || fxOn) dragAbort();
    else {
      if (page == 2 && abs(touchX1 - touchX0) < 36 && abs(touchY1 - touchY0) < 36) musicTap(touchX0, touchY0);
      if (page == 1 && abs(touchX1 - touchX0) < 36 && abs(touchY1 - touchY0) < 36) weatherTap();
      if (page == 3 && abs(touchX1 - touchX0) < 36 && abs(touchY1 - touchY0) < 36) leanMeterTap(touchX0, touchY0);
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
