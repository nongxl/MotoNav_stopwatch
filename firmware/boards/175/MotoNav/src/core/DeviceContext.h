#pragma once
// 依赖、板级对象、布局常量和共享运行状态。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
#include "Arduino_GFX_Library.h"
#include "../../pin_config.h"
#include <Wire.h>
#include "TouchDrvCSTXXX.hpp"     // v5.2 触摸屏（SensorLib 库，微雪示例里带的）
#include <Preferences.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <driver/rtc_io.h>
#include "../../IdlePowerOff.h"
#include <driver/spi_master.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>
#include "../../HidMediaKeys.h"
#include "../../AppleMediaClient.h"
#include "../../ScreenRotationCommand.h"
#include <BLESecurity.h>       // v6.5 配对验证
#include <miniz.h>            // 罪恶都市轮廓：仅接收地图时解压
#include "../../BootAnimation.h"
#include "../../DragonRadar.h"
int bootBrand = 0;
String pendingBootAck;
#include <esp_mac.h>           // v7.0 读蓝牙地址，拼蓝牙名
#if defined(CONFIG_NIMBLE_ENABLED)
#include "host/ble_gap.h"     // v4.5 蓝牙 2M 模式
#include "host/ble_store.h"   // v6.5 记住的手机
#endif

// ===== 屏幕（和官方 HelloWorld 示例一样的初始化方式）=====
// v2.6 第 7 个参数 true = 总线可共享（快速刷屏要和它轮流使用数据线）
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3, true);
Arduino_CO5300 *panel = new Arduino_CO5300(
  bus, LCD_RESET, 0, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);
// 先画在内存里的“画布”上，画完再一次性推到屏幕（需要 PSRAM：OPI PSRAM）
// v2.2：可以切换画布内存的画布，用来做双缓冲
class SwapCanvas : public Arduino_Canvas {
public:
  SwapCanvas(int16_t w, int16_t h, Arduino_G *out) : Arduino_Canvas(w, h, out) {}
  void setFramebuffer(uint16_t *f) { _framebuffer = f; }
};
SwapCanvas *gfx = new SwapCanvas(LCD_WIDTH, LCD_HEIGHT, panel);
bool needFlush = false;
bool frameIsMap = false;          // v2.2 这一帧是小地图（整帧重画，可以异步刷屏）
int qspiMHz = 40;                 // v2.3 屏幕数据线速度
bool fastFlushOn = true;          // v2.6 快速刷屏


// ===== 颜色（RGB565）=====
#define C_BLACK  0x0000
#define C_WHITE  0xFFFF
#define C_GREEN  0x07E0
#define C_GREY   0x7BEF
#define C_CYAN   0x07FF
#define C_RED    0xF800
#define C_DIM    0x6B4D   // #6B6B6B 预告、非推荐车道用的灰色
#define C_TRACK  0x2945   // #2A2A2A 进度环底色
#define C_YELLOW 0xF623   // #F5C518
#define C_ORANGE 0xFBC3   // #FF7A1A
#define C_SEP    0x31A6   // #333333 车道分隔线
#define C_SIGNRED 0xE104  // #E02020 限速牌红圈

// ===== 屏幕几何：466x466，圆心 (233, 233) =====
#define CX 233
#define RING_R 222

// ===== 布局（y 坐标）=====
#define SPEED_Y      50    // 车速数字顶部
#define ICON_CY      188   // 没有车道时，图标中心（v1.8.2 起只作初始位置，最终按可用区域自动摆放）
#define ICON_CY_LANE 172   // 有车道时，图标中心
#define ICON_TOP     120   // v1.8.2 图标可用区域上沿（车速 km/h 下方）
#define ICON_MAX_W   250   // 图标最大宽度
#define LANES_Y      238   // 车道行顶部
#define DIST_Y       270   // 没有车道时，距离数字顶部（7 号字，高 56）
#define DIST_Y_LANE  278   // 有车道时，距离数字顶部（6 号字，高 48）
#define ROAD_Y       334   // 没有车道时，路名图片顶部
#define ROAD_Y_LANE  334   // 有车道时，路名图片顶部
#define BOTTOM_Y     372   // 底部信息图片顶部（常驻显示）
#define TEXT_R       200   // 文字必须在这个半径以内（圆环内沿约 215）

// ===== 蓝牙：手机端必须用同样的名字和 UUID =====
#define DEVICE_NAME  "MotoNav"
String bleName = DEVICE_NAME;        // v7.0 实际蓝牙名 "MotoNav-XXXX"（XXXX = 蓝牙地址最后两个字节），开机时算好
#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define RX_UUID      "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define TX_UUID      "6e400003-b5a3-f393-e0a9-e50e24dcca9e"   // v2.0 圆屏 → 手机通知
BLECharacteristic *txChar = nullptr;

volatile bool bleConnected = false;

// ===== 接收队列：蓝牙线程收到的每个数据包原样放进队列，主循环再取出处理 =====
#define RXQ_SLOTS 48   // v2.0 加大，接收地图大块数据时不丢包
#define RXQ_MAX   512
struct RxMsg { uint16_t len; uint8_t data[RXQ_MAX]; };
RxMsg rxq[RXQ_SLOTS];
volatile int rxHead = 0, rxTail = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// ===== 导航与行车状态 =====
bool shownConn = false;       // 屏幕上反映的连接状态
bool inNav = false;           // 是否处于导航中
String navDir = "";           // 当前方向
int navDist = 0;              // 当前距离（米）
float navAngle = NAN;         // v1.8 路口实际转弯角度；NAN = 没有，用标准角度
unsigned long lastRxMs = 0;   // 最近一次收到任何消息的时间
bool staleShown = false;      // 是否正在显示“数据中断”提示
bool navDirty = false;        // 导航画面需要重画

int speedKmh = -1;            // 车速，-1 表示还没有
int camLimit = 0, camDist = 0;
bool hasCam = false;          // 前方是否有测速
int laneN = 0;                // 车道数，0 表示不显示
// v3.1 小地图底部的行程信息（"#e,剩余米,到达时间HHMM,红绿灯数"）和前方红绿灯（"#g,距离"）
bool tripOn = false;
bool cruiseOn = false;        // v4.8 巡航模式（没有目的地，只显示地图、车速、电子眼、安全提示）
IdlePowerOffTimer idlePowerOffTimer = {0, false};
IdlePowerSupply idlePowerSupply = {0, false, false};
volatile bool powerOffPending = false;
bool cleanOn = false;         // v4.9 清爽模式：地图照常，去掉红绿灯、电子眼、安全提示、测速牌和行程信息（"#w,1/0"）
int tripRemainM = 0, tripEta = 0, tripLights = 0;
// v7.3 路程进度
#include "../../TripProgress.h"
TripProgress tripProgress;
bool progOn = true;
long progDone = 0;                   // 这趟已经走掉的里程（米）
int progLastRm = -1;                 // 上一次收到的剩余里程
unsigned long progLastAt = 0;
#define progReset() do { progDone = 0; progLastRm = -1; progLastAt = 0; tripProgress.reset(); } while (0)   // 宏：放这么靠前写成函数会打乱 Arduino 自动生成的声明
int lightDist = -1;           // 前方红绿灯距离（米），-1 = 不显示
// v3.5 小地图上的电子眼（手机发 "#c,坐标系,x,y,限速,类型,..."，最多 6 个）
#define MCAM_MAX 10           // v4.8 6 → 10（电子眼 + 道路安全提示）
int mapCamN = 0, mapCamFrame = -1;
int32_t mapCamX[MCAM_MAX], mapCamY[MCAM_MAX];
int mapCamSpd[MCAM_MAX];
int mapCamTyp[MCAM_MAX];      // v3.8 图标编号（手机换算好的，0~15）
int8_t mapCamPref[MCAM_MAX];  // v3.8 图标放好了没有（-1 = 还没放过）
float mapCamPrefA[MCAM_MAX];  // v4.1 类型图标在小摄像头的哪个方向（屏幕角度）
int8_t mapCamPrefK[MCAM_MAX]; // v4.3 放图标时这一处有几个类型图标（个数变了就重新放）
float mapCamLockH[MCAM_MAX];  // v4.0 放图标时的地图朝向：之后固定不动，转弯后（朝向变了且已经转稳）才重新放
uint8_t laneBg[8], laneFr[8]; // 每条车道的背景方向码、推荐方向码（0xF = 不推荐）

// ===== 文字图片（手机渲染好的 4 位灰度图）=====
#define IMG_SLOTS 13           // 0 = 路名，1 = 底部信息，2 = 来电人名字，3/4 = 天气地名和风湿度，5/6 = 音乐歌名和歌手
#define IMG_MAX   8192        // 每张图最多字节数
struct TextImg {
  uint8_t *show;              // 正在显示的图
  uint16_t showW; uint8_t showH;
  uint8_t *recv;              // 正在接收的图
  int recvId; uint16_t recvW; uint8_t recvH; uint8_t recvParts; uint32_t recvMask;
};
TextImg imgs[IMG_SLOTS];
AppleMediaClient appleMedia;
volatile uint32_t mediaConnectionEpoch = 0;
uint32_t lyricImageGeneration = 0; uint16_t lyricImageTitleRevision = 0, lyricImageArtistRevision = 0;
bool musicLyrics = false;                  // #ML,0/1; never persist an enabled default
bool musicDirty = true;
bool musicHave = false, musicPlaying = false;
int musicActions = 0;
int musicPressed = -1, musicAck = 0;
unsigned long musicAckUntil = 0;
// HID adds a second GATT service to the existing authenticated navigation connection.
bool hidMusicMode = false;                  // persisted by #HM,0/1; Android explicitly selects 0
BLEHIDDevice *mediaHid = nullptr;
BLECharacteristic *mediaInput = nullptr;
volatile bool hidSubscribed = false, hidSuspended = false, hidReportMode = true;
volatile uint16_t hidSubscriber = 0xFFFF;
volatile bool hidNotifyAccepted = false;
volatile uint32_t hidSessionEpoch = 0;
uint32_t hidSeenEpoch = 0;
bool hidReadyShown = false, hidReleaseOnReady = true;
String pendingHidStatus, pendingLyricsStatus, pendingMediaStatus;
unsigned long hidInitialReleaseAt = 0;
HidMediaKeys::Pulses hidPulses;
unsigned long hidTapAt = 0;
void hidMediaTick();
bool hidMediaReady();
bool wxDirty = false;                 // v5.3 天气页要重画

// ================= v6.5 蓝牙配对验证 =================
#define PAIR_WINDOW_MS  120000UL      // 开机 2 分钟内可以配对新手机
#define AUTH_TIMEOUT_MS 60000UL       // 连上 60 秒还没通过验证就断开
volatile bool bleAuthed = false;      // 当前连接已加密 + 通过配对码验证
volatile uint16_t bleConnHandle = 0xFFFF;
volatile unsigned long bleConnAt = 0, bleAuthAt = 0;
volatile uint32_t pairCode = 0;       // 要显示的配对码
volatile bool pairReq = false;        // 有新的配对请求（蓝牙线程 → 主循环）
volatile bool pairLocked = false;     // 不在允许配对的时间：不显示配对码
volatile bool pairDone = false;       // 配对结束（成功或失败）
volatile bool pairOk = false;
int authNotifies = 0;                 // "U,1" 已经发了几次
