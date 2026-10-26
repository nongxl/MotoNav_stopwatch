#pragma once
// 双核逐行渲染调度。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
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
