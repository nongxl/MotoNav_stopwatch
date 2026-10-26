#pragma once
// 五分钟闲置关机和深睡回退。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// Power down before destroying BLE objects: the host can still dispatch events
// referencing those objects. Shutdown does not need to reclaim their memory.
void idlePowerOffShutdown() {
  powerOffPending = true;
  Serial.println("No navigation for 5 minutes: powering off");
  const bool displayReady = !flushDone ||
      xSemaphoreTake(flushDone, pdMS_TO_TICKS(3000)) == pdTRUE;
  if (displayReady) {
    panel->setBrightness(0);
    panel->displayOff();
  }
  // Check the chip ID, rather than writing to an unknown device at address 0x34.
  const int chip = pmuRead(0x03);
  Serial.printf("Shutdown: PMIC id=%02X, on=%02X, off=%02X\n",
                chip, pmuRead(0x20), pmuRead(0x21));
  if (chip == 0x4A) {
    const int config = pmuRead(0x10);
    if (config >= 0) {
      Wire.beginTransmission(PMU_ADDR);
      Wire.write(0x10);
      // Bit 1 is a separate, self-clearing RESET command; never replay it.
      Wire.write((uint8_t)((config & ~0x02) | 0x01));
      const int result = Wire.endTransmission();
      Serial.printf("Shutdown: PMIC command result=%d\n", result);
      delay(1000);
    }
  }
  // If the PMIC is unavailable or external power keeps the CPU supplied, stop
  // screen/BLE/CPU activity instead of repeatedly rebooting or staying awake.
  // Normal PMIC-off wakes via PWR; this fallback can also wake via BOOT (GPIO0).
  Serial.println("PMIC did not remove CPU power; entering deep sleep");
  // Stop the host/controller without deleting live GATT objects. Deep sleep
  // discards RAM anyway. Keep objects valid until pending callbacks have ended.
#if defined(CONFIG_NIMBLE_ENABLED)
  nimble_port_stop();
#elif defined(CONFIG_BLUEDROID_ENABLED)
  esp_bluedroid_disable();
#endif
#if CONFIG_BT_CONTROLLER_ENABLED
  esp_bt_controller_disable();
#endif
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  rtc_gpio_init(GPIO_NUM_0);
  rtc_gpio_set_direction(GPIO_NUM_0, RTC_GPIO_MODE_INPUT_ONLY);
  rtc_gpio_pullup_en(GPIO_NUM_0);
  rtc_gpio_pulldown_dis(GPIO_NUM_0);
  // EXT0 is level-triggered: arming it while BOOT is held low causes an
  // immediate wake/reboot. Require 100 ms released, with a bounded wait.
  uint32_t releasedAt = millis(), waitAt = releasedAt;
  while (millis() - waitAt < 1500 && millis() - releasedAt < 100) {
    if (!rtc_gpio_get_level(GPIO_NUM_0)) releasedAt = millis();
    delay(10);
  }
  if (millis() - releasedAt >= 100 && rtc_gpio_get_level(GPIO_NUM_0)) {
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, 0);
  } else {
    Serial.println("BOOT is low; leave wake disabled to avoid instant restart (use PWR to power cycle)");
  }
  esp_deep_sleep_start();
}
