#pragma once
// 配对码页面。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// ================= v6.5 配对画面 =================
bool pairOn = false;
unsigned long pairShowAt = 0;

void drawPairScreen() {
  onWaitScreen = false;
  frameIsMap = false;
  gfx->fillScreen(C_BLACK);
  printCentered("PAIRING", CX, 120, 3, C_CYAN);
  if (pairLocked) {
    printCentered("LOCKED", CX, 205, 5, C_RED);
    printCentered("restart this screen,", CX, 290, 2, C_GREY);
    printCentered("then pair within 2 min", CX, 315, 2, C_GREY);
  } else {
    char b[8];
    snprintf(b, sizeof(b), "%06lu", (unsigned long)pairCode);
    printCentered(b, CX, 200, 7, C_WHITE);
    printCentered("enter this code", CX, 295, 2, C_GREY);
    printCentered("on your phone", CX, 320, 2, C_GREY);
  }
  printCentered(bleName, CX, 372, 2, C_CYAN);                            // v7.0 确认连的是这块

  needFlush = true;
}
