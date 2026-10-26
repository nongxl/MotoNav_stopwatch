#pragma once
// 电量轮询及状态消息。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
String pendingStat2 = "";
// v6.8 每 5 秒读一次；有变化就重画、通知手机
void batPoll() {
  if (!pmuOK) return;
  unsigned long now = millis();
  if (batAt && now - batAt < 5000) return;
  batAt = now;
  int s1 = pmuRead(0x00), s2 = pmuRead(0x01);
  if (s1 < 0 || s2 < 0) return;
  bool present = (s1 >> 3) & 1;
  int pct = -1;
  if (present) {
    int v = pmuRead(0xA4);
    if (v >= 0 && v <= 100) pct = v;
    int h = pmuRead(0x34), l = pmuRead(0x35);
    if (h >= 0 && l >= 0) batMv = ((h & 0x1F) << 8) | l;
  }
  // v7.2 偶尔读不到电池：先沿用上次的电量，连续 3 次读不到才当成没接电池
  static int missCnt = 0;
  if (pct < 0) { if (batPct >= 0 && ++missCnt < 3) { pct = batPct; present = true; } }
  else missCnt = 0;
  bool chg = present && ((s2 >> 5) & 3) == 1;
  bool usb = ((s1 >> 5) & 1) && !((s2 >> 3) & 1);
  bool changed = pct != batPct || chg != batChg || usb != usbIn;
  batPct = pct; batChg = chg; usbIn = usb;
  if (mediaHid && changed && pct >= 0) mediaHid->setBatteryLevel((uint8_t)constrain(pct, 0, 100));
  if (changed) {
    if (onWaitScreen && page == 0 && !sliding) drawStatus();        // v7.2 只重画状态栏
    else if (inNav) navDirty = true;
  }
  if (changed || now - batSentAt > 60000) {
    batSentAt = now;
    pendingBat = "V," + String(batPct) + "," + String(batChg ? 1 : 0) + "," + String(usbIn ? 1 : 0) + "," + String(batMv);
  }
}
