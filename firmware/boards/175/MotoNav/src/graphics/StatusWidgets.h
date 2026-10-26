#pragma once
// 电池、连接状态、车速和限速牌。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
void drawBattery(int x0, int y0, bool backing = false) {
  if (batPct < 0) return;                                  // 没接电池不画
  if (backing) {                                           // v7.2 小地图上垫一块黑底
    int tw = String(batPct).length() * 6;
    gfx->fillRoundRect(x0 - 4, y0 - 3, 24 + 2 + 5 + tw + 7, 12 + 6, 6, C_BLACK);
  }
  uint16_t c = batChg ? C_CYAN : batPct > 50 ? C_GREEN : batPct >= 20 ? 0xFE60 : C_RED;
  if (!batChg && batPct < 10 && (millis() / 600) % 2) c = 0x4208;   // 快没电：闪
  const int W = 24, H = 12;
  gfx->drawRoundRect(x0, y0, W, H, 3, c);
  gfx->drawRoundRect(x0 + 1, y0 + 1, W - 2, H - 2, 2, c);
  gfx->fillRect(x0 + W, y0 + 3, 2, H - 6, c);
  int fw = (W - 6) * constrain(batPct, 0, 100) / 100;
  if (fw > 0) gfx->fillRect(x0 + 3, y0 + 3, fw, H - 6, c);
  if (batChg) {                                            // 小闪电
    int bx = x0 + W / 2, by = y0 + H / 2;
    gfx->fillTriangle(bx + 1, by - 5, bx - 3, by + 1, bx, by + 1, C_BLACK);
    gfx->fillTriangle(bx - 1, by + 5, bx + 3, by - 1, bx, by - 1, C_BLACK);
  }
  gfx->setTextSize(1);
  gfx->setTextColor(c);
  gfx->setCursor(x0 + W + 5, y0 + 2);
  gfx->print(String(batPct));
}

void drawStatus() {
  gfx->fillRect(CX - 9, 21, 9 + 14 + 24 + 2 + 5 + 18 + 4, 19, C_BLACK);   // v7.2 先擦掉旧的（只重画状态栏时用）
  aaDisc(CX, 30, 7, bleConnected ? C_GREEN : C_GREY);
  drawBattery(CX + 14, 24);                               // v6.8 电量（蓝牙小圆点右边）
  needFlush = true;
}

// ================= 车速和限速牌 =================

void drawSpeedBlock(bool over) {
  if (speedKmh < 0) return;
  int sx = hasCam ? 200 : CX;
  printCentered(String(speedKmh), sx, SPEED_Y, 5, over ? C_RED : C_WHITE);   // 每个字符 30x40
  printCentered("km/h", sx, SPEED_Y + 46, 2, over ? 0xFC10 : C_GREY);
  if (hasCam) {
    const int bx = 298, by = 76;
    aaDisc(bx, by, 28, C_SIGNRED);
    aaDisc(bx, by, 21, C_WHITE);
    String lim = String(camLimit);
    int sz = lim.length() >= 3 ? 2 : 3;
    printCentered(lim, bx, by - 4 * sz, sz, C_BLACK);
    printCentered(distText(camDist), bx, by + 34, 2, over ? 0xFC10 : C_GREY);
  }
}
