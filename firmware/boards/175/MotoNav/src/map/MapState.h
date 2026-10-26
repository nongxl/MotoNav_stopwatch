#pragma once
// 地图主题、路线缓冲及位置状态。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// ---- 进度环动画 ----
float ringShown = 0, ringTarget = 0;
bool ringActive = false;
String ringDir = "";

// ================= v2.0 小地图 =================
#define MAP_MAX      (200 * 1024)   // 地图块最大字节数
#define ROUTE_MAX    (96 * 1024)    // 路线最大字节数
#define FEAT_MAX     7200           // 地图块最多要素数
#define ROUTE_PTS    24000          // 路线最多点数
#define MAP_NEAR_M   150            // 近路口转向提示放大；车道单独跟随转弯倒计时
#define MAP_PY       262            // 车头在屏幕上的 y 位置（偏下，多看前方）
#include "../../CircleRowBounds.h"
#include "../../RouteSegmentCache.h"
CircleRowBounds<LCD_WIDTH,LCD_HEIGHT> mapCircleRows;
RouteSegmentCache<256> routeSegmentCache;
#define POLY_PTS     4096           // 单个要素最多点数（画图用的临时数组）
#define BADGE_X      130            // 左下角转向小圆牌
#define BADGE_Y      334
#define BADGE_R      60

// v2.9 地图配色主题；v3.0 加上“走过的路线”颜色（done 内芯、done2 描边）和夜间配色
struct MapTheme { uint16_t block, road, park, water, route, route2, hud, blip, done, done2; };
#define THEME_N 12
const MapTheme THEMES[THEME_N] = {
  {0xDEFB, 0x3186, 0x5DEB, 0x4C9B, 0x249F, 0xFFFF, 0xFFFF, 0xFE80, 0x94B3, 0x5AEC},   // 0 原版
  {0xE6D9, 0x3187, 0x760D, 0x4E3B, 0xF258, 0x78EC, 0x6F5F, 0xF258, 0xA4B4, 0x62CC},   // 1 罪恶都市：暖沙色街区、青绿水面、粉紫路线
  {0x3A08, 0x9D15, 0x4327, 0x2A8F, 0xA2DF, 0x38EC, 0xFFFF, 0xFE87, 0x738F, 0x18E4},   // 2 GTA5：深色地面、浅色道路、紫色路线
  {0x0021, 0x04B6, 0x0103, 0x1847, 0xF815, 0x5007, 0xB7FF, 0xFF20, 0x39EA, 0x10A3},   // 3 赛博霓虹
  {0xEF18, 0xF5CD, 0xC6B3, 0x9E3A, 0xB0A3, 0x5841, 0xFFFF, 0x298F, 0xA490, 0x6B0A},   // 4 复古纸地图
  {0x0000, 0x8410, 0x0000, 0x0000, 0xFFFF, 0x39E7, 0xFFFF, 0xFFFF, 0x528A, 0x2104},   // 5 极简黑白
  {0x094B, 0xDF5F, 0x1A50, 0x2B35, 0xFE87, 0x08E8, 0xFFFF, 0xFE87, 0x6C76, 0x116B},   // 6 蓝图
  {0x8E0E, 0xE7DA, 0x334A, 0x08C4, 0x08C4, 0xE7DA, 0xE7DA, 0x08C4, 0x334A, 0x8E0E},   // 7 掌机像素
  {0x00C1, 0x0B46, 0x0183, 0x0084, 0x57F0, 0x01C2, 0x97F5, 0xCFF9, 0x1AC6, 0x0101},   // 8 雷达
  {0x0863, 0x29AA, 0x08C4, 0x08A6, 0xFD80, 0x3940, 0xFF35, 0xFD80, 0x6AC7, 0x18A1},   // 9 寻宝罗盘
  {0x5B08, 0x8C50, 0x3285, 0x29E8, 0x073F, 0x0188, 0xFFFF, 0xF9E7, 0xA514, 0x3186},   // 10 卫星图（照片没到时的替代色）
  {DragonRadar::DayBackground, 0, 0, 0, 0xFFF0, 0x0941, 0xFFFF, 0xFDC4, 0x7D69, 0x1283}, // 11 龙珠雷达（无底图）
};
// v3.0 夜间配色：地面压暗、道路变成浅色线条，路线保持鲜艳（晚上一眼就能看到）
const MapTheme THEMES_NIGHT[THEME_N] = {
  {0xDEFB, 0x3186, 0x5DEB, 0x4C9B, 0x249F, 0xFFFF, 0xFFFF, 0xFE80, 0x4A8B, 0x0862},   // 0 原版·夜：深蓝灰
  {0x20C7, 0x6271, 0x1207, 0x0A2B, 0xF258, 0x5047, 0x6F5F, 0xF258, 0x7370, 0x1063},   // 1 罪恶都市·夜：深紫街区、霓虹粉路线
  {0x10C3, 0x5AED, 0x1163, 0x1107, 0xAB3F, 0x30AB, 0xEF5D, 0xFE87, 0x4229, 0x0841},   // 2 GTA5·夜：更深的地面
  {0x0021, 0x04B6, 0x0103, 0x1847, 0xF815, 0x5007, 0xB7FF, 0xFF20, 0x39EA, 0x10A3},   // 3 赛博霓虹·夜
  {0x0021, 0x04B6, 0x0103, 0x1847, 0xF815, 0x5007, 0xB7FF, 0xFF20, 0x39EA, 0x10A3},   // 4 复古纸地图·夜 = 霓虹
  {0x0000, 0x8410, 0x0000, 0x0000, 0xFFFF, 0x39E7, 0xFFFF, 0xFFFF, 0x528A, 0x2104},   // 5 极简黑白·夜
  {0x00C7, 0xAE3D, 0x118B, 0x1A50, 0xFE87, 0x00A5, 0xFFFF, 0xFE87, 0x4B52, 0x08E7},   // 6 蓝图·夜
  {0x334A, 0x8E0E, 0x08C4, 0x08C4, 0xE7DA, 0x08C4, 0xE7DA, 0xE7DA, 0x334A, 0x08C4},   // 7 掌机像素·夜
  {0x00C1, 0x0B46, 0x0183, 0x0084, 0x57F0, 0x01C2, 0x97F5, 0xCFF9, 0x1AC6, 0x0101},   // 8 雷达·夜
  {0x0863, 0x29AA, 0x08C4, 0x08A6, 0xFD80, 0x3940, 0xFF35, 0xFD80, 0x6AC7, 0x18A1},   // 9 寻宝罗盘·夜
  {0x5B08, 0x8C50, 0x3285, 0x29E8, 0x073F, 0x0188, 0xFFFF, 0xF9E7, 0xA514, 0x3186},   // 10 卫星图·夜（照片压暗）
  {DragonRadar::NightBackground, 0, 0, 0, 0xFFF0, 0x0941, 0xFFFF, 0xFDC4, 0x7D69, 0x1283}, // 11 龙珠雷达（无底图）
};
int themeIdx = 1;                                  // 默认罪恶都市
bool curveOn = false;                              // v6.1 弯道急缓上色
#define ST_NEON 3
#define ST_PAPER 4
#define ST_MINIMAL 5
#define ST_BLUEPRINT 6
#define ST_PIXEL 7
#define ST_RADAR 8
#define ST_COMPASS 9
#define ST_SAT 10
#define ST_DRAGON 11
bool nightOn = false;                              // v3.0 夜间配色（手机按日出日落发 "#n,1/0"）
int trailMode = 1;                                 // v3.0 走过的路线：1 变灰，2 擦除（"#r,1/2"）
// v6.1 实际用的风格：复古纸地图到了夜间配色时换成霓虹
static inline int styleIdx() { return (nightOn && themeIdx == ST_PAPER) ? ST_NEON : themeIdx; }
#define TH       ((nightOn ? THEMES_NIGHT : THEMES)[themeIdx])
#define C_BLOCK  (TH.block)
#define C_ROADM  (TH.road)
#define C_PARK   (TH.park)
#define C_WATER  (TH.water)
#define C_ROUTE  (TH.route)
#define C_ROUTE2 (TH.route2)

static inline int16_t rd16(const uint8_t *p) { return (int16_t)(p[0] | (p[1] << 8)); }
static inline uint16_t rdu16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline int32_t rd32(const uint8_t *p) { return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24)); }

// ---- 大块数据接收（'M'）----
struct Bulk {
  uint8_t *buf; uint32_t cap;
  int id; uint32_t total; uint16_t parts; uint16_t got; bool done;
  uint8_t mask[512];        // 每片一位，最多 4096 片
};
Bulk bulks[5];              // 下标 1 = 地图块，2 = 路线，3 = 图片地图（v2.5），4 = 原路线（v4.4）

// ---- v4.4 原路线（导入的高德分享路线）：导航时画成一条淡色细线，走错路时能看到原来该走哪 ----
#define GHOST_PTS 6000
#define GHOST_MAX (12 + 4 * GHOST_PTS + 64)
int32_t *ghostX = nullptr, *ghostY = nullptr, *gbBox = nullptr;
int ghostN = 0, ghostFrame = -1;

// ---- v2.5 图片地图（天地图）----
#include "../../RasterContinuity.h"
#define RASTER_MAX_BYTES (500 * 1024)   // 压缩后最大字节数
#define RASTER_MAX_PX    (1300 * 1300)  // 最多像素数
#define RASTER_BUF_BYTES (((1300 + 15) / 16) * ((1300 + 15) / 16) * 128)   // v2.6 按 16×16 小块存放
int rasBW = 0;                          // 每行多少个小块
uint8_t *rasterBuf = nullptr;           // 每像素 4 位颜色编号
bool rasterValid = false;
uint8_t *rasterBackBuf = nullptr;
RasterHeader rasterBackHeader = {};
bool rasterBackValid = false;

int rasterFrame = -1;
int32_t rasX0 = 0, rasY0 = 0;           // 左上角（米）
int rasW = 0, rasH = 0;
float rasMpp = 2.0f;                    // 每像素多少米
unsigned long rasterAt = 0, vectorAt = 0;
bool pendingBuildingCapability = false;
String pendingAck = "";     // 待发给手机的确认

// ---- 地图块（正在显示的）----
uint8_t *mapShow = nullptr;
bool mapValid = false;
int mapFrame = -1;
int32_t mapOx = 0, mapOy = 0;     // 地图块原点（米，手机坐标系）
int mapFeatN = 0;
uint32_t *featOff = nullptr;      // 每个要素的点数据在 mapShow 里的位置
uint16_t *featNp = nullptr;       // 点数
uint8_t *featCls = nullptr;       // 类别（最高位 = 面）
int16_t *featBox = nullptr;       // 外框 minx,miny,maxx,maxy（相对原点，米）

// ---- 路线 ----
int32_t *routeX = nullptr, *routeY = nullptr;
int routeN = 0, routeFrame = -1;
int32_t *rbBox = nullptr;         // 每 32 个点一组的外框
#define RB_STEP 32
#define LIGHT_MAX 600
int32_t *lightX = nullptr, *lightY = nullptr;   // v3.2 路线上的红绿灯（米）
float *lightU = nullptr;                        // v3.2 红绿灯在路线上的位置（段号 + 段内比例）
int lightN = 0;
float routeProg = -1;             // v3.0 车在路线上走到哪了：段号 + 段内比例（-1 = 还没对上）
float *routeCum = nullptr;        // v5.0 每个路线点离起点的路程（米），给路况颜色用
uint8_t *routeCurv = nullptr;     // v6.1 每个路线点的弯急程度：0 直/缓，1 中弯，2 急弯
// v5.0 路况：缓行/拥堵的路段（离起点的米数），"#u,起,止,状态,..."；状态 2 缓行 3 拥堵 4 严重拥堵
#define TRAF_MAX 40
int trafN = 0;
int32_t trafA[TRAF_MAX], trafB[TRAF_MAX];
uint8_t trafS[TRAF_MAX];
// v5.0 提示条：重新规划中（"#y,1/0"）、更快路线（"#h,省几分钟" / "#h"）、GPS 信号弱（"#x,1/0"）
unsigned long rerouteAt = 0, fasterAt = 0;
int fasterMin = 0;
bool gpsWeak = false;
unsigned long routeSearchAt = 0;  // v3.0 上次全路线搜索的时间

// ---- 当前位置 ----
bool mapOn = true;
int posFrame = -1;
float posX = 0, posY = 0, posHdg = 0, posKmh = 0;
unsigned long posAt = 0;
float viewHdg = 0, viewScale = 0.7f;
float corrX = 0, corrY = 0;          // v2.6 位置偏差（米），逐渐减小到 0
unsigned long corrAt = 0;
float hdgRate = 0;                   // v6.7 正在转弯的速度（度/秒，顺时针为正）
int leadMs = 300;                    // v6.7 预判提前量（毫秒）
float viewTargetHdg = 0;             // v6.7 地图朝向要追的目标（预判后的朝向）
