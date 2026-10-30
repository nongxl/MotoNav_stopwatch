#pragma once
#include <Arduino.h>
#include "RasterContinuity.h"
#include "CameraIcons.h"

void drawTrafficLightLab(uint32_t now);
void drawCompassBlip(float px, float py, bool outside);

// ================= v2.0 小地图 =================
#define MAP_MAX      (200 * 1024)   // 地图块最大字节数
#define ROUTE_MAX    (96 * 1024)    // 路线最大字节数
#define FEAT_MAX     7200           // 地图块最多要素数
#define ROUTE_PTS    24000          // 路线最多点数
#define MAP_NEAR_M   150            // v2.1 离路口小于这个距离：圆牌放大、显示车道
#define MAP_PY       262            // 车头在屏幕上的 y 位置（偏下，多看前方）
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
#define COVER_MAX_BYTES (48 * 1024)
#define COVER_SIZE 180
#define COVER_BUF_BYTES (COVER_SIZE * COVER_SIZE * 2)

uint16_t *coverBuf = nullptr;
static uint16_t coverPalette[16];
bool coverValid = false;
#define CROSS_MAX_BYTES (128 * 1024)
#define CROSS_MAX_W 256
#define CROSS_MAX_H 160
#define CROSS_BUF_BYTES (CROSS_MAX_W * CROSS_MAX_H * 2)

static float effectiveMapPy = MAP_PY;
static uint16_t *crossBuf = nullptr;
static uint16_t crossPalette[16];
static int crossW = 0, crossH = 0;
static bool crossActive = false;
static float crossProgress = 0.0f;
static float crossStartProg = 0.0f;
static float crossTargetProg = 0.0f;
static unsigned long crossAnimAt = 0;

void setCrossActive(bool on) {
  if (crossActive == on) return;
  crossActive = on;
  crossStartProg = crossProgress;
  crossTargetProg = on ? 1.0f : 0.0f;
  crossAnimAt = millis();
}
Bulk bulks[7];              // 下标 1 = 地图块，2 = 路线，3 = 图片地图（v2.5），4 = 原路线（v4.4）

// ---- v4.4 原路线（导入的高德分享路线）：导航时画成一条淡色细线，走错路时能看到原来该走哪 ----
#define GHOST_PTS 6000
#define GHOST_MAX (12 + 4 * GHOST_PTS + 64)
int32_t *ghostX = nullptr, *ghostY = nullptr, *gbBox = nullptr;
int ghostN = 0, ghostFrame = -1;

// ---- v2.5 图片地图（天地图）----
#include "RasterContinuity.h"
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

// v6.7 推算 t 时刻（再加 lead 秒）的位置和朝向：按当前车速和转弯速度沿弧线往前推（最多推 1.5 秒）
void predictPose(unsigned long t, float lead, float &x, float &y, float &h) {
  float tau = fminf((t - posAt) / 1000.0f + lead, 1.5f);
  if (tau < 0) tau = 0;
  float v = posKmh / 3.6f;
  float rate = (t - posAt) > 2500 ? 0 : hdgRate;        // 很久没新定位（停车等）：不再按转弯推
  float hEnd = posHdg + rate * tau;
  float hMid = (posHdg + hEnd) * 0.5f * PI / 180;      // 弧线用中间的朝向近似
  x = posX + sinf(hMid) * v * tau;
  y = posY + cosf(hMid) * v * tau;
  h = hEnd;
}
bool viewInit = false;
unsigned long lastMapDraw = 0;

// 画图用的临时数组（内部内存，速度快）
float *tmpX = nullptr, *tmpY = nullptr;

void mapInit() {
  bulks[1].cap = MAP_MAX;   bulks[1].buf = (uint8_t *)ps_malloc(MAP_MAX);
  bulks[2].cap = ROUTE_MAX; bulks[2].buf = (uint8_t *)ps_malloc(ROUTE_MAX);
  bulks[3].cap = RASTER_MAX_BYTES; bulks[3].buf = (uint8_t *)ps_malloc(RASTER_MAX_BYTES);
  bulks[4].cap = GHOST_MAX; bulks[4].buf = (uint8_t *)ps_malloc(GHOST_MAX);          // v4.4
  ghostX = (int32_t *)ps_malloc(sizeof(int32_t) * GHOST_PTS);
  ghostY = (int32_t *)ps_malloc(sizeof(int32_t) * GHOST_PTS);
  gbBox  = (int32_t *)ps_malloc(sizeof(int32_t) * 4 * (GHOST_PTS / RB_STEP + 2));
  rasterBuf = (uint8_t *)ps_malloc(RASTER_BUF_BYTES);
  bulks[5].cap = CROSS_MAX_BYTES; bulks[5].buf = (uint8_t *)ps_malloc(CROSS_MAX_BYTES);
  if (!crossBuf) crossBuf = (uint16_t *)ps_malloc(CROSS_BUF_BYTES);
  bulks[6].cap = COVER_MAX_BYTES; bulks[6].buf = (uint8_t *)ps_malloc(COVER_MAX_BYTES);
  if (!coverBuf) coverBuf = (uint16_t *)ps_calloc(1, COVER_BUF_BYTES);
  for (int t = 1; t <= 6; t++) { bulks[t].id = -1; bulks[t].done = false; }
  mapShow = (uint8_t *)ps_malloc(MAP_MAX);
  featOff = (uint32_t *)ps_malloc(sizeof(uint32_t) * FEAT_MAX);
  featNp  = (uint16_t *)ps_malloc(sizeof(uint16_t) * FEAT_MAX);
  featCls = (uint8_t *)ps_malloc(FEAT_MAX);
  featBox = (int16_t *)ps_malloc(sizeof(int16_t) * 4 * FEAT_MAX);
  routeX = (int32_t *)ps_malloc(sizeof(int32_t) * ROUTE_PTS);
  routeY = (int32_t *)ps_malloc(sizeof(int32_t) * ROUTE_PTS);
  rbBox  = (int32_t *)ps_malloc(sizeof(int32_t) * 4 * (ROUTE_PTS / RB_STEP + 2));
  routeCum = (float *)ps_malloc(sizeof(float) * ROUTE_PTS);                         // v5.0
  routeCurv = (uint8_t *)ps_malloc(ROUTE_PTS);                                        // v6.1
  lightX = (int32_t *)ps_malloc(sizeof(int32_t) * LIGHT_MAX);
  lightY = (int32_t *)ps_malloc(sizeof(int32_t) * LIGHT_MAX);
  lightU = (float *)ps_malloc(sizeof(float) * LIGHT_MAX);
  tmpX = (float *)malloc(sizeof(float) * POLY_PTS);
  tmpY = (float *)malloc(sizeof(float) * POLY_PTS);
}

// 解析刚收齐的地图块，建立索引
void parseMap(const uint8_t *p, uint32_t len) {
  mapValid = false;
  if (len < 12) return;
  int frame = rdu16(p);
  int32_t ox = rd32(p + 2), oy = rd32(p + 6);
  int n = rdu16(p + 10);
  uint32_t pos = 12;
  int cnt = 0;
  for (int i = 0; i < n && cnt < FEAT_MAX; i++) {
    if (pos + 3 > len) break;
    uint8_t cls = p[pos];
    uint16_t np = rdu16(p + pos + 1);
    pos += 3;
    if (pos + (uint32_t)np * 4 > len) break;
    int16_t x0 = 32767, y0 = 32767, x1 = -32768, y1 = -32768;
    for (int k = 0; k < np; k++) {
      int16_t x = rd16(p + pos + 4 * k), y = rd16(p + pos + 4 * k + 2);
      if (x < x0) x0 = x;
      if (x > x1) x1 = x;
      if (y < y0) y0 = y;
      if (y > y1) y1 = y;
    }
    featCls[cnt] = cls; featNp[cnt] = np; featOff[cnt] = pos;
    featBox[4 * cnt] = x0; featBox[4 * cnt + 1] = y0; featBox[4 * cnt + 2] = x1; featBox[4 * cnt + 3] = y1;
    cnt++;
    pos += (uint32_t)np * 4;
  }
  mapFeatN = cnt; mapOx = ox; mapOy = oy; mapFrame = frame;
  mapValid = cnt > 0;
  vectorAt = millis();
}

// v2.5 解析图片地图：u16 坐标系, i32 左上角 x, i32 左上角 y, u16 宽, u16 高, u16 每像素毫米, 然后是压缩的颜色编号
static inline uint16_t blendF(uint16_t a, uint16_t b, uint32_t t);   // v6.1 后面定义
bool rasHasPal = false;                    // v6.1 卫星图自带调色板
uint16_t rasPal[16];

// 罪恶都市局部边界几何；其它风格仍读取原来的 4 位材质图。
uint16_t *rasterVice = nullptr;
size_t rasterAllocatedBytes = RASTER_BUF_BYTES;
bool rasterViceValid = false;
tinfl_decompressor *viceInflater = nullptr;
uint16_t viceEdgePal[7][257];
uint16_t vicePalKey[4] = {0xffff, 0xffff, 0xffff, 0xffff};
// 覆盖率按像素方形与半平面的相交面积计算。利用镜像对称，只需保存九个角度的表。
int8_t viceNX[64], viceNY[64];
int32_t viceOffsets[2][32];
uint16_t viceCoverage[9][257];
bool rasterViceFine = false;
uint8_t viceBaseShift = 13;
uint8_t rasterViceVersion = 0;
const uint16_t *vicePatchData = nullptr;
uint16_t vicePatchCount = 0;

void initViceSampling() {
  static bool ready = false;
  if (ready) return;
  for (int i = 0; i < 64; i++) {
    float a = i * (2.0f * PI / 64.0f);
    viceNX[i] = (int8_t)lroundf(cosf(a) * 127);
    viceNY[i] = (int8_t)lroundf(sinf(a) * 127);
  }
  const float offsets[16] = {0.0f, 0.0625f, 0.125f, 0.1875f,
    0.25f, 0.3125f, 0.375f, 0.4375f, 0.5f, 0.5625f,
    0.625f, 0.6875f, 0.8125f, 1.0f, 1.25f, 1.625f};
  for (int i = 0; i < 32; i++) {
    viceOffsets[0][i] = (i < 16 ? i : i - 32) * 4064;
    viceOffsets[1][i] = (int32_t)lroundf(offsets[i & 15] * (i & 16 ? -32512.0f : 32512.0f));
  }
  for (int angle = 0; angle <= 8; angle++) {
    float a = angle * (PI / 32.0f);
    float major = cosf(a), minor = sinf(a);
    for (int i = 0; i <= 256; i++) {
      float distance = (i - 128) * (1.5f / 256.0f);
      float t = distance + (major + minor) * 0.5f;
      float coverage;
      if (t <= 0) coverage = 0;
      else if (t >= major + minor) coverage = 1;
      else if (minor < 0.00001f) coverage = t / major;
      else if (t < minor) coverage = t * t / (2 * major * minor);
      else if (t <= major) coverage = (t - minor * 0.5f) / major;
      else { float tail = major + minor - t; coverage = 1 - tail * tail / (2 * major * minor); }
      viceCoverage[angle][i] = (uint16_t)max(0, min(256, (int)lroundf(coverage * 256.0f)));
    }
  }
  ready = true;
}

static inline uint8_t viceCoverageAngle(float nx, float ny) {
  int angle = (int)lroundf(atan2f(fabsf(ny), fabsf(nx)) * (32.0f / PI));
  if (angle > 8) angle = 16 - angle;
  return (uint8_t)max(0, min(8, angle));
}


void parseViceGeometry(const uint8_t *p, uint32_t len, uint32_t pos, int bw, int bh) {
  rasterViceValid = false;
  vicePatchCount = 0; vicePatchData = nullptr;
  if (pos <= len && len - pos >= 8 && p[pos] == 'V' && p[pos + 1] == 'C' && p[pos + 2] == 'E' &&
      p[pos + 3] >= 1 && p[pos + 3] <= 3) {
    uint8_t version = p[pos + 3];
    uint32_t packed = (uint32_t)rd32(p + pos + 4);
    uint32_t header = version == 3 ? 12 : 8;
    if (len - pos < header) return;
    uint16_t patchCount = version == 3 ? rdu16(p + pos + 8) : 0;
    if (patchCount > 2047 || (version == 3 && rdu16(p + pos + 10) != 0)) return;
    size_t cellsBytes = (size_t)bw * bh * 512;
    size_t needed = cellsBytes + (size_t)patchCount * 8;
    if (packed == len - pos - header && needed <= RASTER_BUF_BYTES * 4 + 2047 * 8) {
      if (rasterAllocatedBytes != needed) {
        free(rasterBuf);
        rasterBuf = (uint8_t *)ps_malloc(needed);
        rasterAllocatedBytes = rasterBuf ? needed : 0;
      }
      rasterVice = (uint16_t *)rasterBuf;
      if (!viceInflater) viceInflater = (tinfl_decompressor *)malloc(sizeof(tinfl_decompressor));
      if (rasterVice && rasterAllocatedBytes == needed && viceInflater) {
        tinfl_init(viceInflater);
        size_t inBytes = packed, outBytes = needed;
        tinfl_status result = tinfl_decompress(viceInflater, p + pos + header, &inBytes,
            (uint8_t *)rasterVice, (uint8_t *)rasterVice, &outBytes,
            TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
        rasterViceValid = result == TINFL_STATUS_DONE && inBytes == packed && outBytes == needed;
        const uint16_t *vertices = (const uint16_t *)(rasterBuf + cellsBytes);
        if (rasterViceValid) {
          for (uint32_t i = 0; i < (uint32_t)patchCount * 4; i++) {
            uint16_t v = vertices[i];
            if ((v & 15) + ((v >> 4) & 15) + ((v >> 8) & 15) + (v >> 12) != 15) {
              rasterViceValid = false; break;
            }
          }
        }
        if (rasterViceValid) {
          rasterViceVersion = version;
          rasterViceFine = version >= 2;
          viceBaseShift = rasterViceFine ? 14 : 13;
          vicePatchCount = patchCount; vicePatchData = patchCount ? vertices : nullptr;
        }
      }
    }
  }
  if (!rasterViceValid) rasterVice = nullptr;
}

void prepareVicePalette(const uint16_t *pal) {
  static bool initialized = false;
  initViceSampling();
  bool changed = !initialized;
  for (int i = 0; i < 4; i++) if (vicePalKey[i] != pal[i]) changed = true;
  if (!changed) return;
  const uint8_t pairA[7] = {0, 0, 0, 0, 1, 1, 2};
  const uint8_t pairB[7] = {0, 1, 2, 3, 2, 3, 3};
  for (int p = 1; p <= 6; p++) for (int a = 0; a <= 256; a++)
    viceEdgePal[p][a] = blendF(pal[pairA[p]], pal[pairB[p]], a);
  memcpy(vicePalKey, pal, sizeof(vicePalKey));
  initialized = true;
}

void parseRasterLegacy(const uint8_t *p, uint32_t len) {
  rasterValid = false;
  rasterViceValid = false;
  if (len < 16) return;
  int frame = rdu16(p);
  int32_t x0 = rd32(p + 2), y0 = rd32(p + 6);
  int w = rdu16(p + 10), h = rdu16(p + 12), mm = rdu16(p + 14);
  bool hasPal = (mm & 0x8000) != 0;
  mm &= 0x7FFF;
  uint16_t palNew[16];
  if (hasPal) {
    if (len < 48) return;
    for (int i = 0; i < 16; i++) palNew[i] = rdu16(p + 16 + 2 * i);
  }
  uint32_t n = (uint32_t)w * h;
  if (w == 0 || h == 0 || n > RASTER_MAX_PX || mm == 0) return;
  int bw = (w + 15) >> 4, bh = (h + 15) >> 4;
  size_t legacyBytes = (size_t)bw * bh * 128;
  if (legacyBytes > RASTER_BUF_BYTES) return;
  uint32_t dataStart = hasPal ? 48 : 16;
  // 先检查普通图的游程长度，找到扩展的位置；不另存一份解压后的普通图。
  uint32_t pos = dataStart, i = 0;
  while (pos < len && i < n) {
    uint8_t b = p[pos++], r = b >> 4;
    uint32_t run = r + 1;
    if (r == 15) { if (pos >= len) return; run = 16 + p[pos++]; }
    if (run > n - i) return;
    i += run;
  }
  if (i != n) return;
  if (!hasPal) parseViceGeometry(p, len, pos, bw, bh);
  if (!rasterViceValid) {
    // 不带扩展的地图、卫星图或扩展解码失败，回到原来的 4 位格式。
    if (!rasterBuf || rasterAllocatedBytes != RASTER_BUF_BYTES) {
      free(rasterBuf);
      rasterBuf = (uint8_t *)ps_malloc(RASTER_BUF_BYTES);
      rasterAllocatedBytes = rasterBuf ? RASTER_BUF_BYTES : 0;
    }
    rasterVice = nullptr;
    if (!rasterBuf) return;
    memset(rasterBuf, 0, legacyBytes);
    pos = dataStart; i = 0;
    int x = 0, y = 0;
    while (i < n) {
      uint8_t b = p[pos++], c = b & 15, r = b >> 4;
      uint32_t run = r + 1;
      if (r == 15) run = 16 + p[pos++];
      if (c) {
        for (uint32_t k = 0; k < run; k++) {
          uint32_t a = ((uint32_t)((y >> 4) * bw + (x >> 4)) << 8) | ((y & 15) << 4) | (x & 15);
          rasterBuf[a >> 1] |= (a & 1) ? (c << 4) : c;
          if (++x == w) { x = 0; y++; }
        }
      } else {
        x += run;
        while (x >= w) { x -= w; y++; }
      }
      i += run;
    }
  }
  rasBW = bw;
  rasX0 = x0; rasY0 = y0; rasW = w; rasH = h; rasMpp = mm / 1000.0f; rasterFrame = frame;
  rasHasPal = hasPal;
  if (hasPal) memcpy(rasPal, palNew, sizeof(rasPal));
  rasterValid = true;
  rasterAt = millis();
}

// 普通地图先在备用缓冲解码，成功后交换；上一块保留，用来补齐新块的覆盖边缘。
bool parseRaster(const uint8_t *p, uint32_t len) {
  RasterHeader probe;
  const uint8_t *src = p;
  uint32_t srcLen = len;
  uint8_t *unpacked = nullptr;
  if (rasterHeaderCheck(p, len, probe, RASTER_MAX_PX, RASTER_BUF_BYTES) && probe.deflate) {
    if (len < probe.dataStart + 6) return false;
    uint32_t rawRleLen = rasterU32(p + probe.dataStart);
    uint32_t compLen = len - probe.dataStart - 4;
    if (rawRleLen == 0 || probe.dataStart + rawRleLen > RASTER_MAX_BYTES) return false;
    unpacked = (uint8_t *)ps_malloc(probe.dataStart + rawRleLen);
    if (!unpacked) return false;
    if (!viceInflater) viceInflater = (tinfl_decompressor *)malloc(sizeof(tinfl_decompressor));
    if (!viceInflater) { free(unpacked); return false; }
    memcpy(unpacked, p, probe.dataStart);
    uint16_t cleanFlags = rasterU16(p + 14) & ~RASTER_FLAG_DEFLATE;
    unpacked[14] = (uint8_t)(cleanFlags & 0xFF);
    unpacked[15] = (uint8_t)(cleanFlags >> 8);
    tinfl_init(viceInflater);
    size_t inBytes = compLen, outBytes = rawRleLen;
    tinfl_status status = tinfl_decompress(viceInflater, p + probe.dataStart + 4, &inBytes,
        unpacked + probe.dataStart, unpacked + probe.dataStart, &outBytes,
        TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    if (status != TINFL_STATUS_DONE || inBytes != compLen || outBytes != rawRleLen) {
      free(unpacked);
      return false;
    }
    src = unpacked;
    srcLen = probe.dataStart + rawRleLen;
  }
  RasterHeader next;
  if (!rasterCheck(src,srcLen,next,RASTER_MAX_PX,RASTER_BUF_BYTES)) {
    if (unpacked) free(unpacked);
    return false;
  }
  if (next.dataEnd != srcLen) {
    if (unpacked) { free(unpacked); return false; }
    // 保留既有轮廓扩展的兼容路径；新 App 只发送无扩展的普通底图。
    if (len-next.dataEnd<8 || p[next.dataEnd]!='V' || p[next.dataEnd+1]!='C' || p[next.dataEnd+2]!='E') return false;
    rasterBackValid=false;
    parseRasterLegacy(p,len);
    return rasterValid;
  }
  if (next.delta) {
    bool canDelta = rasterValid && !rasterViceValid && !rasHasPal &&
        rasterFrame == next.frame && rasW == next.width && rasH == next.height &&
        (uint16_t)lroundf(rasMpp * 1000) == next.mm &&
        rasX0 == next.refX0 && rasY0 == next.refY0 &&
        rasterAllocatedBytes == RASTER_BUF_BYTES && rasterBuf != nullptr;
    if (!canDelta) {
      if (unpacked) free(unpacked);
      return false;
    }
  }
  if (!rasterBackBuf) rasterBackBuf=(uint8_t *)ps_malloc(RASTER_BUF_BYTES);
  if (!rasterBackBuf) {
    if (unpacked) free(unpacked);
    return false;
  }
  rasterDecode(src,next,rasterBackBuf,next.delta ? rasterBuf : nullptr);
  if (unpacked) free(unpacked);
  RasterHeader old = {};
  old.frame=rasterFrame; old.x0=rasX0; old.y0=rasY0; old.width=rasW; old.height=rasH;
  old.mm=(uint16_t)lroundf(rasMpp*1000); old.blocksWide=rasBW;
  bool keep=rasterValid && !rasterViceValid && !rasHasPal && !next.palette &&
      rasterFrame==next.frame && old.mm==next.mm && rasterAllocatedBytes==RASTER_BUF_BYTES;
  uint8_t *oldPixels=rasterBuf;
  rasterBuf=rasterBackBuf;
  rasterBackBuf=oldPixels;
  if (rasterAllocatedBytes!=RASTER_BUF_BYTES) { free(rasterBackBuf); rasterBackBuf=nullptr; keep=false; }
  rasterAllocatedBytes=RASTER_BUF_BYTES;
  rasterBackHeader=old; rasterBackValid=keep;
  rasterVice=nullptr; rasterViceValid=false; vicePatchData=nullptr; vicePatchCount=0;
  rasBW=next.blocksWide; rasX0=next.x0; rasY0=next.y0; rasW=next.width; rasH=next.height;
  rasMpp=next.mm/1000.0f; rasterFrame=next.frame; rasHasPal=next.palette;
  if (next.palette) for (int i=0;i<16;i++) rasPal[i]=rasterU16(p+16+2*i);
  rasterValid=true; rasterAt=millis();
  return true;
}

// ---- v5.7 第二个核心帮忙画：把逐行的计算分给两个核心（各画一半的行）----
TaskHandle_t helperTask = nullptr;
SemaphoreHandle_t helpGo = nullptr, helpDone = nullptr;
void (*volatile helpFn)(int y0, int y1, int step) = nullptr;
volatile int helpY0 = 0, helpY1 = 0;

void helperLoop(void *) {
  for (;;) {
    xSemaphoreTake(helpGo, portMAX_DELAY);
    if (helpFn) helpFn(helpY0, helpY1, 1);
    xSemaphoreGive(helpDone);
  }
}

void helperInit() {
  helpGo = xSemaphoreCreateBinary();
  helpDone = xSemaphoreCreateBinary();
  // v5.8 优先级比推屏任务高：画底图只要几毫秒，推屏晚一点开始没关系（它不卡画面）
  if (xTaskCreatePinnedToCore(helperLoop, "help", 4096, nullptr, 3, &helperTask, 0) != pdPASS) helperTask = nullptr;   // v5.8 优先级 1 → 3
}

// 把 [y0, y1) 这些行分给两个核心：v6.0 主核心画上半，另一个核心画下半（圆形的东西上下对称，活一样多）
void parallelRows(void (*fn)(int, int, int), int y0, int y1) {
  if (!helperTask) { fn(y0, y1, 1); return; }
  int mid = (y0 + y1) / 2;
  helpFn = fn; helpY0 = mid; helpY1 = y1;
  xSemaphoreGive(helpGo);
  fn(y0, mid, 1);
  xSemaphoreTake(helpDone, portMAX_DELAY);
}

// v2.5 画图片地图：屏幕上每个像素反算到地图图片上的位置，取颜色（只画圆环以内）
// v5.7 改成按行分给两个核心画（参数放在全局变量里）
struct { float cx, cy, sc, cs, sn; uint16_t pal[16]; uint16_t *fb; bool pix2; int32_t viceGain; int viceRotation; } RM;
#define PIXB 3                                      // v6.1 掌机像素：3×3 个屏幕像素当一个大像素

void rasterRows(int y0, int y1, int step) {
  uint16_t *fb = RM.fb;
  const uint16_t *pal = RM.pal;
  float cx = RM.cx, cy = RM.cy, sc = RM.sc, cs = RM.cs, sn = RM.sn;
  float inv = 1.0f / sc, kpx = inv / rasMpp;
  int32_t DU = (int32_t)(cs * kpx * 65536.0f), DV = (int32_t)(sn * kpx * 65536.0f);
  const bool useBack=rasterBackValid && rasterBackHeader.frame==rasterFrame && !rasHasPal && !rasterViceValid;
  const int backU=useBack ? (int)lroundf((rasX0-rasterBackHeader.x0)/rasMpp) : 0;
  const int backV=useBack ? (int)lroundf((rasterBackHeader.y0-rasY0)/rasMpp) : 0;
  const float R = 212;
  for (int y = y0; y < y1; y += step) {
    float dyc = y + 0.5f - CX;
    if (RM.pix2 && (y % PIXB) && y > y0 && fabsf(dyc) < R - 2) {  // v6.1 掌机像素：每 3 行只算第一行，其余照抄
      float half = sqrtf(R * R - dyc * dyc);
      int xa = max(0, (int)(CX - half)), xb = min(LCD_WIDTH - 1, (int)(CX + half));
      memcpy(fb + y * LCD_WIDTH + xa, fb + (y - 1) * LCD_WIDTH + xa, (xb - xa + 1) * 2);
      continue;
    }
    if (fabsf(dyc) >= R) continue;
    float half = sqrtf(R * R - dyc * dyc);
    int xa = max(0, (int)(CX - half)), xb = min(LCD_WIDTH - 1, (int)(CX + half));
    if (RM.pix2) xa -= xa % PIXB;                       // 掌机像素：列也按 3 个一组对齐
    float rx = (xa + 0.5f - CX) * inv, ry = (effectiveMapPy - (y + 0.5f)) * inv;
    float dx = rx * cs + ry * sn, dy = -rx * sn + ry * cs;
    float u = (cx + dx - rasX0) / rasMpp, v = (rasY0 - (cy + dy)) / rasMpp;
    int32_t U = (int32_t)(u * 65536.0f), V = (int32_t)(v * 65536.0f);
    uint16_t *row = fb + y * LCD_WIDTH;
    int xstep = RM.pix2 ? PIXB : 1;
    if (RM.pix2) { DU *= PIXB; DV *= PIXB; }
    for (int x = xa; x <= xb; x += xstep) {
      int32_t ui = U >> 16, vi = V >> 16;
      uint8_t idx = 0;
      if ((uint32_t)ui < (uint32_t)rasW && (uint32_t)vi < (uint32_t)rasH) {
        uint32_t k = ((uint32_t)((vi >> 4) * rasBW + (ui >> 4)) << 8) | ((vi & 15) << 4) | (ui & 15);
        if (rasterViceValid) {
          idx = (rasterVice[k] >> viceBaseShift) & 3;
        } else {
          uint8_t b = rasterBuf[k >> 1];
          idx = (k & 1) ? (b >> 4) : (b & 15);
        }
      } else if (useBack) {
        rasterRead(rasterBackBuf,rasterBackHeader,ui+backU,vi+backV,idx);
      }
      row[x] = pal[idx];
      for (int k = 1; k < xstep && x + k <= xb; k++) row[x + k] = pal[idx];
      U += DU; V += DV;
    }
    if (RM.pix2) { DU /= PIXB; DV /= PIXB; }
  }
}

static inline uint16_t vicePatchColor(uint16_t cell, int fx, int fy) {
  uint16_t base = RM.pal[(cell >> 14) & 3];
  uint32_t index = cell & 2047;
  if (index >= vicePatchCount || !vicePatchData) return base;
  const uint16_t *q = vicePatchData + index * 4;
  int tx = max(0, min(256, fx + 128)), ty = max(0, min(256, fy + 128));
  uint32_t r = 0, g = 0, b = 0;
  for (int c = 0; c < 4; c++) {
    int shift = c * 4;
    int a = (q[0] >> shift) & 15, e = (q[1] >> shift) & 15;
    int f = (q[2] >> shift) & 15, d = (q[3] >> shift) & 15;
    int top = a * 256 + (e - a) * tx, bottom = f * 256 + (d - f) * tx;
    uint32_t mass = top * (256 - ty) + bottom * ty;
    uint16_t color = RM.pal[c];
    r += (color >> 11) * mass;
    g += ((color >> 5) & 63) * mass;
    b += (color & 31) * mass;
  }
  // 每个顶点的四材质权重之和固定为 15；双线性权重之和固定为 65536。
  return (uint16_t)((((r + 491520) / 983040) << 11) |
      (((g + 491520) / 983040) << 5) | ((b + 491520) / 983040));
}

static inline bool viceReadColor(int x, int y, int fx, int fy, uint16_t &color) {
  if ((uint32_t)x >= (uint32_t)rasW || (uint32_t)y >= (uint32_t)rasH) return false;
  uint32_t k = ((uint32_t)((y >> 4) * rasBW + (x >> 4)) << 8) | ((y & 15) << 4) | (x & 15);
  uint16_t cell = rasterVice[k];
  int pair = (cell >> 11) & 7;
  color = RM.pal[(cell >> 14) & 3];
  if (pair == 7) { color = vicePatchColor(cell, fx, fy); return true; }
  if (pair == 0) return true;
  int direction = cell & 63, offset = (cell >> 6) & 31;
  int distance = viceNX[direction] * fx + viceNY[direction] * fy + viceOffsets[1][offset];
  int index = 128 + (int)((distance * RM.viceGain) >> 16);
  index = max(0, min(256, index));
  int angle = (direction - RM.viceRotation) & 15;
  if (angle > 8) angle = 16 - angle;
  color = viceEdgePal[pair][viceCoverage[angle][index]];
  return true;
}

static inline uint16_t viceJoinColor(uint16_t color, int x, int y, int qx, int qy) {
  // 仅在格子交界的 1/8 源像素带内过渡；内部轮廓保持原来的清晰度。
  int sx = qx < 32 ? -1 : (qx > 224 ? 1 : 0);
  int sy = qy < 32 ? -1 : (qy > 224 ? 1 : 0);
  int wx = sx < 0 ? (32 - qx) * 4 : (sx > 0 ? (qx - 224) * 4 : 0);
  int wy = sy < 0 ? (32 - qy) * 4 : (sy > 0 ? (qy - 224) * 4 : 0);
  int fx = qx - 128, fy = qy - 128;
  uint16_t bx = color, by = color, diagonal = color;
  if (wx && !viceReadColor(x + sx, y, fx - sx * 256, fy, bx)) wx = 0;
  if (wy && !viceReadColor(x, y + sy, fx, fy - sy * 256, by)) wy = 0;
  if (!wx) return wy ? blendF(color, by, wy) : color;
  if (!wy) return blendF(color, bx, wx);
  if (!viceReadColor(x + sx, y + sy, fx - sx * 256, fy - sy * 256, diagonal)) diagonal = by;
  uint32_t weights[4] = {(uint32_t)((256 - wx) * (256 - wy)),
    (uint32_t)(wx * (256 - wy)), (uint32_t)((256 - wx) * wy), (uint32_t)(wx * wy)};
  uint16_t colors[4] = {color, bx, by, diagonal};
  uint32_t r = 0, g = 0, b = 0;
  for (int i = 0; i < 4; i++) {
    r += (colors[i] >> 11) * weights[i];
    g += ((colors[i] >> 5) & 63) * weights[i];
    b += (colors[i] & 31) * weights[i];
  }
  return (uint16_t)((((r + 32768) >> 16) << 11) |
      (((g + 32768) >> 16) << 5) | ((b + 32768) >> 16));
}

void rasterViceRows(int y0, int y1, int step) {
  uint16_t *fb = RM.fb;
  const uint16_t *pal = RM.pal;
  const uint16_t *cells = rasterVice;
  float inv = 1.0f / RM.sc, kpx = inv / rasMpp;
  int32_t du = (int32_t)(RM.cs * kpx * 65536.0f), dv = (int32_t)(RM.sn * kpx * 65536.0f);
  const int32_t gain = RM.viceGain;
  const int pairShift = rasterViceFine ? 11 : 10;
  const int offsetShift = rasterViceFine ? 6 : 5;
  const int baseShift = viceBaseShift;
  const int32_t *offsets = viceOffsets[rasterViceFine ? 1 : 0];
  const bool fine = rasterViceFine;
  const bool joins = rasterViceVersion == 3;
  const int rotation = RM.viceRotation;
  for (int y = y0; y < y1; y += step) {
    float dyc = y + 0.5f - CX;
    if (fabsf(dyc) >= 212) continue;
    float half = sqrtf(212.0f * 212.0f - dyc * dyc);
    int xa = max(0, (int)(CX - half)), xb = min(LCD_WIDTH - 1, (int)(CX + half));
    float rx = (xa + 0.5f - CX) * inv, ry = (effectiveMapPy - (y + 0.5f)) * inv;
    float dx = rx * RM.cs + ry * RM.sn, dy = -rx * RM.sn + ry * RM.cs;
    int32_t u = (int32_t)(((RM.cx + dx - rasX0) / rasMpp) * 65536.0f);
    int32_t v = (int32_t)(((rasY0 - (RM.cy + dy)) / rasMpp) * 65536.0f);
    uint16_t *out = fb + y * LCD_WIDTH;
    for (int x = xa; x <= xb; x++, u += du, v += dv) {
      int ui = u >> 16, vi = v >> 16;
      uint16_t color = pal[0];
      if ((uint32_t)ui < (uint32_t)rasW && (uint32_t)vi < (uint32_t)rasH) {
        uint32_t k = ((uint32_t)((vi >> 4) * rasBW + (ui >> 4)) << 8) | ((vi & 15) << 4) | (ui & 15);
        uint16_t cell = cells[k];
        uint32_t pair = (cell >> pairShift) & 7;
        color = pal[(cell >> baseShift) & 3];
        if (pair > 0 && (pair <= 6 || joins)) {
          int qx = (u >> 8) & 255, qy = (v >> 8) & 255;
          int fx = qx - 128, fy = qy - 128;
          if (pair <= 6) {
            int direction = fine ? (cell & 63) : ((cell & 31) << 1);
            int offset = (cell >> offsetShift) & 31;
            int distance = viceNX[direction] * fx + viceNY[direction] * fy + offsets[offset];
            int index = 128 + (int)((distance * gain) >> 16);
            index = max(0, min(256, index));
            int angle = (direction - rotation) & 15;
            if (angle > 8) angle = 16 - angle;
            color = viceEdgePal[pair][viceCoverage[angle][index]];
          } else {
            color = vicePatchColor(cell, fx, fy);
          }
          if (joins && (qx < 32 || qx > 224 || qy < 32 || qy > 224))
            color = viceJoinColor(color, ui, vi, qx, qy);
        }
      }
      out[x] = color;
    }
  }
}

uint32_t rasMsAcc = 0;                     // v5.7 统计：底图用时
void drawRasterMap(float cx, float cy, float sc, float cs, float sn) {
  unsigned long t0 = millis();
  RM.fb = gfx->getFramebuffer();
  if (rasHasPal) {                                     // v6.1 卫星图：用照片自带的颜色（夜间压暗）
    for (int i = 0; i < 16; i++) RM.pal[i] = (themeIdx != 0 && nightOn) ? blendF(rasPal[i], C_BLACK, 110) : rasPal[i];
  } else {
    for (int i = 0; i < 16; i++) RM.pal[i] = C_BLOCK;
    RM.pal[1] = C_ROADM; RM.pal[2] = C_PARK; RM.pal[3] = C_WATER;
    RM.pal[4] = blendF(C_BLOCK,C_ROADM,64);   // 俯视屋顶
    RM.pal[5] = blendF(C_BLOCK,C_ROADM,112);  // 屋顶轮廓
    RM.pal[6] = RM.pal[4]; RM.pal[7] = RM.pal[5]; // 保留原绿地类别的屋顶
  }
  if (styleIdx() == ST_MINIMAL) {
    for (int i=0;i<16;i++) RM.pal[i]=0x0000;
    RM.pal[1]=0x8410; // independent monochrome road mask; no building/park/water layer
  }
  RM.pix2 = styleIdx() == ST_PIXEL;
  RM.cx = cx; RM.cy = cy; RM.sc = sc; RM.cs = cs; RM.sn = sn;
  if (styleIdx() == 1 && !rasHasPal && rasterViceValid) {
    prepareVicePalette(RM.pal);
    // 按方形像素的面积覆盖率过渡，斜边的支撑范围自然扩展到约 1.414 像素。
    RM.viceGain = (int32_t)(sc * rasMpp * (65536.0f / (127.0f * 1.5f)));
    RM.viceRotation = (int)lroundf(atan2f(sn, cs) * (32.0f / PI));
    parallelRows(rasterViceRows, (int)(CX - 213), (int)(CX + 213));
  } else {
    parallelRows(rasterRows, (int)(CX - 213), (int)(CX + 213));
  }
  rasMsAcc += millis() - t0;
}

void parseRoute(const uint8_t *p, uint32_t len) {
  routeN = 0;
  if (len < 12) return;
  int frame = rdu16(p);
  int32_t x = rd32(p + 2), y = rd32(p + 6);
  int n = rdu16(p + 10);
  if (n > ROUTE_PTS) n = ROUTE_PTS;
  if (n < 1 || 12 + (uint32_t)(n - 1) * 4 > len) return;
  routeX[0] = x; routeY[0] = y;
  for (int i = 1; i < n; i++) {
    x += rd16(p + 12 + 4 * (i - 1)); y += rd16(p + 14 + 4 * (i - 1));
    routeX[i] = x; routeY[i] = y;
  }
  // 每 32 个点一组算外框（相邻组共用端点，保证线段完整）
  for (int b = 0; b * RB_STEP < n - 1; b++) {
    int s = b * RB_STEP, e = min(n - 1, s + RB_STEP);
    int32_t x0 = INT32_MAX, y0 = INT32_MAX, x1 = INT32_MIN, y1 = INT32_MIN;
    for (int i = s; i <= e; i++) {
      x0 = min(x0, routeX[i]); x1 = max(x1, routeX[i]);
      y0 = min(y0, routeY[i]); y1 = max(y1, routeY[i]);
    }
    rbBox[4 * b] = x0; rbBox[4 * b + 1] = y0; rbBox[4 * b + 2] = x1; rbBox[4 * b + 3] = y1;
  }
  routeN = n; routeFrame = frame;
  routeProg = -1; routeSearchAt = 0;                  // v3.0 新路线：重新找车的位置
  if (routeCum) {                                     // v5.0 路程表（路况颜色用）
    routeCum[0] = 0;
    for (int i = 1; i < n; i++) {
      float dx = routeX[i] - routeX[i - 1], dy = routeY[i] - routeY[i - 1];
      routeCum[i] = routeCum[i - 1] + sqrtf(dx * dx + dy * dy);
    }
  }
  if (routeCum && routeCurv) {                        // v6.1 弯急程度：前后各 20 米之间转了多少度
    const float WIN = 20;
    int j0 = 0, j1 = 0;
    for (int i = 0; i < n; i++) {
      while (j0 < i && routeCum[i] - routeCum[j0 + 1] >= WIN) j0++;
      if (j1 < i) j1 = i;
      while (j1 < n - 1 && routeCum[j1] - routeCum[i] < WIN) j1++;
      uint8_t cls = 0;
      if (j0 < i && j1 > i) {
        float h0 = atan2f(routeY[i] - routeY[j0], routeX[i] - routeX[j0]);
        float h1 = atan2f(routeY[j1] - routeY[i], routeX[j1] - routeX[i]);
        float d = fabsf(h1 - h0);
        if (d > PI) d = 2 * PI - d;
        float deg = d * 180 / PI;
        cls = deg < 20 ? 0 : (deg < 50 ? 1 : 2);
      }
      routeCurv[i] = cls;
    }
  }
  // v3.2 路线点后面可选：红绿灯个数(2) + 每个灯 x(4) y(4) 所在段号(2) 段内比例(1，0~255)
  lightN = 0;
  uint32_t pos = 12 + (uint32_t)(n - 1) * 4;
  if (pos + 2 <= len) {
    int ln = rdu16(p + pos); pos += 2;
    for (int i = 0; i < ln && lightN < LIGHT_MAX && pos + 11 <= len; i++, pos += 11) {
      lightX[lightN] = rd32(p + pos); lightY[lightN] = rd32(p + pos + 4);
      lightU[lightN] = rdu16(p + pos + 8) + p[pos + 10] / 255.0f;
      lightN++;
    }
  }
}

// ---- v3.0 走过的路线 ----
static inline float segLen(int k) {
  float dx = routeX[k + 1] - routeX[k], dy = routeY[k + 1] - routeY[k];
  return sqrtf(dx * dx + dy * dy);
}

// 路线上的点：u = 段号 + 段内比例
void routePt(float u, float &x, float &y) {
  int k = (int)u;
  if (k >= routeN - 1) { x = routeX[routeN - 1]; y = routeY[routeN - 1]; return; }
  float t = u - k;
  x = routeX[k] + (routeX[k + 1] - routeX[k]) * t;
  y = routeY[k] + (routeY[k + 1] - routeY[k]) * t;
}

// 找车在路线上的位置：平时只在当前位置附近（往回 30 米、往前 300 米）找，
// 这样掉头、绕回同一条路时不会跳到路线后面去；还没对上时才搜整条路线（最多每秒一次）
void updateRouteProgress(float cx, float cy) {
  if (routeN < 2) return;
  int k0, k1;
  if (routeProg < 0) {
    if (millis() - routeSearchAt < 1000 && routeSearchAt) return;
    routeSearchAt = millis();
    k0 = 0; k1 = routeN - 2;
  } else {
    int base = min((int)routeProg, routeN - 2);
    float acc = 0;
    k0 = base; while (k0 > 0 && acc < 30) { acc += segLen(k0 - 1); k0--; }
    acc = 0;
    k1 = base; while (k1 < routeN - 2 && acc < 300) { acc += segLen(k1); k1++; }
  }
  float best = 1e18f, bestU = -1;
  for (int k = k0; k <= k1; k++) {
    float ax = routeX[k], ay = routeY[k];
    float vx = routeX[k + 1] - ax, vy = routeY[k + 1] - ay;
    float L2 = vx * vx + vy * vy;
    float t = L2 > 0 ? ((cx - ax) * vx + (cy - ay) * vy) / L2 : 0;
    t = constrain(t, 0.0f, 1.0f);
    float ex = ax + vx * t - cx, ey = ay + vy * t - cy;
    float d2 = ex * ex + ey * ey;
    if (d2 < best) { best = d2; bestU = k + t; }
  }
  if (best > 50.0f * 50.0f) return;                   // 离路线超过 50 米（偏航中）：先不动
  routeProg = bestU;
}

void mapPolyline(const float *xs, const float *ys, int n, float w, uint16_t c, bool caps);

// 画路线的一段（u 从 a 到 b），先画整段描边再画内芯
// v6.1 弯道上色：内芯按每段的弯急程度分成几截，各用绿 / 黄 / 红画
const uint16_t CURVE_COL[3] = {0x3EEB, 0xFE45, 0xF9E6};
void drawRouteCurved(float a, float b, float cx, float cy, float sc, float cs, float sn, float viewR, float w1) {
  for (int bi = 0; bi * RB_STEP < routeN - 1; bi++) {
    int s = bi * RB_STEP, e = min(routeN - 1, s + RB_STEP);
    if (e <= a || s >= b) continue;
    const int32_t *bb = rbBox + 4 * bi;
    if (bb[2] < cx - viewR || bb[0] > cx + viewR || bb[3] < cy - viewR || bb[1] > cy + viewR) continue;
    float ua = fmaxf(a, (float)s), ub = fminf(b, (float)e);
    int m = 0, cls = -1;
    auto proj = [&](float x, float y) {
      tmpX[m] = CX + sc * ((x - cx) * cs - (y - cy) * sn); tmpY[m] = effectiveMapPy - sc * ((x - cx) * sn + (y - cy) * cs); m++;
    };
    float x, y;
    routePt(ua, x, y); proj(x, y);
    cls = routeCurv[(int)floorf(ua)];
    for (int k = (int)floorf(ua) + 1; k < ub; k++) {
      proj((float)routeX[k], (float)routeY[k]);
      int c2 = routeCurv[k];
      if (c2 != cls) {                                   // 换颜色：先把这一截画掉，从这个点接着画
        if (m >= 2) mapPolyline(tmpX, tmpY, m, w1, CURVE_COL[cls], true);
        tmpX[0] = tmpX[m - 1]; tmpY[0] = tmpY[m - 1]; m = 1;
        cls = c2;
      }
    }
    routePt(ub, x, y); proj(x, y);
    if (m >= 2) mapPolyline(tmpX, tmpY, m, w1, CURVE_COL[cls], true);
  }
}

void drawRouteRange(float a, float b, float cx, float cy, float sc, float cs, float sn, float viewR,
                    float w0, float w1, uint16_t c0, uint16_t c1) {
  if (b - a < 0.001f) return;
  for (int pass = 0; pass < 2; pass++) {
    for (int bi = 0; bi * RB_STEP < routeN - 1; bi++) {
      int s = bi * RB_STEP, e = min(routeN - 1, s + RB_STEP);
      if (e <= a || s >= b) continue;
      const int32_t *bb = rbBox + 4 * bi;
      if (bb[2] < cx - viewR || bb[0] > cx + viewR || bb[3] < cy - viewR || bb[1] > cy + viewR) continue;
      float ua = fmaxf(a, (float)s), ub = fminf(b, (float)e);
      int m = 0;
      float x, y;
      routePt(ua, x, y);
      tmpX[m] = CX + sc * ((x - cx) * cs - (y - cy) * sn); tmpY[m] = effectiveMapPy - sc * ((x - cx) * sn + (y - cy) * cs); m++;
      for (int k = (int)floorf(ua) + 1; k < ub; k++) {
        float dx = routeX[k] - cx, dy = routeY[k] - cy;
        tmpX[m] = CX + sc * (dx * cs - dy * sn); tmpY[m] = effectiveMapPy - sc * (dx * sn + dy * cs); m++;
      }
      routePt(ub, x, y);
      tmpX[m] = CX + sc * ((x - cx) * cs - (y - cy) * sn); tmpY[m] = effectiveMapPy - sc * ((x - cx) * sn + (y - cy) * cs); m++;
      if (m >= 2) mapPolyline(tmpX, tmpY, m, pass == 0 ? w0 : w1, pass == 0 ? c0 : c1, true);
    }
  }
}

// v4.4 原路线：格式和导航路线一样（坐标系、起点、点数、差值），后面的红绿灯不管
void parseGhost(const uint8_t *p, uint32_t len) {
  ghostN = 0;
  if (!ghostX || len < 12) return;
  int frame = rdu16(p);
  int32_t x = rd32(p + 2), y = rd32(p + 6);
  int n = rdu16(p + 10);
  if (n > GHOST_PTS) n = GHOST_PTS;
  if (n < 2 || 12 + (uint32_t)(n - 1) * 4 > len) return;
  ghostX[0] = x; ghostY[0] = y;
  for (int i = 1; i < n; i++) {
    x += rd16(p + 12 + 4 * (i - 1)); y += rd16(p + 14 + 4 * (i - 1));
    ghostX[i] = x; ghostY[i] = y;
  }
  for (int b = 0; b * RB_STEP < n - 1; b++) {
    int s = b * RB_STEP, e = min(n - 1, s + RB_STEP);
    int32_t x0 = INT32_MAX, y0 = INT32_MAX, x1 = INT32_MIN, y1 = INT32_MIN;
    for (int i = s; i <= e; i++) {
      x0 = min(x0, ghostX[i]); x1 = max(x1, ghostX[i]);
      y0 = min(y0, ghostY[i]); y1 = max(y1, ghostY[i]);
    }
    gbBox[4 * b] = x0; gbBox[4 * b + 1] = y0; gbBox[4 * b + 2] = x1; gbBox[4 * b + 3] = y1;
  }
  ghostN = n; ghostFrame = frame;
}

// v4.4 画原路线：橙色细线，画在导航路线下面（两条重合的地方只看到导航路线，分开的地方才露出来）
void drawGhost(float cx, float cy, float sc, float cs, float sn, float viewR, float w) {
  if (ghostN < 2 || ghostFrame != posFrame) return;
  for (int bi = 0; bi * RB_STEP < ghostN - 1; bi++) {
    const int32_t *bb = gbBox + 4 * bi;
    if (bb[2] < cx - viewR || bb[0] > cx + viewR || bb[3] < cy - viewR || bb[1] > cy + viewR) continue;
    int s = bi * RB_STEP, e = min(ghostN - 1, s + RB_STEP), m = 0;
    for (int k = s; k <= e; k++) {
      float dx = ghostX[k] - cx, dy = ghostY[k] - cy;
      tmpX[m] = CX + sc * (dx * cs - dy * sn); tmpY[m] = effectiveMapPy - sc * (dx * sn + dy * cs); m++;
    }
    if (m >= 2) mapPolyline(tmpX, tmpY, m, w, styleIdx()==ST_MINIMAL ? 0x8410 : 0xFD20, true);
  }
}

// 'M' 类型 编号 总片数(2) 片序号(2) 总字节(4) 偏移(4) + 数据

bool parseCross(const uint8_t *d, uint32_t total) {
  if (!d || total < 42) return false;
  int w = d[0] | (d[1] << 8);
  int h = d[2] | (d[3] << 8);
  if (w <= 0 || w > CROSS_MAX_W || h <= 0 || h > CROSS_MAX_H) return false;

  for (int i = 0; i < 16; i++) {
    crossPalette[i] = d[6 + i * 2] | (d[7 + i * 2] << 8);
  }

  size_t packed = total - 42;
  size_t rleMax = (size_t)w * h * 2;
  uint8_t *rleBuf = (uint8_t *)ps_malloc(rleMax);
  if (!rleBuf) return false;

  if (!viceInflater) viceInflater = (tinfl_decompressor *)malloc(sizeof(tinfl_decompressor));
  if (!viceInflater) { free(rleBuf); return false; }
  tinfl_init(viceInflater);
  size_t inBytes = packed, outBytes = rleMax;
  tinfl_status res = tinfl_decompress(viceInflater, d + 42, &inBytes,
      rleBuf, rleBuf, &outBytes,
      TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  if (res != TINFL_STATUS_DONE) {
    free(rleBuf);
    return false;
  }

  if (!crossBuf) crossBuf = (uint16_t *)ps_malloc(CROSS_BUF_BYTES);
  if (!crossBuf) { free(rleBuf); return false; }

  int totalPix = w * h;
  int outIdx = 0;
  size_t p = 0;
  while (p < outBytes && outIdx < totalPix) {
    uint8_t tag = rleBuf[p++];
    if (tag < 16) {
      crossBuf[outIdx++] = crossPalette[tag];
    } else {
      int cIdx = tag & 0x0F;
      int len = ((tag >> 4) & 0x0F) + 1;
      if (len == 16 && p < outBytes) {
        len += rleBuf[p++];
      }
      uint16_t c = crossPalette[cIdx];
      for (int k = 0; k < len && outIdx < totalPix; k++) {
        crossBuf[outIdx++] = c;
      }
    }
  }

  free(rleBuf);
  if (outIdx >= totalPix) {
    crossW = w;
    crossH = h;
    setCrossActive(true);
    return true;
  }
  return false;
}

void drawJunctionCrossCard(float prog) {
  if (prog <= 0.01f || !crossBuf || crossW <= 0 || crossH <= 0) return;
  int cardW = 380;
  int cardH = 210;
  int cardX = (LCD_WIDTH - cardW) / 2;
  int cardY = (int)(-cardH + prog * (cardH + 12));

  // 绘制圆角卡片底板与边框
  gfx->fillRoundRect(cardX, cardY, cardW, cardH, 16, 0x10A2);
  gfx->drawRoundRect(cardX, cardY, cardW, cardH, 16, 0x4A69);
  gfx->drawRoundRect(cardX - 1, cardY - 1, cardW + 2, cardH + 2, 17, 0x2124);

  // 居中绘制路口放大图
  int imgX = cardX + (cardW - crossW) / 2;
  int imgY = cardY + (cardH - crossH) / 2 + 10;
  for (int r = 0; r < crossH; r++) {
    int py = imgY + r;
    if (py < 0 || py >= LCD_HEIGHT) continue;
    gfx->draw16bitRGBBitmap(imgX, py, crossBuf + r * crossW, crossW, 1);
  }

  // 顶部实景标签
  int tagY = cardY + 8;
  if (tagY >= 0 && tagY + 18 < LCD_HEIGHT) {
    gfx->fillRoundRect(cardX + 14, tagY, 74, 18, 4, 0x2145);
    gfx->setTextColor(0xFFFF);
    gfx->setTextSize(1);
    gfx->setCursor(cardX + 18, tagY + 5);
    gfx->print("JUNCTION");
  }
}


static tinfl_decompressor *coverInflater = nullptr;

bool parseCover(const uint8_t *d, uint32_t total) {
  if (!d || total < 42) return false;
  int w = d[0] | (d[1] << 8);
  int h = d[2] | (d[3] << 8);
  if (w <= 0 || w > COVER_SIZE || h <= 0 || h > COVER_SIZE) return false;

  for (int i = 0; i < 16; i++) {
    coverPalette[i] = d[6 + i * 2] | (d[7 + i * 2] << 8);
  }

  uint32_t rawRleLen = (uint32_t)d[38] | ((uint32_t)d[39] << 8) | ((uint32_t)d[40] << 16) | ((uint32_t)d[41] << 24);
  if (rawRleLen == 0 || rawRleLen > (uint32_t)w * h * 2) return false;

  size_t packed = total - 42;
  uint8_t *rleBuf = (uint8_t *)ps_malloc(rawRleLen);
  if (!rleBuf) return false;

  if (!coverInflater) coverInflater = (tinfl_decompressor *)malloc(sizeof(tinfl_decompressor));
  if (!coverInflater) { free(rleBuf); return false; }
  tinfl_init(coverInflater);

  size_t inBytes = packed, outBytes = rawRleLen;
  tinfl_status res = tinfl_decompress(coverInflater, d + 42, &inBytes,
      rleBuf, rleBuf, &outBytes,
      TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  if (res != TINFL_STATUS_DONE && outBytes != rawRleLen) {
    tinfl_init(coverInflater);
    inBytes = packed; outBytes = rawRleLen;
    res = tinfl_decompress(coverInflater, d + 42, &inBytes,
        rleBuf, rleBuf, &outBytes,
        TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
  }

  if (outBytes != rawRleLen) {
    free(rleBuf);
    return false;
  }

  if (!coverBuf) coverBuf = (uint16_t *)ps_calloc(1, COVER_BUF_BYTES);
  if (!coverBuf) { free(rleBuf); return false; }

  int totalPix = w * h;
  int outIdx = 0;
  size_t p = 0;
  while (p < outBytes && outIdx < totalPix) {
    uint8_t tag = rleBuf[p++];
    if (tag < 16) {
      coverBuf[outIdx++] = coverPalette[tag];
    } else {
      int cIdx = tag & 0x0F;
      int len = ((tag >> 4) & 0x0F) + 1;
      if (len == 16 && p < outBytes) {
        len += rleBuf[p++];
      }
      uint16_t c = coverPalette[cIdx];
      for (int k = 0; k < len && outIdx < totalPix; k++) {
        coverBuf[outIdx++] = c;
      }
    }
  }

  free(rleBuf);
  if (outIdx >= totalPix) {
    coverValid = true;
    musicDirty = true;
    return true;
  }
  return false;
}

void handleBulk(const uint8_t *d, int n) {
  if (n < 15) return;
  int type = d[1], id = d[2];
  int parts = d[3] | (d[4] << 8), seq = d[5] | (d[6] << 8);
  uint32_t total = (uint32_t)rd32(d + 7), off = (uint32_t)rd32(d + 11);
  if (type < 1 || type > 6) return;
  Bulk &b = bulks[type];
  if (!b.buf || total == 0 || total > b.cap || parts == 0 || parts > 4096 || seq >= parts) return;
  if (id != b.id || total != b.total || parts != b.parts) {     // 新的一份：从头开始收
    b.id = id; b.total = total; b.parts = parts; b.got = 0; b.done = false;
    memset(b.mask, 0, sizeof(b.mask));
  }
  if (b.done) { pendingAck = "K," + String(type) + "," + String(id); return; }   // 重复的：再确认一次
  uint32_t len = n - 15;
  if (off >= total) return;
  if (off + len > total) len = total - off;
  if (!(b.mask[seq >> 3] & (1 << (seq & 7)))) {
    memcpy(b.buf + off, d + 15, len);
    b.mask[seq >> 3] |= (1 << (seq & 7));
    b.got++;
  }
  if (b.got >= b.parts) {
    b.done = true;
    if (type == 1) {
      uint8_t *t = mapShow; mapShow = b.buf; b.buf = t;   // 交换：新图显示，旧缓冲留着收下一块
      parseMap(mapShow, total);
    } else if (type == 2) {
      parseRoute(b.buf, total);
    } else if (type == 4) {
      parseGhost(b.buf, total);                        // v4.4 原路线
    } else if (type == 5) {
      parseCross(b.buf, total);
    } else if (type == 6) {
      if (parseCover(b.buf, total)) {
        musicDirty = true;
      }
    } else {
      if (!parseRaster(b.buf,total)) {
        // 没有成功解码就不回“已确认”，通知手机回退全量重发。
        b.done=false; b.got=0; memset(b.mask,0,sizeof(b.mask));
        pendingAck = "N," + String(type) + "," + String(id);
        return;
      }
    }
    pendingAck = "K," + String(type) + "," + String(id);
    navDirty = true;
  }
}

String pendingStat2 = "";
// v6.8 每 5 秒读一次；有变化就重画、通知手机
void batPoll() {
  if (!pmuOK) return;
  unsigned long now = millis();
  if (batAt && now - batAt < 5000) return;
  batAt = now;
  // StopWatch：M5PM1 读电池电压/输入电压/充电脚；电量按官方 UserDemo 的
  // 3300-4200mV 线性映射 + 7:1 低通滤波（避免显示跳动）
  uint16_t mv = 0, vin = 0;
  uint8_t chgStat = 1;
  bool vOK = StopWatchBoard::pm1.readVbat(&mv) == M5PM1_OK;
  bool vinOK = StopWatchBoard::pm1.readVin(&vin) == M5PM1_OK;
  StopWatchBoard::pm1.gpioGetInput(M5PM1_GPIO_NUM_2, &chgStat);
  bool present = false;
  int pct = -1;
  if (vOK && mv >= 2800) {
    present = true;
    batMv = mv;
    static uint16_t filt = 0;
    filt = filt == 0 ? mv : (filt * 7 + mv + 4) / 8;
    // 锂电池截止电压与实际容量曲线校准：3300mV(0%) ~ 4150mV(100%)，跨度 850mV
    pct = (int)constrain((int32_t)(filt - 3300) * 100 / 850, 0, 100);
  }
  // v7.2 偶尔读不到电池：先沿用上次的电量，连续 3 次读不到才当成没接电池
  static int missCnt = 0;
  if (pct < 0) { if (batPct >= 0 && ++missCnt < 3) { pct = batPct; present = true; } }
  else missCnt = 0;
  // 充电中 = 有外部供电（Vin>4V）且 CHG_STAT 脚为低；插着 USB 只看 Vin
  bool chg = present && vinOK && vin > 4000 && chgStat == 0;
  bool usb = vinOK && vin > 4000;
  // 充满状态处理：插着 USB、电压已达高位（>=4080mV）且硬件充电指示截止（chgStat != 0），直接置为 100%
  if (usb && chgStat != 0 && pct >= 88 && batMv >= 4080) {
    pct = 100;
  }
  bool changed = pct != batPct || chg != batChg || usb != usbIn;
  batPct = pct; batChg = chg; usbIn = usb;
  if (mediaHid && changed && pct >= 0 && bleConnected) mediaHid->setBatteryLevel((uint8_t)constrain(pct, 0, 100));
  if (changed) {
    if (onWaitScreen && page == 0 && !sliding) drawStatus();        // v7.2 只重画状态栏
    else if (inNav) navDirty = true;
  }
  if (changed || now - batSentAt > 60000) {
    batSentAt = now;
    pendingBat = "V," + String(batPct) + "," + String(batChg ? 1 : 0) + "," + String(usbIn ? 1 : 0) + "," + String(batMv);
  }
}

void sendPendingAck() {
  if (!txChar || !bleConnected || !bleAuthed) return;    // v6.5 没通过验证的手机什么都不发
  // v6.5 告诉手机“验证通过了”：连上后多次发送，覆盖手机端协商MTU、PHY与订阅CCCD的时间窗口
  static const uint16_t uAt[5] = {300, 1500, 3500, 6500, 10000};
  if (authNotifies < 5 && millis() - bleAuthAt > uAt[authNotifies]) {
    authNotifies++;
    lastAuthNotifyAt = millis();
    txChar->setValue((uint8_t *)"U,1", 3);
    txChar->notify();
    txChar->setValue((uint8_t *)"CAP,HID,1", 9); txChar->notify();
    txChar->setValue((uint8_t *)"CAP,LYR,1", 9); txChar->notify();
    txChar->setValue((uint8_t *)"CAP,ZMAP,1", 10); txChar->notify();
    pendingHidStatus = String("HID,") + (hidMusicMode ? 1 : 0) + "," + (hidMediaReady() ? 1 : 0);
    appleMedia.resend();
    pendingLyricsStatus = String("ML,") + (musicLyrics ? 1 : 0);
    pendingMediaStatus = String("AMC,") + appleMedia.controlMask();
  }
  if (pendingLyricsStatus.length()) { txChar->setValue((uint8_t *)pendingLyricsStatus.c_str(), pendingLyricsStatus.length()); txChar->notify(); pendingLyricsStatus = ""; }
  if (pendingMediaStatus.length()) { txChar->setValue((uint8_t *)pendingMediaStatus.c_str(), pendingMediaStatus.length()); txChar->notify(); pendingMediaStatus = ""; }
  uint8_t amsFrame[139]; size_t amsSize = 0;
  if (appleMedia.nextFrame(amsFrame, amsSize, millis())) { txChar->setValue(amsFrame, amsSize); txChar->notify(); }
  if (pendingHidStatus.length()) {
    txChar->setValue((uint8_t *)pendingHidStatus.c_str(), pendingHidStatus.length());
    txChar->notify(); pendingHidStatus = "";
  }
  if (pendingBootAck.length()) {
    txChar->setValue((uint8_t *)pendingBootAck.c_str(), pendingBootAck.length());
    txChar->notify(); pendingBootAck = "";
  }
  if (pendingBuildingCapability) {
    txChar->setValue((uint8_t *)"CAP,BLD,1", 9); txChar->notify();
    txChar->setValue((uint8_t *)"CAP,ZMAP,1", 10); txChar->notify();
    pendingBuildingCapability=false;
  }
  if (pendingAck.length()) {
    txChar->setValue((uint8_t *)pendingAck.c_str(), pendingAck.length());
    txChar->notify();
    pendingAck = "";
  }
  if (pendingStat2.length()) {
    txChar->setValue((uint8_t *)pendingStat2.c_str(), pendingStat2.length());
    txChar->notify();
    pendingStat2 = "";
  }
  if (pendingProf.length()) {                         // v5.8 分步用时
    txChar->setValue((uint8_t *)pendingProf.c_str(), pendingProf.length());
    txChar->notify();
    pendingProf = "";
  }
  if (pendingBat.length()) {                          // v6.8 电量
    txChar->setValue((uint8_t *)pendingBat.c_str(), pendingBat.length());
    txChar->notify();
    pendingBat = "";
  }
  if (pendingEvt.length()) {                          // v5.2 触摸事件
    txChar->setValue((uint8_t *)pendingEvt.c_str(), pendingEvt.length());
    txChar->notify();
    pendingEvt = "";
  }
}

// 现在是否显示小地图
bool mapModeActive() {
  if (!mapOn || !inNav) return false;
  bool vecOK = mapValid && mapFrame == posFrame;
  bool rasOK = rasterValid && rasterFrame == posFrame;      // v2.5
  // v4.0 停车或暂停时手机可能不再发位置：不再 5 秒后切回箭头画面，保持地图（位置停在最后一次）
  if (!(styleIdx() == ST_DRAGON || vecOK || rasOK) || posAt == 0) return false;
  return true;                                      // v2.1 一直显示地图，路口信息放大显示在圆牌里
}

// ---- 画图工具 ----

// 粗折线（圆角连接）；屏幕外的线段直接跳过
// v2.2：只在方向变化明显的拐点补圆（直线上的点不画圆，省很多时间）；caps = 两端也画圆头
struct VicePlane { float nx, ny, c, support; uint8_t angle; };

static inline void viceBlendPixel(uint16_t *row, int x, uint16_t color, float coverage) {
  int alpha = (int)(coverage * 256.0f + 0.5f);
  if (alpha <= 0) return;
  row[x] = alpha >= 256 ? color : blendF(row[x], color, alpha);
}

void viceAAQuad(const float *px, const float *py, uint16_t color) {
  VicePlane plane[4];
  float cx = 0, cy = 0, ymin = py[0], ymax = py[0];
  for (int i = 0; i < 4; i++) { cx += px[i]; cy += py[i]; ymin = fminf(ymin, py[i]); ymax = fmaxf(ymax, py[i]); }
  cx *= 0.25f; cy *= 0.25f;
  for (int i = 0; i < 4; i++) {
    int j = (i + 1) & 3;
    float dx = px[j] - px[i], dy = py[j] - py[i], length = sqrtf(dx * dx + dy * dy);
    if (length < 0.001f) return;
    float nx = -dy / length, ny = dx / length;
    if ((cx - px[i]) * nx + (cy - py[i]) * ny < 0) { nx = -nx; ny = -ny; }
    plane[i] = {nx, ny, -nx * px[i] - ny * py[i], (fabsf(nx) + fabsf(ny)) * 0.5f, viceCoverageAngle(nx, ny)};
  }
  uint16_t *fb = gfx->getFramebuffer();
  int ya = max(0, (int)floorf(ymin - 1)), yb = min(LCD_HEIGHT - 1, (int)ceilf(ymax + 1));
  for (int y = ya; y <= yb; y++) {
    float sy = y + 0.5f, left = 0.5f, right = LCD_WIDTH - 0.5f;
    float solidLeft = left, solidRight = right;
    bool outside = false, noSolid = false;
    float q[4];
    for (int i = 0; i < 4; i++) {
      q[i] = plane[i].ny * sy + plane[i].c;
      float nx = plane[i].nx, support = plane[i].support;
      if (fabsf(nx) < 0.00001f) {
        if (q[i] <= -support) outside = true;
        if (q[i] < support) noSolid = true;
      } else if (nx > 0) {
        left = fmaxf(left, (-support - q[i]) / nx);
        solidLeft = fmaxf(solidLeft, (support - q[i]) / nx);
      } else {
        right = fminf(right, (-support - q[i]) / nx);
        solidRight = fminf(solidRight, (support - q[i]) / nx);
      }
    }
    if (outside || left > right) continue;
    int xa = max(0, (int)ceilf(left - 0.5f)), xb = min(LCD_WIDTH - 1, (int)floorf(right - 0.5f));
    int ia = max(xa, (int)ceilf(solidLeft - 0.5f)), ib = min(xb, (int)floorf(solidRight - 0.5f));
    if (noSolid || ia > ib) { ia = xb + 1; ib = xb; }
    uint16_t *row = fb + y * LCD_WIDTH;
    for (int x = xa; x <= xb;) {
      if (x == ia && ia <= ib) {
        gfx->drawFastHLine(x, y, ib - ia + 1, color);
        x = ib + 1; continue;
      }
      float sx = x + 0.5f;
      int alpha = 256;
      for (int i = 0; i < 4; i++) {
        float distance = plane[i].nx * sx + q[i];
        if (distance <= -plane[i].support) { alpha = 0; break; }
        if (distance < plane[i].support) {
          int index = max(0, min(256, (int)floorf(128.5f + distance * (256.0f / 1.5f))));
          alpha = min(alpha, (int)viceCoverage[plane[i].angle][index]);
        }
      }
      if (alpha) row[x] = alpha >= 256 ? color : blendF(row[x], color, alpha);
      x++;
    }
  }
}

void viceAACircle(float cx, float cy, float radius, uint16_t color) {
  uint16_t *fb = gfx->getFramebuffer();
  int ya = max(0, (int)floorf(cy - radius - 1)), yb = min(LCD_HEIGHT - 1, (int)ceilf(cy + radius + 1));
  float outer = radius + 0.5f, inner = fmaxf(0, radius - 0.5f);
  for (int y = ya; y <= yb; y++) {
    float dy = y + 0.5f - cy;
    if (fabsf(dy) >= outer) continue;
    float extent = sqrtf(outer * outer - dy * dy);
    int xa = max(0, (int)ceilf(cx - extent - 0.5f)), xb = min(LCD_WIDTH - 1, (int)floorf(cx + extent - 0.5f));
    int ia = xb + 1, ib = xb;
    if (fabsf(dy) < inner) {
      float core = sqrtf(inner * inner - dy * dy);
      ia = max(xa, (int)ceilf(cx - core - 0.5f)); ib = min(xb, (int)floorf(cx + core - 0.5f));
    }
    uint16_t *row = fb + y * LCD_WIDTH;
    for (int x = xa; x <= xb;) {
      if (x == ia && ia <= ib) {
        gfx->drawFastHLine(x, y, ib - ia + 1, color);
        x = ib + 1; continue;
      }
      float dx = x + 0.5f - cx;
      viceBlendPixel(row, x, color, outer - sqrtf(dx * dx + dy * dy));
      x++;
    }
  }
}

void mapPolyline(const float *xs, const float *ys, int n, float w, uint16_t c, bool caps) {
  bool vice = styleIdx() == 1;
  if (vice) initViceSampling();
  float r = w / 2;
  float pdx = 0, pdy = 0;
  bool havePrev = false;
  for (int i = 0; i + 1 < n; i++) {
    float x0 = xs[i], y0 = ys[i], x1 = xs[i + 1], y1 = ys[i + 1];
    if ((x0 < -w && x1 < -w) || (x0 > LCD_WIDTH + w && x1 > LCD_WIDTH + w) ||
        (y0 < -w && y1 < -w) || (y0 > LCD_HEIGHT + w && y1 > LCD_HEIGHT + w)) continue;
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.5f) continue;
    float ux = dx / len, uy = dy / len;
    float nx = -uy * r, ny = ux * r;
    if (vice) {
      float px[4] = {x0 + nx, x0 - nx, x1 - nx, x1 + nx};
      float py[4] = {y0 + ny, y0 - ny, y1 - ny, y1 + ny};
      viceAAQuad(px, py, c);
    } else {
      gfx->fillTriangle(x0 + nx, y0 + ny, x0 - nx, y0 - ny, x1 + nx, y1 + ny, c);
      gfx->fillTriangle(x0 - nx, y0 - ny, x1 - nx, y1 - ny, x1 + nx, y1 + ny, c);
    }
    if (r >= 1.5f) {
      if (havePrev ? (ux * pdx + uy * pdy < 0.97f) : caps) {
        if (vice) viceAACircle(x0, y0, r, c); else gfx->fillCircle(x0, y0, r, c);
      }
      if (caps && i + 2 == n) {
        if (vice) viceAACircle(x1, y1, r, c); else gfx->fillCircle(x1, y1, r, c);
      }
    }
    pdx = ux; pdy = uy; havePrev = true;
  }
}

// 实心多边形（扫描线，奇偶规则）
void mapFillPoly(const float *xs, const float *ys, int n, uint16_t c) {
  if (n < 3) return;
  float ymin = 1e9f, ymax = -1e9f, xmin = 1e9f, xmax = -1e9f;
  for (int i = 0; i < n; i++) {
    ymin = fminf(ymin, ys[i]); ymax = fmaxf(ymax, ys[i]);
    xmin = fminf(xmin, xs[i]); xmax = fmaxf(xmax, xs[i]);
  }
  if (ymax < 0 || ymin >= LCD_HEIGHT || xmax < 0 || xmin >= LCD_WIDTH) return;
  int y0 = max(0, (int)ceilf(ymin - 0.5f)), y1 = min(LCD_HEIGHT - 1, (int)floorf(ymax - 0.5f));
  static float xs_hit[256];
  for (int y = y0; y <= y1; y++) {
    float sy = y + 0.5f;
    int k = 0;
    for (int i = 0, j = n - 1; i < n; j = i++) {
      float ya = ys[j], yb = ys[i];
      if ((ya <= sy && yb > sy) || (yb <= sy && ya > sy)) {
        if (k < 256) xs_hit[k++] = xs[j] + (sy - ya) * (xs[i] - xs[j]) / (yb - ya);
      }
    }
    for (int a = 1; a < k; a++) {               // 插入排序（交点通常很少）
      float v = xs_hit[a]; int b = a - 1;
      while (b >= 0 && xs_hit[b] > v) { xs_hit[b + 1] = xs_hit[b]; b--; }
      xs_hit[b + 1] = v;
    }
    for (int a = 0; a + 1 < k; a += 2) {
      int xa = max(0, (int)ceilf(xs_hit[a] - 0.5f)), xb = min(LCD_WIDTH - 1, (int)floorf(xs_hit[a + 1] - 0.5f));
      if (xb >= xa) gfx->drawFastHLine(xa, y, xb - xa + 1, c);
    }
  }
}

// 车头箭头（白色，深色描边）
void drawPlayer(float x, float y) {
  // v3.9 车头箭头缩小到原来的约 70%
  const float T[4][2] = {{0, -13}, {-10, 10.5f}, {0, 5}, {10, 10.5f}};
  for (int pass = 0; pass < 2; pass++) {
    float k = pass == 0 ? 1.35f : 1.0f;
    uint16_t c = pass == 0 ? C_BLACK : C_WHITE;
    float yo = pass == 0 ? 1.0f : 0;
    gfx->fillTriangle(x + T[0][0] * k, y + T[0][1] * k + yo, x + T[1][0] * k, y + T[1][1] * k + yo, x + T[2][0] * k, y + T[2][1] * k + yo, c);
    gfx->fillTriangle(x + T[0][0] * k, y + T[0][1] * k + yo, x + T[2][0] * k, y + T[2][1] * k + yo, x + T[3][0] * k, y + T[3][1] * k + yo, c);
  }
}

// ---- v2.7 上下弧形面板：和黑色外圈连成一体 ----
#define CAP_R      213      // 面板填到这个半径（和外圈黑边接上）
#define TOPCAP_Y   98       // 上面板下沿（有测速时 112）
#define BOTCAP_Y   368      // 下面板上沿（离路口 150 米内 334）
#define BOTCAP_Y2  334

// 填充圆内 y0~y1 行（圆的一个弓形）
void fillCapRows(int y0, int y1, uint16_t c) {
  for (int y = max(0, y0); y <= min(LCD_HEIGHT - 1, y1); y++) {
    float dy = y + 0.5f - CX;
    if (fabsf(dy) >= CAP_R) continue;
    float h = sqrtf((float)CAP_R * CAP_R - dy * dy);
    int xa = max(0, (int)(CX - h)), xb = min(LCD_WIDTH - 1, (int)(CX + h));
    gfx->drawFastHLine(xa, y, xb - xa + 1, c);
  }
}

// 面板和地图之间的分隔线（只画到外圈黑边以内）
void capLine(int y, int thick, uint16_t c) {
  for (int k = 0; k < thick; k++) {
    float dy = y + k + 0.5f - CX;
    if (fabsf(dy) >= 211) continue;
    float h = sqrtf(211.0f * 211.0f - dy * dy);
    gfx->drawFastHLine((int)(CX - h), y + k, (int)(2 * h), c);
  }
}

// v2.8 带黑色描边的文字（直接浮在地图上）
void printOutlined(const String &t, int x, int y, int size, uint16_t col) {
  int o = size >= 3 ? 2 : 1;
  // v5.8 只画最外一圈偏移（字的笔画至少 o 像素粗，里面那圈会被盖住，画了也白画）
  for (int dy = -o; dy <= o; dy++)
    for (int dx = -o; dx <= o; dx++)
      if ((dx || dy) && (o < 2 || abs(dx) == o || abs(dy) == o)) printCentered(t, x + dx, y + dy, size, C_BLACK);
  printCentered(t, x, y, size, col);
}

// 把一块区域调暗（半透明深色底），a = 保留多少原来的亮度（0~255）
void dimRect(int x, int y, int w, int h, int r, int keep) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  for (int j = max(0, y); j < min(LCD_HEIGHT, y + h); j++) {
    for (int i = max(0, x); i < min(LCD_WIDTH, x + w); i++) {
      int dx = i < x + r ? x + r - i : (i >= x + w - r ? i - (x + w - r - 1) : 0);
      int dy = j < y + r ? y + r - j : (j >= y + h - r ? j - (y + h - r - 1) : 0);
      if (dx * dx + dy * dy > r * r) continue;
      uint16_t &p = fb[j * LCD_WIDTH + i];
      p = blend565(C_BLACK, p, keep);
    }
  }
}

// 车速（浮在顶部）+ 测速牌
void drawMapSpeed(bool over) {
  if (speedKmh < 0) return;
  int sx = hasCam ? CX - 40 : CX;
  printOutlined(String(speedKmh), sx, 44, 4, over ? C_RED : TH.hud);
  printOutlined("km/h", sx, 80, 1, over ? 0xFC10 : TH.hud);
  if (hasCam) {
    const int bx = CX + 52, by = 60;
    aaDisc(bx, by, 27, C_BLACK);
    aaDisc(bx, by, 25, C_SIGNRED);
    aaDisc(bx, by, 19, C_WHITE);
    String lim = String(camLimit);
    int sz = lim.length() >= 3 ? 2 : 3;
    printCentered(lim, bx, by - 4 * sz, sz, C_BLACK);
    printOutlined(distText(camDist), bx, by + 31, 2, over ? 0xFC10 : C_WHITE);
  }
}

// ---- v3.1 红绿灯小图标：竖着的黑色灯箱 + 红黄绿三个灯 ----
void drawTrafficLight(float cx, float cy, float h) {
  float w = h * 0.46f, r = h * 0.13f;
  gfx->fillRoundRect((int)(cx - w / 2 - 1), (int)(cy - h / 2 - 1), (int)w + 2, (int)h + 2, (int)(w / 2.6f), C_BLACK);
  gfx->fillRoundRect((int)(cx - w / 2), (int)(cy - h / 2), (int)w, (int)h, (int)(w / 3), 0x2104);
  aaDisc(cx, cy - h * 0.29f, r, 0xF8A2);     // 红
  aaDisc(cx, cy, r, 0xFDA0);                // 黄
  aaDisc(cx, cy + h * 0.29f, r, 0x3F09);    // 绿
}

// v3.1 底部一行行程信息：到达时间 · 剩余里程 · 红绿灯数（半透明深色底）
void drawTripLine(int y) {
  char eta[8];
  snprintf(eta, sizeof(eta), "%02d:%02d", (tripEta / 100) % 24, tripEta % 100);
  String a = String(eta);
  String b = tripRemainM >= 100000 ? String(tripRemainM / 1000) + "km" : distText(tripRemainM);
  String c = String(tripLights);
  const int CW = 12, GAP = 14, ICON = 10;
  bool lights = tripLights > 0;
  int w = (a.length() + b.length()) * CW + GAP + (lights ? GAP + ICON + 3 + c.length() * CW : 0);
  int x = CX - w / 2;
  dimRect(x - 10, y - 5, w + 20, 26, 12, 75);
  printCentered(a, x + a.length() * CW / 2, y, 2, TH.hud);
  x += a.length() * CW + GAP;
  printCentered(b, x + b.length() * CW / 2, y, 2, C_WHITE);
  x += b.length() * CW + GAP;
  if (lights) {
    drawTrafficLight(x + ICON / 2, y + 7, 18);
    x += ICON + 3;
    printCentered(c, x + c.length() * CW / 2, y, 2, C_WHITE);
  }
}

// v3.3 车道：放在车速下面（离路口 150 米内才显示），比原来大一点；有测速牌时再往下让一点
void drawMapLanes() {
  if (laneN <= 0 || navDir == "S" || navDist > MAP_NEAR_M) return;
  const int H = 30, MAXW = 210;
  int lw = (int)(fminf(36.0f * H / 32, (float)MAXW / laneN) * laneN);
  int y = hasCam ? 114 : 100;
  dimRect(CX - lw / 2 - 9, y - 5, lw + 18, H + 10, 11, 70);
  drawLanesAt(CX, y, MAXW, H);
}

// ---- v3.8 小地图上的“占位框”：先登记已经占用的地方，电子眼图标只放在空位上 ----
#define OBS_MAX 40
int16_t obsX0[OBS_MAX], obsY0[OBS_MAX], obsX1[OBS_MAX], obsY1[OBS_MAX];
int obsN = 0;
void obsAdd(float x0, float y0, float x1, float y1) {
  if (obsN >= OBS_MAX) return;
  obsX0[obsN] = (int16_t)x0; obsY0[obsN] = (int16_t)y0; obsX1[obsN] = (int16_t)x1; obsY1[obsN] = (int16_t)y1;
  obsN++;
}
// 和已占用的地方重叠了多少面积（像素）；from = 从第几个占位框开始算（v4.2）
long obsOverlap(int x0, int y0, int x1, int y1, int from = 0) {
  long sum = 0;
  for (int i = from; i < obsN; i++) {
    int w = min(x1, (int)obsX1[i]) - max(x0, (int)obsX0[i]);
    int h = min(y1, (int)obsY1[i]) - max(y0, (int)obsY0[i]);
    if (w > 0 && h > 0) sum += (long)w * h;
  }
  return sum;
}
bool obsHit(int x0, int y0, int x1, int y1) {
  for (int i = 0; i < obsN; i++)
    if (x0 < obsX1[i] && x1 > obsX0[i] && y0 < obsY1[i] && y1 > obsY0[i]) return true;
  return false;
}

// 本帧在雷达里的电子眼，在屏幕上的实际位置
float camPx[MCAM_MAX], camPy[MCAM_MAX];
bool camShown[MCAM_MAX];

// v3.5 算出电子眼在小地图上的位置（已经开过的 = 在车后面的不画）；v3.8 真正的绘制放到 drawCamIcons()
void drawMapCams(float cx, float cy, float sc, float cs, float sn) {
  for (int i = 0; i < MCAM_MAX; i++) camShown[i] = false;
  if (mapCamN == 0 || mapCamFrame != posFrame) return;
  for (int i = 0; i < mapCamN; i++) {
    float dx = mapCamX[i] - cx, dy = mapCamY[i] - cy;
    float px = CX + sc * (dx * cs - dy * sn), py = effectiveMapPy - sc * (dx * sn + dy * cs);
    if (py > effectiveMapPy + 12) continue;                                     // 在车后面：已经过了
    float ex = px - CX, ex2 = py - CX;
    if (ex * ex + ex2 * ex2 > 194.0f * 194.0f) continue;
    camPx[i] = px; camPy[i] = py; camShown[i] = true;
  }
}

#include "CameraIcons.h"


// 把一张 n×n 的图标（每像素 RGB565 + 透明度）画在 (中心 x, 中心 y)，按透明度和地图混合
void blitRGBA(const uint8_t *src, int n, int cx, int cy) {
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  int x0 = cx - n / 2, y0 = cy - n / 2;
  for (int j = 0; j < n; j++) {
    int y = y0 + j;
    if (y < 0 || y >= LCD_HEIGHT) continue;
    uint16_t *row = fb + y * LCD_WIDTH;
    const uint8_t *p = src + j * n * 3;
    for (int i = 0; i < n; i++, p += 3) {
      uint8_t a = p[2];
      if (!a) continue;
      int x = x0 + i;
      if (x < 0 || x >= LCD_WIDTH) continue;
      uint16_t c = p[0] | (p[1] << 8);
      row[x] = a >= 250 ? c : blend565(row[x], c, a);
    }
  }
}
void blitCamIcon(int id, int cx, int cy) {
  if (id < 0 || id >= CAMI_N) id = 0;
  blitRGBA(CAM_ICONS[id], CAMI_S, cx, cy);
}

// 画一个电子眼：测速且有限速值 = 红圈限速牌（数字是实时的）；其他 = 对应类型的图标
void drawOneCam(int i, float x, float y) {
  if (mapCamTyp[i] == CAMI_SPEED && mapCamSpd[i] > 0) {
    aaDisc(x, y, 15, C_BLACK);
    aaDisc(x, y, 13.6f, C_SIGNRED);
    aaDisc(x, y, 10.2f, C_WHITE);
    String t = String(mapCamSpd[i]);
    printCentered(t, (int)x + (t.length() == 3 ? 0 : 1), (int)y - 3, 1, C_BLACK);
  } else {
    blitCamIcon(mapCamTyp[i], (int)lroundf(x), (int)lroundf(y));
  }
}

static float angDiff(float a, float b) {
  float d = a - b;
  while (d > 180) d -= 360;
  while (d < -180) d += 360;
  return fabsf(d);
}

// v4.1 世界坐标 (wx, wy) 离导航路线最近多远（米）；ux, uy 返回最近那一段路的方向（单位向量）。
//      只看外框在 maxD 以内的路段组，算得很快；没有路线返回很大的数
float routeDistW(float wx, float wy, float maxD, float &ux, float &uy) {
  float best = 1e9f;
  ux = 0; uy = 1;
  if (routeN < 2 || routeFrame != posFrame) return best;
  for (int bi = 0; bi * RB_STEP < routeN - 1; bi++) {
    const int32_t *bb = rbBox + 4 * bi;
    if (wx < bb[0] - maxD || wx > bb[2] + maxD || wy < bb[1] - maxD || wy > bb[3] + maxD) continue;
    int s0 = bi * RB_STEP, e0 = min(routeN - 1, s0 + RB_STEP);
    for (int k = s0; k < e0; k++) {
      float ax = routeX[k], ay = routeY[k], dx = routeX[k + 1] - ax, dy = routeY[k + 1] - ay;
      float l2 = dx * dx + dy * dy;
      float t = l2 > 0 ? ((wx - ax) * dx + (wy - ay) * dy) / l2 : 0;
      t = constrain(t, 0.0f, 1.0f);
      float ex = ax + t * dx - wx, ey = ay + t * dy - wy;
      float d = sqrtf(ex * ex + ey * ey);
      if (d < best && l2 > 0) { best = d; float l = sqrtf(l2); ux = dx / l; uy = dy / l; }
    }
  }
  return best;
}

// v3.8 → v4.1 画所有电子眼：
//   1. 路上（电子眼的实际位置）画一个蓝色小摄像头；挨得很近的几个只画一个
//   2. 类型图标（测速牌、闯红灯、违章拍照……）不放在路上，而是摆在小摄像头的旁边：
//      优先垂直于道路的两侧，图标之间、图标和路线/红绿灯/车速等都不重叠；普通监控只有路上的小摄像头
//   3. 位置选好就锁住不动；转了弯（地图朝向和锁定时差 15 度以上）并且已经转稳了，才重新选一次
void drawCamIcons(float cx, float cy, float sc, float cs, float sn) {
  bool turning = angDiff(viewTargetHdg, viewHdg) > 4 || fabsf(hdgRate) > 8;   // 地图还在跟着转（v6.7 含正在转弯）
  const float MKR = 10, R = 15;                                           // 小摄像头半径、类型图标半径
  const float D = MKR + 3 + R;                                            // 类型图标离小摄像头中心的距离（边缘相隔 3 像素）
  // ---- 1. 路上的小摄像头 ----
  float mkX[MCAM_MAX], mkY[MCAM_MAX];
  int mkN = 0, camMk[MCAM_MAX];
  for (int i = 0; i < mapCamN && i < MCAM_MAX; i++) {
    camMk[i] = -1;
    if (!camShown[i]) continue;
    for (int q = 0; q < mkN; q++)
      if (fabsf(mkX[q] - camPx[i]) < 9 && fabsf(mkY[q] - camPy[i]) < 9) { camMk[i] = q; break; }
    if (camMk[i] < 0) { mkX[mkN] = camPx[i]; mkY[mkN] = camPy[i]; camMk[i] = mkN++; }
  }
  // v4.7 小摄像头和类型图标都是圆的：彼此之间按圆来判断重叠（以前按方框算，斜着放时会误判成“挡住了”，
  //      结果图标被推到远处、拉一根长线）
  float ciX[3 * MCAM_MAX], ciY[3 * MCAM_MAX], ciR[3 * MCAM_MAX];
  int ciN = 0;
  for (int q = 0; q < mkN; q++) {
    bool cam = false;                                                    // v4.8 这一处有没有电子眼（道路安全提示不算）
    for (int i = 0; i < mapCamN && i < MCAM_MAX; i++) if (camMk[i] == q && mapCamTyp[i] < 16) cam = true;
    if (cam) {
      blitRGBA(CAM_MARK, CAMM_S, (int)lroundf(mkX[q]), (int)lroundf(mkY[q]));
      ciX[ciN] = mkX[q]; ciY[ciN] = mkY[q]; ciR[ciN] = MKR; ciN++;
    } else {                                                             // 只有安全提示：路上画个小黄点
      aaDisc(mkX[q], mkY[q], 4.5f, C_BLACK);
      aaDisc(mkX[q], mkY[q], 3, 0xFE40);
      ciX[ciN] = mkX[q]; ciY[ciN] = mkY[q]; ciR[ciN] = 5; ciN++;
    }
  }
  // ---- 2. 旁边的类型图标：v4.3 同一处的几个排成一列（和道路平行），整列放在道路的一侧 ----
  int clN[MCAM_MAX], clCam[MCAM_MAX][MCAM_MAX];
  for (int q = 0; q < mkN; q++) clN[q] = 0;
  for (int i = 0; i < mapCamN && i < MCAM_MAX; i++) {
    if (camMk[i] < 0) continue;
    int t = mapCamTyp[i];
    if (t == 0 || t == 1) continue;                                      // 普通监控：路上的小摄像头就够了
    int q = camMk[i];
    bool dup = false;                                                    // 同一处同一种只标一次
    for (int j = 0; j < clN[q]; j++) if (mapCamTyp[clCam[q][j]] == t) { dup = true; break; }
    if (!dup) clCam[q][clN[q]++] = i;
  }
  // 每一列里按类型排序，顺序固定
  for (int q = 0; q < mkN; q++)
    for (int a = 0; a < clN[q]; a++)
      for (int b = a + 1; b < clN[q]; b++)
        if (mapCamTyp[clCam[q][b]] < mapCamTyp[clCam[q][a]]) { int t = clCam[q][a]; clCam[q][a] = clCam[q][b]; clCam[q][b] = t; }
  // 锁住：这一处的每个电子眼都放过、个数没变、地图没转过弯
  bool keep[MCAM_MAX];
  int order[MCAM_MAX], n = 0;
  for (int q = 0; q < mkN; q++) {
    if (clN[q] == 0) continue;
    bool k = true;
    for (int j = 0; j < clN[q]; j++) {
      int i = clCam[q][j];
      if (mapCamPref[i] <= 0 || mapCamPrefK[i] != clN[q] || !(turning || angDiff(viewHdg, mapCamLockH[i]) < 15)) k = false;
    }
    keep[q] = k;
    order[n++] = q;
  }
  for (int a = 0; a < n; a++)                                            // 锁住的在前，再按离车头的距离
    for (int b = a + 1; b < n; b++) {
      int qa = order[a], qb = order[b];
      float da = fabsf(mkX[qa] - CX) + fabsf(mkY[qa] - effectiveMapPy) + (keep[qa] ? 0 : 10000);
      float db = fabsf(mkX[qb] - CX) + fabsf(mkY[qb] - effectiveMapPy) + (keep[qb] ? 0 : 10000);
      if (db < da) { order[a] = qb; order[b] = qa; }
    }
  const float GAP = 2 * R + 3;                                           // 一列里相邻两个图标的间距
  for (int o = 0; o < n; o++) {
    int q = order[o], cnt = clN[q], i0 = clCam[q][0];
    float ax = mkX[q], ay = mkY[q];
    float A = mapCamPrefA[i0];
    int lvl = mapCamPref[i0];
    if (!keep[q]) {
      // 道路在屏幕上的方向（没有路线就当作竖直向上）
      float sdx = 0, sdy = -1, ux, uy;
      if (routeDistW(mapCamX[i0], mapCamY[i0], 60, ux, uy) < 60) { sdx = ux * cs - uy * sn; sdy = -(ux * sn + uy * cs); }
      float perp = atan2f(sdy, sdx) + PI / 2;
      // 候选：道路右侧/左侧，先贴近，再远一列
      long bestScore = 0x7FFFFFFF; float bestA = perp; int bestL = 1;
      for (int c = 0; c < 4; c++) {
        float cA = perp + ((c & 1) ? PI : 0);
        int cL = c < 2 ? 1 : 2;
        float d = cL == 1 ? D : D + GAP;
        float px = cosf(cA), py = sinf(cA), tx = -py, ty = px;
        long score = 0;
        for (int j = 0; j < cnt; j++) {
          float off = (j - (cnt - 1) / 2.0f) * GAP;
          float x = ax + px * d + tx * off, y = ay + py * d + ty * off;
          float ex = x - CX, ey = y - CX;
          if (ex * ex + ey * ey > 186.0f * 186.0f) score += 100000;      // 要整个在雷达里
          score += obsOverlap((int)(x - R - 2), (int)(y - R - 2), (int)(x + R + 2), (int)(y + R + 2));
          for (int m = 0; m < ciN; m++) {                                // 和小摄像头、别的图标：按圆算
            float ddx = x - ciX[m], ddy = y - ciY[m], need = R + ciR[m] + 2;
            float dd2 = ddx * ddx + ddy * ddy;
            if (dd2 < need * need) score += (long)((need - sqrtf(dd2)) * 60) + 1;
          }
          float sx = x - CX, sy = effectiveMapPy - y, rux, ruy;
          float wx = cx + (sx * cs + sy * sn) / sc, wy = cy + (-sx * sn + sy * cs) / sc;
          if (routeDistW(wx, wy, (R + 4) / sc, rux, ruy) < (R + 4) / sc) score += 600;   // 别压在路线上
        }
        if (score < bestScore) { bestScore = score; bestA = cA; bestL = cL; }
        if (score == 0) break;
      }
      A = bestA; lvl = bestL;
      for (int j = 0; j < cnt; j++) {                                    // 整列一起锁住
        int i = clCam[q][j];
        mapCamPref[i] = lvl; mapCamPrefA[i] = A; mapCamPrefK[i] = cnt; mapCamLockH[i] = viewHdg;
      }
    }
    float d = lvl == 2 ? D + GAP : D;
    float px = cosf(A), py = sinf(A), tx = -py, ty = px;
    if (lvl == 2) {                                                      // 远一列时，细线连到小摄像头
      gfx->drawLine((int)(ax + px * (MKR + 1)), (int)(ay + py * (MKR + 1)), (int)(ax + px * (d - R)), (int)(ay + py * (d - R)), C_BLACK);
    }
    for (int j = 0; j < cnt; j++) {
      float off = (j - (cnt - 1) / 2.0f) * GAP;
      float x = ax + px * d + tx * off, y = ay + py * d + ty * off;
      drawOneCam(clCam[q][j], x, y);
      if (ciN < 3 * MCAM_MAX) { ciX[ciN] = x; ciY[ciN] = y; ciR[ciN] = R; ciN++; }
    }
  }
}

// v3.2 红绿灯直接画在地图上的实际位置（图标始终竖直）；已经开过的不画；
//      下一个红绿灯在 300 米内时，旁边标出距离
void drawMapLights(float cx, float cy, float sc, float cs, float sn) {
  if (lightN == 0 || routeFrame != posFrame) return;
  bool first = true;
  for (int i = 0; i < lightN; i++) {
    if (routeProg >= 0 && lightU[i] <= routeProg + 0.02f) continue;     // 已经过了
    float dx = lightX[i] - cx, dy = lightY[i] - cy;
    float px = CX + sc * (dx * cs - dy * sn), py = effectiveMapPy - sc * (dx * sn + dy * cs);
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
  float px = CX + sx, py = effectiveMapPy + sy;
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
  obsAdd(CX - 15, (int)(effectiveMapPy - 18), CX + 15, (int)(effectiveMapPy + 16));                 // 车头箭头
  if (speedKmh >= 0) {
    int sx = hasCam ? CX - 40 : CX;
    obsAdd(sx - 60, 36, sx + 60, 92);                                   // 车速数字 + km/h
    if (hasCam) obsAdd(CX + 52 - 34, 30, CX + 52 + 34, 98);             // 测速牌 + 距离
  }
  if (laneN > 0 && navDir != "S" && navDist <= MAP_NEAR_M) {           // 车道（和 drawMapLanes 一致）
    const int H = 30, MAXW = 210;
    int lw = (int)(fminf(36.0f * H / 32, (float)MAXW / laneN) * laneN);
    int y = hasCam ? 114 : 100;
    obsAdd(CX - lw / 2 - 9, y - 5, CX + lw / 2 + 9, y + H + 5);
  }
  obsAdd(0, 340, LCD_WIDTH, LCD_HEIGHT);                                // 底部箭头、距离、行程信息
}

// 画一整帧小地图
uint32_t mapFrames = 0, mapDrawMsAcc = 0, mapMsAcc = 0, ovlMsAcc = 0;
unsigned long statAt = 0;
String pendingStat = "";

// ---- v5.9 外圈和边缘渐暗：逐行、两个核心一起画 ----
static inline uint16_t blendF(uint16_t a, uint16_t b, uint32_t t) {   // t: 0~256，256 = 全是 b
  uint32_t ar = a >> 11, ag = (a >> 5) & 63, ab = a & 31;
  uint32_t br = b >> 11, bg = (b >> 5) & 63, bb = b & 31;
  uint32_t r = (ar * (256 - t) + br * t) >> 8, g = (ag * (256 - t) + bg * t) >> 8, bl = (ab * (256 - t) + bb * t) >> 8;
  return (uint16_t)((r << 11) | (g << 5) | bl);
}

// 快速 atan2（误差约 0.01°），返回 0~1：0 = 正上方，顺时针
static inline float ringAngle(float dx, float dy) {
  float ax = fabsf(dx), ay = fabsf(dy);
  float mx = fmaxf(ax, ay), mn = fminf(ax, ay);
  float a = mx > 0 ? mn / mx : 0, s2 = a * a;
  float r = ((-0.0464964749f * s2 + 0.15931422f) * s2 - 0.327622764f) * s2 * a + a;
  if (ay > ax) r = 1.57079637f - r;
  if (dx < 0) r = 3.14159265f - r;      // 这里的 “x” 是向上的分量，见下面调用
  if (dy < 0) r = -r;
  float t = r / 6.2831853f;
  return t < 0 ? t + 1 : t;
}

// 外圈参数
struct { uint16_t *fb; bool track; float prog; float ph; uint16_t pcol; bool over; float trip; } RP;   // v7.4 trip：行程环（-1 = 不画）
static inline float bandCov(float r, float rIn, float rOut) {
  float a = fminf(r - rIn + 0.5f, rOut - r + 0.5f);
  return a <= 0 ? 0 : (a >= 1 ? 1 : a);
}

// v6.0 整数版。坐标放大 2 倍取整：ix = 2x+1-W，iy = 2y+1-H，q = ix²+iy² = 4×半径²
#define RQ_MIN  (4 * 209 * 209)              // 半径 209 以内不用管
#define RQ_MAX  (4 * 235 * 235)              // 半径 235 以外整段清零
#define RQ_SH   4
uint16_t *rq2r = nullptr;                    // (q - RQ_MIN) >> 4 → 半径 × 64
int16_t ringInH[LCD_HEIGHT], ringOutH[LCD_HEIGHT];   // 每行：内圈半宽（以内跳过）、外圈半宽（以外清零）
float ringUx = 0, ringUy = -1;               // 进度条末端方向
float tripUx = 0, tripUy = -1;               // v7.4 行程环“走过 / 没走”交界方向

bool ringTablesInit() {
  if (rq2r) return true;
  int n = ((RQ_MAX - RQ_MIN) >> RQ_SH) + 2;
  rq2r = (uint16_t *)heap_caps_malloc(n * 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (!rq2r) return false;
  for (int k = 0; k < n; k++) rq2r[k] = (uint16_t)(sqrtf((float)(RQ_MIN + (k << RQ_SH) + 8)) * 32.0f);   // √q/2×64
  for (int y = 0; y < LCD_HEIGHT; y++) {
    int iy = 2 * y + 1 - LCD_HEIGHT;
    long iy2 = (long)iy * iy;
    int hin = 0, hout = 0;
    for (int x = LCD_WIDTH / 2; x < LCD_WIDTH; x++) {                // 右半边往外找
      int ix = 2 * x + 1 - LCD_WIDTH;
      long q = (long)ix * ix + iy2;
      if (q < RQ_MIN) hin = x - LCD_WIDTH / 2 + 1;
      if (q < RQ_MAX) hout = x - LCD_WIDTH / 2 + 1;
    }
    ringInH[y] = hin; ringOutH[y] = hout;
  }
  return true;
}

// 一段圆环的覆盖率（0~64）：r64 = 半径×64；带宽 [a, b]
static inline int bandCov64(int r64, int a, int b) {
  int c = min(r64 - (a * 64 - 32), (b * 64 + 32) - r64);
  return c <= 0 ? 0 : (c >= 64 ? 64 : c);
}

static inline void ringPixel(uint16_t *px, int ix, long iy2, int iy) {
  long q = (long)ix * ix + iy2;
  int k = (int)((q - RQ_MIN) >> RQ_SH);
  int r64 = rq2r[k < 0 ? 0 : k];
  uint16_t c = *px;
  int a = bandCov64(r64, 211, 240);
  if (a) c = a >= 64 ? C_BLACK : blendF(c, C_BLACK, a * 4);
  if (RP.track) { a = bandCov64(r64, RING_R - 5, RING_R + 5); if (a) c = a >= 64 ? C_TRACK : blendF(c, C_TRACK, a * 4); }
  if (RP.trip >= 0) {                                  // v7.4 行程环：走过的灰，没走的绿
    a = bandCov64(r64, RING_R - TRIP_H, RING_R + TRIP_H);
    if (a) {
      bool right = ix >= 0;
      bool before = (ix * tripUy - iy * tripUx) >= 0;
      bool done = RP.trip >= 0.9999f ? true : RP.trip <= 0.0001f ? false : (RP.trip <= 0.5f ? (right && before) : (right || before));
      uint16_t tc = done ? TRIP_DONE_C : TRIP_LEFT_C;
      c = a >= 64 ? tc : blendF(c, tc, a * 4);
    }
  }
  if (RP.prog > 0.002f) {
    int h = (int)RP.ph;
    a = bandCov64(r64, RING_R - h, RING_R + h);
    if (a) {
      // 在进度范围内吗：正上方顺时针转到末端方向 (ux, uy)
      bool right = ix >= 0;
      bool before = (ix * ringUy - iy * ringUx) >= 0;
      bool in = RP.prog >= 0.9999f ? true : (RP.prog <= 0.5f ? (right && before) : (right || before));
      if (in) c = a >= 64 ? RP.pcol : blendF(c, RP.pcol, a * 4);
    }
  }
  if (RP.over) { a = bandCov64(r64, 228, 233); if (a) c = a >= 64 ? C_RED : blendF(c, C_RED, a * 4); }
  *px = c;
}

void ringRows(int y0, int y1, int step) {
  uint16_t *fb = RP.fb;
  const int W = LCD_WIDTH, HW = LCD_WIDTH / 2;
  for (int y = y0; y < y1; y += step) {
    uint16_t *row = fb + y * W;
    int iy = 2 * y + 1 - LCD_HEIGHT;
    long iy2 = (long)iy * iy;
    int ho = ringOutH[y], hi = ringInH[y];
    // 圆外两头：整段清零（黑色 = 0）
    if (HW - ho > 0) memset(row, 0, (HW - ho) * 2);
    if (HW + ho < W) memset(row + HW + ho, 0, (W - HW - ho) * 2);
    // 左边一段 [HW-ho, HW-hi)，右边一段 [HW+hi, HW+ho)
    for (int x = HW - ho; x < HW - hi; x++) ringPixel(row + x, 2 * x + 1 - W, iy2, iy);
    for (int x = HW + hi; x < HW + ho; x++) ringPixel(row + x, 2 * x + 1 - W, iy2, iy);
  }
}

// 画外圈：黑边 + 进度环底（track）+ 进度条（prog 0~1，粗 thick）+ 超速红圈
void drawRingLayer(bool track, float prog, int thick, uint16_t pcol, bool over, float trip) {
  RP.fb = gfx->getFramebuffer();
  if (!RP.fb) return;
  RP.track = track; RP.prog = prog > 1 ? 1 : prog; RP.ph = thick / 2.0f; RP.pcol = pcol; RP.over = over;
  RP.trip = trip; tripUx = sinf(trip * 2 * PI); tripUy = -cosf(trip * 2 * PI);
  if (!ringTablesInit()) {                             // 内存不够：退回老办法
    fillRingBand(0, 1, 211, 240, C_BLACK);
    if (trip >= 0) drawTripRing(trip);
    if (track) drawRingTrack();
    if (prog > 0.002f) drawRingProgress(prog, thick, pcol);
    if (over) fillRingBand(0, 1, 228, 233, C_RED);
    return;
  }
  ringUx = sinf(RP.prog * 2 * PI); ringUy = -cosf(RP.prog * 2 * PI);
  parallelRows(ringRows, 0, LCD_HEIGHT);
  if (RP.prog > 0.002f) {                              // 进度条两头的圆头
    float h = RP.ph, a = RP.prog * 2 * PI;
    aaDisc(CX, CX - RING_R, h, pcol);
    aaDisc(CX + RING_R * sinf(a), CX - RING_R * cosf(a), h, pcol);
  }
  if (trip >= 0) drawTripDot(trip);
}

// 边缘渐暗：半径 r0~r1 之间越往外越暗；暗度按 “半径平方” 查表（表放在芯片内部内存）
uint8_t *vigKeep = nullptr;                // 下标 = 半径² - r0²，值 = 保留亮度（1~256 存成 0~255）
int vigK0 = 0, vigK1 = 0;
struct { uint16_t *fb; float r0, r1; } VP;

void vignetteRows(int y0, int y1, int step) {
  uint16_t *fb = VP.fb;
  float r0 = VP.r0, r1 = VP.r1, ri2 = r0 * r0, ro2 = r1 * r1;
  for (int y = y0; y < y1; y += step) {
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
        int k = (int)(dx * dx + dy2) - vigK0;
        if (k < 0 || k >= vigK1 - vigK0) continue;
        uint32_t keep = (uint32_t)vigKeep[k] + 1;
        if (keep >= 256) continue;
        uint32_t c = row[x];
        row[x] = (uint16_t)((((c >> 11) * keep >> 8) << 11) | ((((c >> 5) & 63) * keep >> 8) << 5) | ((c & 31) * keep >> 8));
      }
    }
  }
}

void drawVignette(float r0, float r1, int maxDark) {
  VP.fb = gfx->getFramebuffer();
  if (!VP.fb) return;
  if (!vigKeep || vigK0 != (int)(r0 * r0) || vigK1 != (int)(r1 * r1) + 1) {
    vigK0 = (int)(r0 * r0); vigK1 = (int)(r1 * r1) + 1;
    if (!vigKeep) vigKeep = (uint8_t *)heap_caps_malloc(vigK1 - vigK0 + 64, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!vigKeep) { radarVignette(r0, r1, maxDark); return; }
    for (int k = 0; k < vigK1 - vigK0; k++) {
      float r = sqrtf((float)(k + vigK0) + 0.5f);
      float t = (r - r0) / (r1 - r0);
      if (t < 0) t = 0;
      if (t > 1) t = 1;
      int dark = (int)(t * t * maxDark);
      vigKeep[k] = (uint8_t)(255 - dark);
    }
  }
  VP.r0 = r0; VP.r1 = r1;
  parallelRows(vignetteRows, (int)(CX - r1) - 1, (int)(CX + r1) + 2);
}

// ================= v6.1 风格特效 =================
static inline void projXY(float x, float y, float cx, float cy, float sc, float cs, float sn, float &px, float &py) {
  px = CX + sc * ((x - cx) * cs - (y - cy) * sn);
  py = effectiveMapPy - sc * ((x - cx) * sn + (y - cy) * cs);
}

// 蓝图：跟着地图走的网格，每 50 米一条线
void drawBlueprintGrid(float cx, float cy, float sc, float cs, float sn, float viewR) {
  const float G = 50;
  uint16_t col = nightOn ? 0x1A0F : 0x2AB2;
  float x0 = floorf((cx - viewR) / G) * G, y0 = floorf((cy - viewR) / G) * G;
  for (float g = x0; g <= cx + viewR; g += G) {
    float ax, ay, bx, by;
    projXY(g, cy - viewR, cx, cy, sc, cs, sn, ax, ay); projXY(g, cy + viewR, cx, cy, sc, cs, sn, bx, by);
    gfx->drawLine((int)ax, (int)ay, (int)bx, (int)by, col);
  }
  for (float g = y0; g <= cy + viewR; g += G) {
    float ax, ay, bx, by;
    projXY(cx - viewR, g, cx, cy, sc, cs, sn, ax, ay); projXY(cx + viewR, g, cx, cy, sc, cs, sn, bx, by);
    gfx->drawLine((int)ax, (int)ay, (int)bx, (int)by, col);
  }
}

// 雷达：三圈距离环 + 绕着车转的扫描线（后面拖一串渐暗的线）
void drawRadarSweep() {
  for (int r = 70; r <= 210; r += 70) gfx->drawCircle(CX, (int)effectiveMapPy, r, 0x0A44);
  float a0 = (millis() % 3200) / 3200.0f * 2 * PI;
  const int TRAIL = 26;
  for (int k = TRAIL - 1; k >= 0; k--) {
    float a = a0 - k * 0.022f;
    uint16_t c = blendF(0x00C1, 0x8FF4, (uint32_t)((TRAIL - k) * 256 / TRAIL));
    gfx->drawLine(CX, (int)effectiveMapPy, (int)(CX + sinf(a) * 300), (int)(effectiveMapPy - cosf(a) * 300), c);
  }
}

// 寻宝罗盘：屏幕上一层淡淡的六边形网格（开机第一次用时把网格上的像素位置算好存起来）
uint32_t *hexIdx = nullptr;
int hexN = 0;
bool hexTried = false;
void buildHexGrid() {
  hexTried = true;
  uint8_t *m = (uint8_t *)ps_malloc(LCD_WIDTH * LCD_HEIGHT);
  if (!m) return;
  memset(m, 0, LCD_WIDTH * LCD_HEIGHT);
  const float R = 26, H = R * 1.7320508f;
  auto line = [&](float x0, float y0, float x1, float y1) {
    int n = (int)(fmaxf(fabsf(x1 - x0), fabsf(y1 - y0))) + 1;
    for (int i = 0; i <= n; i++) {
      int x = (int)(x0 + (x1 - x0) * i / n), y = (int)(y0 + (y1 - y0) * i / n);
      if (x >= 0 && x < LCD_WIDTH && y >= 0 && y < LCD_HEIGHT) m[y * LCD_WIDTH + x] = 1;
    }
  };
  for (int col = -1; col * R * 1.5f < LCD_WIDTH + R; col++)
    for (int row = -1; row * H < LCD_HEIGHT + H; row++) {
      float cx = col * R * 1.5f, cy = row * H + ((col & 1) ? H / 2 : 0);
      for (int k = 0; k < 6; k++) {
        float a0 = k * PI / 3, a1 = (k + 1) * PI / 3;
        line(cx + R * cosf(a0), cy + R * sinf(a0), cx + R * cosf(a1), cy + R * sinf(a1));
      }
    }
  int cnt = 0;
  for (int i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++) if (m[i]) cnt++;
  hexIdx = (uint32_t *)ps_malloc(sizeof(uint32_t) * (cnt + 1));
  if (hexIdx) {
    for (int y = 0; y < LCD_HEIGHT; y++) {
      float dy = y + 0.5f - CX;
      for (int x = 0; x < LCD_WIDTH; x++) {
        if (!m[y * LCD_WIDTH + x]) continue;
        float dx = x + 0.5f - CX;
        if (dx * dx + dy * dy < 206.0f * 206.0f) hexIdx[hexN++] = y * LCD_WIDTH + x;
      }
    }
  }
  free(m);
}

void drawHexGrid() {
  if (!hexTried) buildHexGrid();
  if (!hexIdx) return;
  uint16_t *fb = gfx->getFramebuffer();
  for (int i = 0; i < hexN; i++) { uint16_t &p = fb[hexIdx[i]]; p = blendF(p, 0x3A4F, 80); }
}

// 寻宝罗盘：外圈刻度（每 5° 一根，东南西北长一点）+ 方位字母，跟着车头转
void drawCompassRose(float hdgDeg) {
  for (int k = 0; k < 72; k++) {
    float a = (k * 5 - hdgDeg) * PI / 180;
    bool major = k % 18 == 0;
    float r0 = 208, r1 = major ? 192 : (k % 2 == 0 ? 199 : 203);
    float sx = sinf(a), cy = -cosf(a);
    uint16_t c = major ? 0xFD80 : 0x7B48;
    gfx->drawLine((int)(CX + sx * r0), (int)(CX + cy * r0), (int)(CX + sx * r1), (int)(CX + cy * r1), c);
    if (major) gfx->drawLine((int)(CX + sx * r0) + 1, (int)(CX + cy * r0), (int)(CX + sx * r1) + 1, (int)(CX + cy * r1), c);
  }
  const char *L[4] = {"N", "E", "S", "W"};
  for (int k = 0; k < 4; k++) {
    float a = (k * 90 - hdgDeg) * PI / 180;
    float x = CX + sinf(a) * 178, y = CX - cosf(a) * 178;
    if (k == 0) obsAdd(x - 10, y - 10, x + 10, y + 10);
    printOutlined(L[k], (int)x, (int)y - 7, 2, k == 0 ? 0xFD80 : 0x7B48);
  }
}

// 寻宝罗盘：目的地 = 琥珀色菱形 + 往外扩的脉冲圈
void drawCompassBlip(float px, float py, bool outside) {
  if (!outside) {
    float ph = (millis() % 1400) / 1400.0f;
    for (int k = 0; k < 2; k++) {
      float t = fmodf(ph + k * 0.5f, 1.0f);
      int r = (int)(12 + t * 20);
      uint16_t c = blendF(0xFD80, TH.block, (uint32_t)(t * 256));
      gfx->drawCircle((int)px, (int)py, r, c);
      gfx->drawCircle((int)px, (int)py, r + 1, c);
    }
  }
  float s = outside ? 8 : 11;
  gfx->fillTriangle(px, py - s - 2, px + s + 2, py, px - s - 2, py, C_BLACK);
  gfx->fillTriangle(px, py + s + 2, px + s + 2, py, px - s - 2, py, C_BLACK);
  gfx->fillTriangle(px, py - s, px + s, py, px - s, py, 0xFD80);
  gfx->fillTriangle(px, py + s, px + s, py, px - s, py, 0xFD80);
}

// v5.8 分步计时（微秒累计，每 2 秒发一次）
#define PROF_N 11
uint32_t profAcc[PROF_N];
uint32_t profT = 0;
static inline void profMark(int i) { uint32_t n = micros(); profAcc[i] += n - profT; profT = n; }

void drawMapFrame() {
  unsigned long t = millis();
  lastMapDraw = t;
  frameIsMap = true;

  if (crossProgress != crossTargetProg) {
    float dt = (t - crossAnimAt) / 320.0f;
    if (dt >= 1.0f) {
      crossProgress = crossTargetProg;
    } else {
      float ease = crossTargetProg > crossStartProg
          ? sinf(dt * 1.5707963f)
          : (1.0f - cosf(dt * 1.5707963f));
      crossProgress = crossStartProg + (crossTargetProg - crossStartProg) * ease;
    }
    navDirty = true;
  }
  effectiveMapPy = MAP_PY + crossProgress * (LCD_HEIGHT / 3.0f);
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
  float viewR = (CX + (effectiveMapPy - CX) + 30) / sc;

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
      tmpY[k] = effectiveMapPy - sc * (dx * sn + dy * cs);
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
  drawPlayer(CX, effectiveMapPy);
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
  drawBattery(CX - 28, 14, true);                         // 电量（车速上方居中，内嵌电量数字带黑底）
  drawTrafficLightLab(millis());                        // 红绿灯竖排卡片
  profMark(10);
  drawJunctionCrossCard(crossProgress);
  // v2.9 小地图里不画顶部的蓝牙小圆点（会和 N 标记重叠；能收到地图就说明连着）
  needFlush = true;
  mapFrames++;
  mapDrawMsAcc += millis() - t;
  ovlMsAcc += millis() - tMapDone;
}

