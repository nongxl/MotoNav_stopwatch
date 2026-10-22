#pragma once

#include <Arduino.h>
#include <Wire.h>
#include <Preferences.h>
#include <SparkFun_BMI270_Arduino_Library.h>
#ifndef ST_MINIMAL
#define ST_MINIMAL 5
#endif
#ifndef ST_DRAGON
#define ST_DRAGON 11
#endif

struct LeanTheme {
  uint16_t accent;      // 核心主题色 (实时 G 读数、高亮)
  uint16_t radarRing1;   // 0.5G 细环
  uint16_t radarRing2;   // 1.0G 标尺环
  uint16_t radarRing3;   // 1.5G 边界环
  uint16_t crosshair;    // 十字准星
  uint16_t gBallGlow;    // G 小球光晕主色
  uint16_t pillBg;       // 胶囊卡片背景
  uint16_t pillBorder;   // 胶囊卡片边框
  uint16_t labelColor;   // 标签文字颜色 (如 "MAX L", "MAX G")
  uint16_t safeZone;     // 0~35° 安全区颜色
  uint16_t warnZone;     // 35~50° 激进区颜色
  uint16_t alertZone;    // >50° 极限区颜色

  static LeanTheme get(int st) {
    LeanTheme t;
    if (st == 1) {
      // 风格 1: 罪恶都市 (Vice City) - 默认高能霓虹风格
      t.accent      = 0x6F5F; // 霓虹亮粉青
      t.radarRing1  = 0x2126; // 幽暗紫青
      t.radarRing2  = 0x39E9; // 霓虹青紫环
      t.radarRing3  = 0x51E9; // 1.5G 边界外环
      t.crosshair   = 0x4A69; // 经典十字
      t.gBallGlow   = 0xF258; // 迈阿密粉紫光晕
      t.pillBg      = 0x18C4; // 深紫灰暗底
      t.pillBorder  = 0x41E9; // 霓虹微边框
      t.labelColor  = 0x8D38; // 柔粉灰标签
      t.safeZone    = 0x6F5F; // 霓虹青绿
      t.warnZone    = 0xF258; // 赛道粉紫
      t.alertZone   = 0xF800; // 极限烈红
    } else if (st == ST_MINIMAL) {
      // 风格 5: 极简黑白 (Minimal Monochrome) - 冷光高对比工业风格
      t.accent      = 0xFFFF; // 纯白高亮
      t.radarRing1  = 0x18C3; // 极深炭灰 0.5G
      t.radarRing2  = 0x3186; // 1.0G 标尺环
      t.radarRing3  = 0x528A; // 1.5G 边界环
      t.crosshair   = 0x4208; // 沉稳十字
      t.gBallGlow   = 0xC618; // 钛银冷光
      t.pillBg      = 0x0821; // 黑曜石暗底
      t.pillBorder  = 0x39E7; // 银灰边框
      t.labelColor  = 0x7BEF; // 灰调标签
      t.safeZone    = 0xAD55; // 银灰
      t.warnZone    = 0xFFFF; // 高亮纯白
      t.alertZone   = 0xF800; // 极限红区
    } else if (st == ST_DRAGON) {
      // 风格 11: 龙珠雷达 (Dragon Radar) - 军工雷达格栅与神龙金光
      t.accent      = 0xFFF0; // 龙珠金黄
      t.radarRing1  = 0x12C2; // 0.5G 暗雷达绿
      t.radarRing2  = 0x2424; // 1.0G 雷达绿
      t.radarRing3  = 0x3DE6; // 1.5G 边界绿环
      t.crosshair   = 0x2444; // 雷达十字准星
      t.gBallGlow   = 0xFDC4; // 龙珠琥珀橙
      t.pillBg      = 0x0921; // 军绿暗底
      t.pillBorder  = 0x2484; // 雷达微边框
      t.labelColor  = 0x75A7; // 雷达绿灰标签
      t.safeZone    = 0x2FE4; // 雷达荧光绿
      t.warnZone    = 0xFDC4; // 龙珠琥珀橙
      t.alertZone   = 0xF800; // 极限红
    } else {
      // 风格 0: 原版 (Classic / Cyber Dark) 及其他通用主题
      t.accent      = 0x07FF; // 科技亮青 (C_CYAN)
      t.radarRing1  = 0x1CE7; // 0.5G 冰蓝灰
      t.radarRing2  = 0x3186; // 1.0G 标尺环
      t.radarRing3  = 0x4A69; // 1.5G 边界环
      t.crosshair   = 0x39E7; // 冰蓝十字
      t.gBallGlow   = 0x07F3; // 赛博亮青光晕
      t.pillBg      = 0x18C3; // 经典暗灰底色
      t.pillBorder  = 0x3186; // 精致微边框
      t.labelColor  = 0x9CD3; // 科技灰蓝标签
      t.safeZone    = 0xB723; // 荧光黄绿
      t.warnZone    = 0xFD20; // 赛道高能橙
      t.alertZone   = 0xF800; // 极限赛道红
    }
    return t;
  }
};

class LeanAngleMeter {
public:
  static BMI270 imu;
  static bool available;
  static bool calibrated;

  // 车身正交基向量 (在传感器坐标系下的表示)
  // StopWatch 垂直或后仰面向骑手安装：
  //   vUp:    垂直车身向上 (默认屏幕 12 点钟方向 +Y)
  //   vRight: 水平车身向右 (默认屏幕 3 点钟方向 +X)
  //   vFwd:   车头前进方向 (默认穿透屏幕向前 -Z)
  static float vUp[3];      // 向上单位向量
  static float vRight[3];   // 向右单位向量
  static float vFwd[3];     // 向前单位向量

  // 实时运算数据
  static float leanAngle;   // 当前倾角（度）：负为左倾，正为右倾
  static float rollRate;    // 当前滚转角速度 (deg/s)
  static float gForward;    // 纵向前向 G 值 (原始)
  static float gRight;      // 横向右向 G 值 (原始)
  static float gTotal;      // 综合水平 G 值
  static float gLongitudinal;// 纵向平滑前向 G 值 (+加速, -刹车)
  static float gLateral;     // 横向平滑右向 G 值 (+右弯, -左弯)

  // 极值统计
  static float maxLeftLean;  // 最大左倾角 (正数保存，如 35°)
  static float maxRightLean; // 最大右倾角 (正数保存，如 38°)
  static float maxG;         // 本次骑行最大 G 值

  static unsigned long lastUpdate;
  static unsigned long calibSuccessUntil;

  static bool begin() {
    // Stopwatch 板载 BMI270 位于内部 I2C (Wire), 地址 0x68
    if (imu.beginI2C(BMI2_I2C_PRIM_ADDR, Wire) != BMI2_OK) {
      Serial.println("[IMU] BMI270 begin failed, retrying 0x69...");
      if (imu.beginI2C(BMI2_I2C_SEC_ADDR, Wire) != BMI2_OK) {
        Serial.println("[IMU] BMI270 not detected");
        available = false;
        return false;
      }
    }
    available = true;
    Serial.println("[IMU] BMI270 initialized successfully");

    loadCalibration();
    if (!calibrated) {
      // 快速采样初始重力向量 (映射为屏幕物理坐标)
      float sy = 0, sx_neg = 0, sz = 0;
      for (int i = 0; i < 5; i++) {
        imu.getSensorData();
        sy     += imu.data.accelY;
        sx_neg += -imu.data.accelX;
        sz     += imu.data.accelZ;
        delay(6);
      }
      float sNorm = sqrtf(sy*sy + sx_neg*sx_neg + sz*sz);
      if (sNorm > 0.5f) {
        updateBasisFromUp(sy, sx_neg, sz);
      }
    }
    resetStats();
    lastUpdate = millis();
    return true;
  }

  static void resetStats() {
    maxLeftLean = 0.0f;
    maxRightLean = 0.0f;
    maxG = 0.0f;
    gLongitudinal = 0.0f;
    gLateral = 0.0f;
  }

  // 根据车身垂直向上向量构建连续正交基 (支持 0° 垂直竖放、45° 斜装、90° 桌面平放等任意安装角)
  static void updateBasisFromUp(float ux, float uy, float uz) {
    float norm = sqrtf(ux * ux + uy * uy + uz * uz);
    if (norm < 0.3f) return;
    vUp[0] = ux / norm;
    vUp[1] = uy / norm;
    vUp[2] = uz / norm;

    // 在 Y-Z 纵向平面的名义前进参考向量 (12点为+Y, 穿透屏幕向前为-Z)
    // 无论垂直立着(vUp≈(0,1,0)->fwd=(0,0,-1)) 还是水平平放(vUp≈(0,0,1)->fwd=(0,1,0))
    // refFwd = (0, vUp[2], -vUp[1]) 天然与 vUp 在 Y-Z 纵向对称面内严格正交！
    float refFwd[3] = {0.0f, vUp[2], -vUp[1]};
    float rfNorm = sqrtf(refFwd[1] * refFwd[1] + refFwd[2] * refFwd[2]);
    if (rfNorm < 0.05f) {
      refFwd[1] = 1.0f; refFwd[2] = 0.0f;
    } else {
      refFwd[1] /= rfNorm; refFwd[2] /= rfNorm;
    }

    // 正交施密特投影: vFwd = refFwd - (refFwd · vUp) * vUp
    float dot = refFwd[0] * vUp[0] + refFwd[1] * vUp[1] + refFwd[2] * vUp[2];
    float px = refFwd[0] - dot * vUp[0];
    float py = refFwd[1] - dot * vUp[1];
    float pz = refFwd[2] - dot * vUp[2];
    float pNorm = sqrtf(px * px + py * py + pz * pz);
    if (pNorm > 0.05f) {
      vFwd[0] = px / pNorm;
      vFwd[1] = py / pNorm;
      vFwd[2] = pz / pNorm;
    }

    // 叉乘得到向右向量: vRight = vFwd × vUp (严格指向屏幕 3 点钟方向 +X)
    vRight[0] = vFwd[1] * vUp[2] - vFwd[2] * vUp[1];
    vRight[1] = vFwd[2] * vUp[0] - vFwd[0] * vUp[2];
    vRight[2] = vFwd[0] * vUp[1] - vFwd[1] * vUp[0];
    float rNorm = sqrtf(vRight[0]*vRight[0] + vRight[1]*vRight[1] + vRight[2]*vRight[2]);
    if (rNorm > 0.001f) {
      vRight[0] /= rNorm; vRight[1] /= rNorm; vRight[2] /= rNorm;
    }
  }

  // 加载校准参数
  static void loadCalibration() {
    Preferences pref;
    pref.begin("motonav_imu", true);
    calibrated = pref.getBool("calibrated", false);
    if (calibrated) {
      vUp[0] = pref.getFloat("up_x", 0.0f);
      vUp[1] = pref.getFloat("up_y", 1.0f);
      vUp[2] = pref.getFloat("up_z", 0.0f);

      vRight[0] = pref.getFloat("rt_x", 1.0f);
      vRight[1] = pref.getFloat("rt_y", 0.0f);
      vRight[2] = pref.getFloat("rt_z", 0.0f);

      vFwd[0] = pref.getFloat("fwd_x", 0.0f);
      vFwd[1] = pref.getFloat("fwd_y", 0.0f);
      vFwd[2] = pref.getFloat("fwd_z", -1.0f);
      Serial.println("[IMU] Calibration loaded from NVS");
    } else {
      updateBasisFromUp(0.0f, 1.0f, 0.0f);
    }
    pref.end();
  }

  // 保存校准参数
  static void saveCalibration() {
    Preferences pref;
    pref.begin("motonav_imu", false);
    pref.putBool("calibrated", true);
    pref.putFloat("up_x", vUp[0]);
    pref.putFloat("up_y", vUp[1]);
    pref.putFloat("up_z", vUp[2]);
    pref.putFloat("rt_x", vRight[0]);
    pref.putFloat("rt_y", vRight[1]);
    pref.putFloat("rt_z", vRight[2]);
    pref.putFloat("fwd_x", vFwd[0]);
    pref.putFloat("fwd_y", vFwd[1]);
    pref.putFloat("fwd_z", vFwd[2]);
    pref.end();
    calibrated = true;
    calibSuccessUntil = millis() + 2500;
    Serial.println("[IMU] Calibration saved to NVS");
  }

  // 零位基准校准（在摩托车直立静止时调用，锁定当前安装角度）
  static bool calibrateZero() {
    if (!available) return false;

    // 多次采样平滑当前安装状态下的重力向量 (转换为屏幕坐标系)
    float axSum = 0, aySum = 0, azSum = 0;
    const int SAMPLES = 30;
    for (int i = 0; i < SAMPLES; i++) {
      imu.getSensorData();
      axSum += imu.data.accelY;     // 屏幕 +X (3点钟方向向右)
      aySum += -imu.data.accelX;    // 屏幕 +Y (12点钟方向向前)
      azSum += imu.data.accelZ;     // 屏幕 +Z (垂直屏幕朝外)
      delay(8);
    }
    float ax = axSum / SAMPLES;
    float ay = aySum / SAMPLES;
    float az = azSum / SAMPLES;

    float norm = sqrtf(ax * ax + ay * ay + az * az);
    if (norm < 0.5f || norm > 1.8f) {
      Serial.println("[IMU] Calibration failed: invalid gravity norm");
      return false;
    }

    // 根据当前静止重力向量解析并锁定任意安装角度下的车身正交基
    updateBasisFromUp(ax, ay, az);
    saveCalibration();
    resetStats();
    leanAngle = 0.0f;
    return true;
  }

  // 周期性更新倾角与 G 值（建议以 30~50Hz 调用）
  static void update() {
    if (!available) return;

    unsigned long now = millis();
    float dt = (now - lastUpdate) / 1000.0f;
    if (dt <= 0.0f || dt > 0.2f) dt = 0.02f;
    lastUpdate = now;

    imu.getSensorData();

    // 关键硬件旋转映射：StopWatch 板载 BMI270 物理芯片相对于屏幕旋转了 90 度
    // 屏幕向右 (+X_scr, 3点钟方向) = +imu.data.accelY
    // 屏幕向前 (+Y_scr, 12点钟方向) = -imu.data.accelX
    // 屏幕朝天 (+Z_scr, 垂直屏幕向外) = +imu.data.accelZ
    float ax = imu.data.accelY;
    float ay = -imu.data.accelX;
    float az = imu.data.accelZ;

    float gx = imu.data.gyroY;
    float gy = -imu.data.gyroX;
    float gz = imu.data.gyroZ;

    // 1. 若用户尚未手动锁定校准，静止时平滑自适应追踪当前安装放置姿态
    // (实现水平桌面平放与垂直立握无缝自适应，左右倾斜效果完全一致)
    if (!calibrated) {
      float aMag = sqrtf(ax*ax + ay*ay + az*az);
      float gMag = sqrtf(gx*gx + gy*gy + gz*gz);
      if (aMag >= 0.85f && aMag <= 1.15f && gMag < 10.0f) {
        static float autoUp[3] = {0.0f, 1.0f, 0.0f};
        const float ADAPT_RATE = 0.05f;
        autoUp[0] += ADAPT_RATE * (ax - autoUp[0]);
        autoUp[1] += ADAPT_RATE * (ay - autoUp[1]);
        autoUp[2] += ADAPT_RATE * (az - autoUp[2]);
        updateBasisFromUp(autoUp[0], autoUp[1], autoUp[2]);
      }
    }

    // 2. 瞬时加速度与角速度投影到车身正交坐标系
    float aUp  = ax * vUp[0]    + ay * vUp[1]    + az * vUp[2];
    float aRt  = ax * vRight[0] + ay * vRight[1] + az * vRight[2];
    float aFwd = ax * vFwd[0]   + ay * vFwd[1]   + az * vFwd[2];

    // 绕前进轴的滚转角速度 (向右倾斜顺时针压弯角速度，右倾为正，左倾为负)
    rollRate = gx * vFwd[0] + gy * vFwd[1] + gz * vFwd[2];

    // 瞬时加速度倾角 (左右侧倾角，右倾为正，左倾为负)
    // 物理：向左倾斜 aRt>0，向右倾斜 aRt<0，在任意安装姿态下均严格有效
    float accRoll = atan2f(-aRt, aUp) * (180.0f / (float)M_PI);

    // 自适应极速跟手互补滤波：
    // 当角度偏差较大（例如手拿大幅倾斜）时动态提高追赶速度，零延迟极速跟手；
    // 当角度稳定微小波动时提升滤波系数消除细碎抖动
    float angleDiff = fabsf(accRoll - leanAngle);
    float alpha = 0.90f;
    if (angleDiff > 10.0f) {
      alpha = 0.70f; // 大角度变化时极速跟手，瞬间响应！
    } else if (angleDiff > 3.0f) {
      alpha = 0.82f;
    }
    leanAngle = alpha * (leanAngle + rollRate * dt) + (1.0f - alpha) * accRoll;

    if (leanAngle > 65.0f) leanAngle = 65.0f;
    if (leanAngle < -65.0f) leanAngle = -65.0f;

    // 3. 真实车辆动态运动 G 值 (根据安装角度正交解耦，重力 1G 支持力完全由 aUp 吸收，不污染运动加速度)
    // 纵向运动推力：加速时向前推(惯性比力 aFwd<0 -> rawLong>0)，刹车减速冲向前(aFwd>0 -> rawLong<0)
    float rawLong = -aFwd;

    // 横向运动推力：消除静态重力倾角伪加速度，还原真实过弯向心力
    float rollRad = leanAngle * (float)M_PI / 180.0f;
    float gravityRt = -sinf(rollRad);            // 倾斜时重力静态反作用力分量
    float dynamicRt = -(aRt - gravityRt);        // 扣除静态重力后的纯动态运动侧向加速度
    float rawLat    = dynamicRt + sinf(rollRad); // 综合过弯抓地摩擦力 G 值

    // 阻尼滤波，兼顾平滑与灵敏响应
    const float G_ALPHA = 0.15f;
    gLongitudinal += G_ALPHA * (rawLong - gLongitudinal);
    gLateral      += G_ALPHA * (rawLat  - gLateral);

    gForward = aFwd;
    gRight   = aRt;

    // 静止直立或静止平放时，rawLong≈0, rawLat≈0 -> 稳定为 0.00 G
    gTotal = sqrtf(gLongitudinal * gLongitudinal + gLateral * gLateral);

    // 更新极值统计
    if (leanAngle > maxRightLean) maxRightLean = leanAngle;
    if (-leanAngle > maxLeftLean) maxLeftLean = -leanAngle;
    if (gTotal > maxG) maxG = gTotal;
  }
};

BMI270 LeanAngleMeter::imu;
bool LeanAngleMeter::available = false;
bool LeanAngleMeter::calibrated = false;

float LeanAngleMeter::vUp[3] = {0.0f, 1.0f, 0.0f};
float LeanAngleMeter::vRight[3] = {1.0f, 0.0f, 0.0f};
float LeanAngleMeter::vFwd[3] = {0.0f, 0.0f, -1.0f};

float LeanAngleMeter::leanAngle = 0.0f;
float LeanAngleMeter::rollRate = 0.0f;
float LeanAngleMeter::gForward = 0.0f;
float LeanAngleMeter::gRight = 0.0f;
float LeanAngleMeter::gTotal = 0.0f;
float LeanAngleMeter::gLongitudinal = 0.0f;
float LeanAngleMeter::gLateral = 0.0f;

float LeanAngleMeter::maxLeftLean = 0.0f;
float LeanAngleMeter::maxRightLean = 0.0f;
float LeanAngleMeter::maxG = 0.0f;

unsigned long LeanAngleMeter::lastUpdate = 0;
unsigned long LeanAngleMeter::calibSuccessUntil = 0;
