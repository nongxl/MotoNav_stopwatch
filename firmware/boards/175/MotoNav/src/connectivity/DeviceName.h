#pragma once
// 蓝牙设备名称。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// v7.0 蓝牙名 = "MotoNav-" + 蓝牙地址最后两个字节
void makeBleName() {
  uint8_t mac[6] = {0};
  if (esp_read_mac(mac, ESP_MAC_BT) != ESP_OK) return;
  char b[20];
  snprintf(b, sizeof(b), "%s-%02X%02X", DEVICE_NAME, mac[4], mac[5]);
  bleName = b;
}
