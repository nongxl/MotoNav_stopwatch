#pragma once
// 双缓冲、旋转和 QSPI 推屏。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// ---- v2.2 双缓冲：核心 0 上的刷屏任务 ----
uint16_t *fbBuf[2] = {nullptr, nullptr};
int fbCur = 0;                    // 画布当前用的是哪一块
SemaphoreHandle_t flushReq = nullptr, flushDone = nullptr;
uint16_t *volatile flushSrc = nullptr;
volatile uint32_t flushMsAcc = 0, flushCnt = 0;

// ---- v2.6 快速刷屏：自己往屏幕数据线上排队发送，两块缓冲轮流用（一块在传、一块在准备）----
#include "../../QuarterTurnBlit.h"
#define FAST_PIX 4096                     // 每段像素数（8KB）
spi_device_handle_t fastDev = nullptr;
uint8_t *fastBuf[2] = {nullptr, nullptr};

int screenRot = 0;                        // v6.2 圆屏方向：0 / 1 / 2 / 3 = 0° / 90° / 180° / 270°
int screenDeg = 0;                        // v6.4 圆屏方向（度，0~359）
bool rotFree = false;                     // v6.4 不是 90° 的整数倍：推屏时逐像素按角度取样
int32_t rotCos = 65536, rotSin = 0;       // v6.4 角度的 cos / sin（×65536）
float rotCosF = 1, rotSinF = 0;
unsigned long rotSaveAt = 0;              // v6.4 角度停下后过一会儿再存进板子
int16_t rotSpan[LCD_HEIGHT];              // v6.4 每一行圆里面的半宽（圆外面的像素直接涂黑）

// v6.4 设定圆屏角度。save = true：稍后存进板子
void setScreenDeg(int d, bool save) {
  d = ((d % 360) + 360) % 360;
  screenDeg = d;
  rotFree = (d % 90) != 0;
  screenRot = rotFree ? 0 : d / 90;
  float a = d * (float)M_PI / 180.0f;
  rotCosF = cosf(a); rotSinF = sinf(a);
  rotCos = (int32_t)lroundf(rotCosF * 65536.0f);
  rotSin = (int32_t)lroundf(rotSinF * 65536.0f);
  if (save) rotSaveAt = millis() + 1500;
}

void initRotSpan() {
  const int c = LCD_WIDTH / 2;
  for (int y = 0; y < LCD_HEIGHT; y++) {
    int dy = y - c;
    int r2 = c * c - dy * dy;
    rotSpan[y] = r2 > 0 ? (int16_t)sqrtf((float)r2) : 0;
  }
}

// v6.4 停下 1.5 秒后把角度存起来（loop 里调用）
void saveRotIfDue() {
  if (!rotSaveAt || (long)(millis() - rotSaveAt) < 0) return;
  rotSaveAt = 0;
  Preferences pr;
  pr.begin("motonav", false);
  pr.putInt("rotd", screenDeg);
  pr.putInt("rot", screenRot);
  pr.end();
}

bool fastFlushInit() {
  spi_device_interface_config_t dc;
  memset(&dc, 0, sizeof(dc));
  dc.command_bits = 8;
  dc.address_bits = 24;
  dc.mode = 0;
  dc.clock_source = SPI_CLK_SRC_DEFAULT;
  dc.clock_speed_hz = qspiMHz * 1000000;
  dc.spics_io_num = -1;                   // 片选自己控制（和屏幕库一样）
  dc.flags = SPI_DEVICE_HALFDUPLEX;
  dc.queue_size = 2;
  if (spi_bus_add_device(SPI2_HOST, &dc, &fastDev) != ESP_OK) { fastDev = nullptr; return false; }
  for (int i = 0; i < 2; i++) {
    fastBuf[i] = (uint8_t *)heap_caps_aligned_alloc(16, FAST_PIX * 2, MALLOC_CAP_DMA);
    if (!fastBuf[i]) { fastDev = nullptr; return false; }
  }
  return true;
}

// v7.7 RGB565 拆成 0x0GGRRBB 形式（绿、红、蓝之间留空），一次乘法就能三个颜色一起按权重混合
static inline uint32_t rgbSpread(uint16_t c) { return ((uint32_t)c | ((uint32_t)c << 16)) & 0x07E0F81Fu; }

void __attribute__((optimize("O3"))) fastPush(const uint16_t *fb) {
  panel->startWrite();
  panel->writeAddrWindow(0, 0, LCD_WIDTH, LCD_HEIGHT);   // 设置写入区域（整屏）+ 写显存命令
  panel->endWrite();
  spi_device_acquire_bus(fastDev, portMAX_DELAY);
  digitalWrite(LCD_CS, LOW);
  static spi_transaction_ext_t tr[2];
  const uint32_t total = (uint32_t)LCD_WIDTH * LCD_HEIGHT;
  uint32_t sent = 0;
  int bi = 0, inflight = 0;
  bool first = true;
  while (sent < total) {
    uint32_t l = total - sent;
    const uint32_t block=(!rotFree && (screenRot==1 || screenRot==3))?LCD_WIDTH*8:FAST_PIX;
    if (l > block) l = block;
    if (inflight == 2) {                                  // 两块都在传：等最早那块传完再复用
      spi_transaction_t *r;
      spi_device_get_trans_result(fastDev, &r, portMAX_DELAY);
      inflight--;
    }
    // 拷贝并交换高低字节（屏幕要高字节在前）
    if (rotFree) {
      // v6.4 任意角度：屏幕上的点 (px,py) 取画布上转过去的点；圆外面涂黑
      uint16_t *d16 = (uint16_t *)fastBuf[bi];
      const int W = LCD_WIDTH, C = LCD_WIDTH / 2;
      int py = sent / W, px = sent - py * W;
      int32_t fx = 0, fy = 0;
      int xs = 0, xe = -1;
      bool rowInit = false;
      for (uint32_t i = 0; i < l; i++) {
        if (!rowInit) {
          int hw = rotSpan[py];
          xs = C - hw; xe = C + hw;
          // 以像素中心算（屏幕宽 466 是双数，正中心在 232.5）：dx2、dy2 是离中心距离的 2 倍
          int dx2 = 2 * px - (W - 1), dy2 = 2 * py - (W - 1);
          // 源点 = (dx·cos + dy·sin, −dx·sin + dy·cos) + 中心；+0.5 四舍五入
          fx = ((dx2 * rotCos + dy2 * rotSin) >> 1) + ((W - 1) << 15) + 32768;
          fy = ((-dx2 * rotSin + dy2 * rotCos) >> 1) + ((W - 1) << 15) + 32768;
          rowInit = true;
        }
        uint16_t v = 0;
        if (px >= xs && px <= xe) {
          // v7.7 双线性插值：取周围 4 个点按距离混合（以前只取最近的一个点，斜着转时字的笔画粗细不均、发糊）
          int32_t bx = fx - 32768, by = fy - 32768;          // 去掉“四舍五入”的 0.5，换成小数部分当权重
          int sx = bx >> 16, sy = by >> 16;
          if ((unsigned)sx < (unsigned)(W - 1) && (unsigned)sy < (unsigned)(W - 1)) {
            const uint16_t *r0 = fb + sy * W + sx, *r1 = r0 + W;
            uint32_t ax = (bx >> 11) & 31, ay = (by >> 11) & 31;   // 0~31 的权重
            uint32_t a = rgbSpread(r0[0]), b = rgbSpread(r0[1]), c = rgbSpread(r1[0]), d = rgbSpread(r1[1]);
            uint32_t top = ((a * (32 - ax) + b * ax) >> 5) & 0x07E0F81Fu;
            uint32_t bot = ((c * (32 - ax) + d * ax) >> 5) & 0x07E0F81Fu;
            uint32_t m = ((top * (32 - ay) + bot * ay) >> 5) & 0x07E0F81Fu;
            v = (uint16_t)(m | (m >> 16));
          } else {
            int nx = fx >> 16, ny = fy >> 16;                 // 画布最边上一圈：还按最近的点取
            if ((unsigned)nx < (unsigned)W && (unsigned)ny < (unsigned)W) v = fb[ny * W + nx];
          }
        }
        d16[i] = (uint16_t)((v >> 8) | (v << 8));
        fx += rotCos; fy -= rotSin;
        if (++px == W) { px = 0; py++; rowInit = false; }
      }
    } else if (screenRot == 0) {
      const uint32_t *src = (const uint32_t *)(fb + sent);
      uint32_t *dst = (uint32_t *)fastBuf[bi];
      for (uint32_t i = 0; i < l / 2; i++) {
        uint32_t v = src[i];
        dst[i] = ((v & 0x00FF00FFu) << 8) | ((v >> 8) & 0x00FF00FFu);
      }
    } else if((screenRot==1 || screenRot==3) && sent%LCD_WIDTH==0 && l%LCD_WIDTH==0) {
      QuarterTurnBlit::pack((uint16_t*)fastBuf[bi],fb,LCD_WIDTH,sent/LCD_WIDTH,l/LCD_WIDTH,screenRot);
    } else {
      // v6.2 转向：屏幕上第 j 个像素取画布上转过去的那个位置
      uint16_t *d16 = (uint16_t *)fastBuf[bi];
      const uint32_t W = LCD_WIDTH, N = (uint32_t)LCD_WIDTH * LCD_HEIGHT;
      uint32_t py = sent / W, px = sent - py * W;
      for (uint32_t i = 0; i < l; i++) {
        uint32_t idx;
        if (screenRot == 2) idx = N - 1 - (sent + i);
        else if (screenRot == 1) idx = (W - 1 - px) * W + py;
        else idx = px * W + (W - 1 - py);
        uint16_t v = fb[idx];
        d16[i] = (uint16_t)((v >> 8) | (v << 8));
        if (++px == W) { px = 0; py++; }
      }
    }
    spi_transaction_ext_t &t = tr[bi];
    memset(&t, 0, sizeof(t));
    if (first) {
      t.base.flags = SPI_TRANS_MODE_QIO;
      t.base.cmd = 0x32;
      t.base.addr = 0x003C00;
      first = false;
    } else {
      t.base.flags = SPI_TRANS_MODE_QIO | SPI_TRANS_VARIABLE_CMD | SPI_TRANS_VARIABLE_ADDR | SPI_TRANS_VARIABLE_DUMMY;
    }
    t.base.tx_buffer = fastBuf[bi];
    t.base.length = l * 16;
    spi_device_queue_trans(fastDev, (spi_transaction_t *)&t, portMAX_DELAY);
    inflight++;
    bi ^= 1;
    sent += l;
  }
  while (inflight) {
    spi_transaction_t *r;
    spi_device_get_trans_result(fastDev, &r, portMAX_DELAY);
    inflight--;
  }
  digitalWrite(LCD_CS, HIGH);
  spi_device_release_bus(fastDev);
}

void flushTask(void *arg) {
  for (;;) {
    xSemaphoreTake(flushReq, portMAX_DELAY);
    uint32_t t0 = millis();
    if (fastDev) fastPush(flushSrc);
    else panel->draw16bitRGBBitmap(0, 0, flushSrc, LCD_WIDTH, LCD_HEIGHT);
    flushMsAcc += millis() - t0; flushCnt++;
    xSemaphoreGive(flushDone);
  }
}

// v2.6 等刷屏完成；超过 3 秒还没完成，说明快速刷屏在这块板子上不工作：关掉它并重启
void flushWait() {
  if (xSemaphoreTake(flushDone, pdMS_TO_TICKS(3000)) == pdTRUE) return;
  if (fastFlushOn) saveAndRestart("fastf", 0, true);
  xSemaphoreTake(flushDone, portMAX_DELAY);
}

// 异步刷屏：把刚画好的这一块交给核心 0 推到屏幕，画布换到另一块继续画下一帧
void flushAsync() {
  if (!fbBuf[1]) { gfx->flush(); return; }
  flushWait();                                    // 等上一帧推完
  flushSrc = fbBuf[fbCur];
  xSemaphoreGive(flushReq);
  fbCur ^= 1;
  gfx->setFramebuffer(fbBuf[fbCur]);
}

// 同步刷屏（普通画面用，等推完再返回）
void flushSync() {
  if (!fbBuf[1]) { gfx->flush(); return; }
  flushWait();
  flushSrc = fbBuf[fbCur];
  xSemaphoreGive(flushReq);
  flushWait();                                    // 等这一帧推完
  xSemaphoreGive(flushDone);
}

void initDoubleBuffer() {
  fbBuf[0] = gfx->getFramebuffer();
  fbBuf[1] = (uint16_t *)heap_caps_aligned_alloc(16, LCD_WIDTH * LCD_HEIGHT * 2, MALLOC_CAP_SPIRAM);
  if (!fbBuf[1]) return;
  memcpy(fbBuf[1], fbBuf[0], LCD_WIDTH * LCD_HEIGHT * 2);
  if (fastFlushOn) fastFlushInit();                // v2.6 快速刷屏
  flushReq = xSemaphoreCreateBinary();
  flushDone = xSemaphoreCreateBinary();
  xSemaphoreGive(flushDone);
  xTaskCreatePinnedToCore(flushTask, "flush", 4096, nullptr, 2, nullptr, 0);
}
