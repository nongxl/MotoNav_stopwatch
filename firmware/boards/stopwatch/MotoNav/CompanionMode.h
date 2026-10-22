// SPDX-License-Identifier: AGPL-3.0-or-later

#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <algorithm>
#include <cmath>

#include "AvatarEngine.h"
#include "BoardStopWatchInit.h"
#include "LeanAngleMeter.h"
#include "pin_config.h"

// 声明 MotoNav 全局资源
class SwapCanvas;
extern SwapCanvas *gfx;
extern void flushAsync();
extern void flushSync();
extern bool needFlush;
extern void showPageAnimated(int target, int sx, int sy);

// 适配器：将 AvatarEngine 的绘图调用桥接到 MotoNav 的 SwapCanvas 画布
class AvatarGFXAdapter : public AvatarDisplayAdapter {
 public:
  int width() const override { return LCD_WIDTH; }
  int height() const override { return LCD_HEIGHT; }
  void fillScreen(uint16_t color) override { gfx->fillScreen(color); }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) override {
    gfx->fillRect(x, y, w, h, color);
  }
  void fillCircle(int16_t x, int16_t y, int16_t r, uint16_t color) override {
    gfx->fillCircle(x, y, r, color);
  }
  void fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2,
                    int16_t y2, uint16_t color) override {
    gfx->fillTriangle(x0, y0, x1, y1, x2, y2, color);
  }
  void fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r,
                     uint16_t color) override {
    gfx->fillRoundRect(x, y, w, h, r, color);
  }
  void drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) override {
    gfx->drawLine(x0, y0, x1, y1, color);
  }
  void drawWideLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, float width,
                    uint16_t color) override {
    if (width <= 1.0f) {
      gfx->drawLine(x0, y0, x1, y1, color);
      return;
    }
    const float dx = x1 - x0;
    const float dy = y1 - y0;
    const float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) {
      gfx->fillCircle(x0, y0, static_cast<int16_t>(ceilf(width * 0.5f)), color);
      return;
    }
    const float nx = -dy / len * (width * 0.5f);
    const float ny = dx / len * (width * 0.5f);
    const int16_t p0x = lroundf(x0 + nx);
    const int16_t p0y = lroundf(y0 + ny);
    const int16_t p1x = lroundf(x0 - nx);
    const int16_t p1y = lroundf(y0 - ny);
    const int16_t p2x = lroundf(x1 - nx);
    const int16_t p2y = lroundf(y1 - ny);
    const int16_t p3x = lroundf(x1 + nx);
    const int16_t p3y = lroundf(y1 + ny);
    gfx->fillTriangle(p0x, p0y, p1x, p1y, p2x, p2y, color);
    gfx->fillTriangle(p0x, p0y, p2x, p2y, p3x, p3y, color);
    gfx->fillCircle(x0, y0, static_cast<int16_t>(width * 0.5f), color);
    gfx->fillCircle(x1, y1, static_cast<int16_t>(width * 0.5f), color);
  }
  void startWrite() override {}
  void endWrite() override {}
};

// 伴侣角色定义（预留多角色接口）
enum class CompanionRole : uint8_t {
  KK = 0,  // KK 动态程序化表情角色 (Trentct/m5stack-stopwatch-avatar)
  Count,
};

class CompanionMode {
 public:
  static AvatarEngine avatar;
  static AvatarGFXAdapter adapter;
  static bool initialized;
  static bool vibrationReady;
  static uint32_t vibrationStopsAtMs;
  static CompanionRole currentRole;

  // 手势与交互参数
  static constexpr int16_t kGestureDirectionLockPx = 12;
  static constexpr int16_t kGestureCommitPx = 52;
  static constexpr uint16_t kSwipeTransitionMs = 160;
  enum class GestureAxis : uint8_t { None, Horizontal, Vertical };
  static GestureAxis gestureAxis;
  static bool gestureCommitted;
  static bool touchPressed;
  static int16_t touchStartX;
  static int16_t touchStartY;
  static uint32_t touchDownAtMs;
  static uint32_t lastTouchReleaseAtMs;
  static uint8_t clickCount;

  // IMU 交互参数
  static constexpr uint32_t kImuInteractionIntervalMs = 20;
  static constexpr uint32_t kShakeReleaseDelayMs = 650;
  static constexpr uint32_t kShakeSequenceTimeoutMs = 1500;
  static constexpr uint32_t kShakeMaximumSwingGapMs = 480;
  static constexpr uint32_t kShakeMinimumSwingGapMs = 90;
  static constexpr float kStrongShakeAxisDelta = 0.32f;
  static constexpr float kStrongShakeJerk = 0.50f;
  static constexpr uint8_t kRequiredShakeSwings = 4;
  static constexpr uint16_t kImuCalibrationSamples = 30;

  static uint32_t lastImuInteractionMs;
  static uint32_t shakeQuietStartedMs;
  static uint16_t imuCalibrationCount;
  static float filteredAccelX;
  static float filteredAccelY;
  static float neutralAccelX;
  static float neutralAccelY;
  static float previousAccelX;
  static float previousAccelY;
  static float previousAccelZ;
  static float shakeEnergy;
  static bool shakeReactionActive;
  static uint8_t shakeSwingCount;
  static int8_t previousShakeDirection;
  static uint32_t shakeSequenceStartedMs;
  static uint32_t lastStrongShakeMs;

  // 震动马达控制 (M5IOE1)
  static void setupVibration() {
    if (!StopWatchBoard::ok) return;
    StopWatchBoard::ioe.setPwmFrequency(200);
    vibrationReady = (StopWatchBoard::ioe.setPwmDuty12bit(M5IOE1_PWM_CH1, 0, false, true) == M5IOE1_OK);
  }

  static void stopVibration() {
    if (!vibrationReady) return;
    const uint8_t pwmData[2] = {0, 0x80};
    Wire.beginTransmission(0x4F);
    Wire.write(0x1B);
    Wire.write(pwmData, 2);
    Wire.endTransmission();
    vibrationStopsAtMs = 0;
  }

  static void startVibration(uint8_t strength = 160, uint16_t durationMs = 45) {
    if (!vibrationReady) return;
    const uint16_t duty12 = static_cast<uint16_t>(strength) * 0x0FFF / 0xFF;
    const uint8_t pwmData[2] = {
        static_cast<uint8_t>(duty12 & 0xFF),
        static_cast<uint8_t>(0x80 | ((duty12 >> 8) & 0x0F)),
    };
    Wire.beginTransmission(0x4F);
    Wire.write(0x1B);
    Wire.write(pwmData, 2);
    Wire.endTransmission();
    vibrationStopsAtMs = millis() + durationMs;
  }

  static void updateVibration(uint32_t nowMs) {
    if (vibrationStopsAtMs != 0 && static_cast<int32_t>(nowMs - vibrationStopsAtMs) >= 0) {
      stopVibration();
    }
  }

  static uint32_t lastInteractionAtMs;
  static void noteInteraction() {
    lastInteractionAtMs = millis();
  }

  // 初始化伴侣模式
  static void begin() {
    if (initialized) return;
    setupVibration();
    avatar.begin(&adapter);
    initialized = true;
    lastInteractionAtMs = millis();
    Serial.println("[Companion] Avatar engine initialized");
  }

  // 进入伴侣模式
  static void enter() {
    begin();
    resetShakeSequence();
    gestureAxis = GestureAxis::None;
    gestureCommitted = false;
    touchPressed = false;
    avatar.invalidate();
    noteInteraction();
    startVibration(140, 40);
    Serial.println("[Companion] Entered companion mode (Role: KK)");
  }

  // 绘制第一帧（供滑动换页合成器调用）
  static void drawInitialFrame() {
    begin();
    avatar.renderStatic(millis());
  }

  // 触发指定表情并带轻微震动
  static void trigger(ExpressionId expression, uint32_t nowMs, uint16_t firstTransitionMs = 0) {
    noteInteraction();
    avatar.show(expression, nowMs, true, firstTransitionMs);
    startVibration(150, 40);
    Serial.printf("[Companion] Trigger expression: %s\n", avatar.activeName());
  }

  // 角色切换预留接口（未来支持左右切换角色或 App 蓝牙配置下发）
  static void setRole(CompanionRole role) {
    currentRole = role;
    Serial.printf("[Companion] Role switched to %u\n", static_cast<uint8_t>(role));
  }

  static void nextRole() {
    uint8_t nextIdx = (static_cast<uint8_t>(currentRole) + 1) % static_cast<uint8_t>(CompanionRole::Count);
    setRole(static_cast<CompanionRole>(nextIdx));
  }

  static void previousRole() {
    uint8_t cur = static_cast<uint8_t>(currentRole);
    uint8_t prevIdx = cur == 0 ? (static_cast<uint8_t>(CompanionRole::Count) - 1) : (cur - 1);
    setRole(static_cast<CompanionRole>(prevIdx));
  }

  static bool handleCommand(const String& cmd, uint32_t nowMs) {
    if (cmd.startsWith("#ROLE,")) {
      int r = cmd.substring(6).toInt();
      if (r >= 0 && r < static_cast<int>(CompanionRole::Count)) {
        setRole(static_cast<CompanionRole>(r));
        return true;
      }
    }
    return avatar.showFromCommand(cmd, nowMs);
  }

  // 屏幕触控互动
  static void handleTouch(bool isDown, int16_t touchX, int16_t touchY) {
    const uint32_t nowMs = millis();

    if (isDown) {
      if (!touchPressed) {
        // 刚刚按下
        touchPressed = true;
        touchStartX = touchX;
        touchStartY = touchY;
        touchDownAtMs = nowMs;
        gestureAxis = GestureAxis::None;
        gestureCommitted = false;
        noteInteraction();
      }

      const int deltaX = touchX - touchStartX;
      const int deltaY = touchY - touchStartY;
      if (abs(deltaX) > 4 || abs(deltaY) > 4) {
        noteInteraction();
      }

      // 长按不动（持续超过 650ms 且位移很小）触发生气表情
      if (!gestureCommitted && gestureAxis == GestureAxis::None &&
          (nowMs - touchDownAtMs >= 650) &&
          std::max(abs(deltaX), abs(deltaY)) < kGestureDirectionLockPx) {
        gestureCommitted = true;
        avatar.releaseTouch();
        trigger(ExpressionId::Angry, nowMs);
        return;
      }

      // 判定滑动方向锁定
      if (gestureAxis == GestureAxis::None &&
          std::max(abs(deltaX), abs(deltaY)) >= kGestureDirectionLockPx) {
        gestureAxis = abs(deltaX) >= abs(deltaY) ? GestureAxis::Horizontal
                                                 : GestureAxis::Vertical;
      }

      // 水平滑动：预览与切换相邻表情
      if (gestureAxis == GestureAxis::Horizontal) {
        avatar.releaseTouch();
        const int8_t direction = deltaX > 0 ? -1 : 1;
        avatar.setSwipeOffset(deltaX, 0.0f, gestureCommitted ? 0 : direction);
        if (!gestureCommitted && abs(deltaX) >= kGestureCommitPx) {
          avatar.commitSwipe(direction, nowMs, kSwipeTransitionMs);
          gestureCommitted = true;
          startVibration(125, 35);
          Serial.printf("[Companion] Horizontal swipe commit: %s dx=%d\n",
                        avatar.activeName(), deltaX);
        }
        return;
      }

      // 垂直滑动：上滑 Surprised，下滑 Sleepy
      if (gestureAxis == GestureAxis::Vertical) {
        avatar.releaseTouch();
        avatar.setSwipeOffset(0.0f, deltaY, 0);
        if (!gestureCommitted && abs(deltaY) >= kGestureCommitPx) {
          trigger(deltaY > 0 ? ExpressionId::Sleepy : ExpressionId::Surprised,
                  nowMs, kSwipeTransitionMs);
          gestureCommitted = true;
          Serial.printf("[Companion] Vertical swipe commit: %s dy=%d\n",
                        avatar.activeName(), deltaY);
        }
        return;
      }

      // 正常按住移动：眼珠视线与面部跟随手指
      if (!gestureCommitted) {
        avatar.setTouchTarget(touchX, touchY);
      }
    } else {
      // 手指抬起
      if (touchPressed) {
        touchPressed = false;
        const bool consumed = gestureCommitted || gestureAxis != GestureAxis::None;
        avatar.releaseTouch();
        avatar.releaseSwipe();
        gestureAxis = GestureAxis::None;
        gestureCommitted = false;

        // 若不是滑动手势且按住时间很短，则为点击
        if (!consumed && (nowMs - touchDownAtMs < 400)) {
          if (nowMs - lastTouchReleaseAtMs < 320) {
            clickCount++;
          } else {
            clickCount = 1;
          }
          lastTouchReleaseAtMs = nowMs;

          if (clickCount >= 2) {
            trigger(ExpressionId::Surprised, nowMs);
            clickCount = 0;
          } else {
            trigger(ExpressionId::Happy, nowMs);
          }
        }
      }
    }
  }

  // IMU 六轴互动处理（视线倾斜跟随 + 晃动晕厥检测）
  static void handleImu(uint32_t nowMs) {
    if (nowMs - lastImuInteractionMs < kImuInteractionIntervalMs) return;
    lastImuInteractionMs = nowMs;

    if (!LeanAngleMeter::available) return;
    LeanAngleMeter::imu.getSensorData();

    // BMI270 屏幕坐标系映射：X 对应 screenAccelX, Y 对应 screenAccelY
    const float screenAccelX = LeanAngleMeter::imu.data.accelY;
    const float screenAccelY = LeanAngleMeter::imu.data.accelX;
    const float screenAccelZ = LeanAngleMeter::imu.data.accelZ;
    const float screenGyroX = LeanAngleMeter::imu.data.gyroY;
    const float screenGyroY = LeanAngleMeter::imu.data.gyroX;

    constexpr float kFilterAmount = 0.32f;
    if (imuCalibrationCount == 0) {
      filteredAccelX = screenAccelX;
      filteredAccelY = screenAccelY;
      previousAccelX = screenAccelX;
      previousAccelY = screenAccelY;
      previousAccelZ = screenAccelZ;
    } else {
      filteredAccelX += (screenAccelX - filteredAccelX) * kFilterAmount;
      filteredAccelY += (screenAccelY - filteredAccelY) * kFilterAmount;
    }

    if (imuCalibrationCount < kImuCalibrationSamples) {
      neutralAccelX += screenAccelX / kImuCalibrationSamples;
      neutralAccelY += screenAccelY / kImuCalibrationSamples;
      ++imuCalibrationCount;
    } else {
      // 慢速自适应中立位置；陀螺仪前馈带来灵动注视
      neutralAccelX += (filteredAccelX - neutralAccelX) * 0.00015f;
      neutralAccelY += (filteredAccelY - neutralAccelY) * 0.00015f;
      avatar.setTiltTarget(-(filteredAccelX - neutralAccelX) / 0.28f,
                           -(filteredAccelY - neutralAccelY) / 0.28f,
                           -screenGyroY / 180.0f, screenGyroX / 180.0f);
    }

    const float deltaX = screenAccelX - previousAccelX;
    const float deltaY = screenAccelY - previousAccelY;
    const float deltaZ = screenAccelZ - previousAccelZ;
    const float jerk = fabsf(deltaX) + fabsf(deltaY) + fabsf(deltaZ);
    previousAccelX = screenAccelX;
    previousAccelY = screenAccelY;
    previousAccelZ = screenAccelZ;
    shakeEnergy = shakeEnergy * 0.72f + jerk * 0.28f;

    if (jerk > 0.18f || fabsf(screenGyroX) > 25.0f || fabsf(screenGyroY) > 25.0f) {
      noteInteraction();
    }

    if (imuCalibrationCount < kImuCalibrationSamples) return;

    const float shakeIntensity = std::max(0.0f, std::min(1.0f, (shakeEnergy - 0.055f) / 0.48f));
    const float shakeDirectionX = std::max(-1.0f, std::min(1.0f, -deltaX / 0.28f));
    const float shakeDirectionY = std::max(-1.0f, std::min(1.0f, -deltaY / 0.28f));
    avatar.setShakeTarget(shakeDirectionX, shakeDirectionY, shakeIntensity);

    if (!shakeReactionActive) {
      if (shakeSwingCount > 0 &&
          (nowMs - shakeSequenceStartedMs > kShakeSequenceTimeoutMs ||
           nowMs - lastStrongShakeMs > kShakeMaximumSwingGapMs)) {
        resetShakeSequence();
      }

      if (shakeSwingCount == 0 && jerk >= kStrongShakeJerk) {
        if (fabsf(deltaX) >= kStrongShakeAxisDelta) {
          shakeSwingCount = 1;
          previousShakeDirection = deltaX >= 0.0f ? 1 : -1;
          shakeSequenceStartedMs = nowMs;
          lastStrongShakeMs = nowMs;
        }
      } else if (shakeSwingCount > 0) {
        const int8_t direction = deltaX >= 0.0f ? 1 : -1;
        const uint32_t swingGapMs = nowMs - lastStrongShakeMs;
        const bool strongEnough = jerk >= kStrongShakeJerk &&
                                  fabsf(deltaX) >= kStrongShakeAxisDelta;
        if (strongEnough && direction != previousShakeDirection &&
            swingGapMs >= kShakeMinimumSwingGapMs &&
            swingGapMs <= kShakeMaximumSwingGapMs) {
          ++shakeSwingCount;
          previousShakeDirection = direction;
          lastStrongShakeMs = nowMs;
        }
      }

      if (shakeSwingCount >= kRequiredShakeSwings) {
        shakeReactionActive = true;
        shakeQuietStartedMs = 0;
        resetShakeSequence();
        avatar.play(ExpressionId::Dizzy, nowMs, AvatarEngine::PlaybackMode::Loop, false, 150);
        startVibration(190, 80);
        Serial.printf("[Companion] Repeated horizontal shake -> DIZZY (energy=%.2f)\n", shakeEnergy);
      }
      return;
    }

    if (shakeIntensity > 0.10f) {
      shakeQuietStartedMs = 0;
      return;
    }

    if (shakeQuietStartedMs == 0) {
      shakeQuietStartedMs = nowMs;
    } else if (nowMs - shakeQuietStartedMs >= kShakeReleaseDelayMs) {
      shakeReactionActive = false;
      shakeQuietStartedMs = 0;
      resetShakeSequence();
      if (avatar.activeExpression() == ExpressionId::Dizzy) {
        avatar.show(avatar.baseExpression(), nowMs, false, 260);
      }
      Serial.println("[Companion] IMU shake settled -> Back to base");
    }
  }

  static void resetShakeSequence() {
    shakeSwingCount = 0;
    previousShakeDirection = 0;
    shakeSequenceStartedMs = 0;
    lastStrongShakeMs = 0;
  }

  // 主循环每帧调用
  static void tick() {
    const uint32_t nowMs = millis();
    updateVibration(nowMs);
    handleImu(nowMs);
    avatar.update(nowMs);
    flushAsync();
  }
};

// 静态成员定义
AvatarEngine CompanionMode::avatar;
AvatarGFXAdapter CompanionMode::adapter;
bool CompanionMode::initialized = false;
bool CompanionMode::vibrationReady = false;
uint32_t CompanionMode::vibrationStopsAtMs = 0;
CompanionRole CompanionMode::currentRole = CompanionRole::KK;

CompanionMode::GestureAxis CompanionMode::gestureAxis = CompanionMode::GestureAxis::None;
bool CompanionMode::gestureCommitted = false;
bool CompanionMode::touchPressed = false;
int16_t CompanionMode::touchStartX = 0;
int16_t CompanionMode::touchStartY = 0;
uint32_t CompanionMode::touchDownAtMs = 0;
uint32_t CompanionMode::lastTouchReleaseAtMs = 0;
uint8_t CompanionMode::clickCount = 0;

uint32_t CompanionMode::lastImuInteractionMs = 0;
uint32_t CompanionMode::shakeQuietStartedMs = 0;
uint16_t CompanionMode::imuCalibrationCount = 0;
float CompanionMode::filteredAccelX = 0.0f;
float CompanionMode::filteredAccelY = 0.0f;
float CompanionMode::neutralAccelX = 0.0f;
float CompanionMode::neutralAccelY = 0.0f;
float CompanionMode::previousAccelX = 0.0f;
float CompanionMode::previousAccelY = 0.0f;
float CompanionMode::previousAccelZ = 0.0f;
float CompanionMode::shakeEnergy = 0.0f;
bool CompanionMode::shakeReactionActive = false;
uint8_t CompanionMode::shakeSwingCount = 0;
int8_t CompanionMode::previousShakeDirection = 0;
uint32_t CompanionMode::shakeSequenceStartedMs = 0;
uint32_t CompanionMode::lastStrongShakeMs = 0;
uint32_t CompanionMode::lastInteractionAtMs = 0;
