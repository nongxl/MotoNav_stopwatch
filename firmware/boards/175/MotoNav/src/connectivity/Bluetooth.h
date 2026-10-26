#pragma once
// 配对、媒体 HID 和蓝牙接收队列。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
int bondCount() {
  int n = 0;
#if defined(CONFIG_NIMBLE_ENABLED)
  ble_store_util_count(BLE_STORE_OBJ_TYPE_OUR_SEC, &n);
#endif
  return n;
}
bool pairingAllowed() { return millis() < PAIR_WINDOW_MS || bondCount() == 0; }

class SecCB : public BLESecurityCallbacks {
  uint32_t onPassKeyRequest() { return 0; }
  // 圆屏要显示配对码（每次配对随机生成）：只在允许配对的时间里真的显示出来
  void onPassKeyNotify(uint32_t k) { pairCode = k; pairLocked = !pairingAllowed(); pairDone = false; pairReq = true; }
  bool onSecurityRequest() { return true; }
  bool onConfirmPIN(uint32_t pin) { return false; }
  bool onAuthorizationRequest(uint16_t connHandle, uint16_t attrHandle, bool isRead) { return true; }
#if defined(CONFIG_NIMBLE_ENABLED)
  void onAuthenticationComplete(ble_gap_conn_desc *d) {
    bool ok = d && d->sec_state.encrypted && d->sec_state.authenticated;
    if (ok) { bleAuthed = true; bleAuthAt = millis(); authNotifies = 0; }
    pairOk = ok;
    pairDone = true;
  }
#endif
};

class HidInputCB : public BLECharacteristicCallbacks {
public:
#if defined(CONFIG_NIMBLE_ENABLED)
  void onSubscribe(BLECharacteristic *, ble_gap_conn_desc *desc, uint16_t value) override {
    hidSubscriber = desc ? desc->conn_handle : 0xFFFF;
    hidSubscribed = (value & 1) != 0; ++hidSessionEpoch;
  }
#endif
  void onStatus(BLECharacteristic *, Status status, uint32_t) override {
    hidNotifyAccepted = status == SUCCESS_NOTIFY;
  }
};
class HidControlCB : public BLECharacteristicCallbacks {
public:
  void onWrite(BLECharacteristic *c) override {
    if (c->getLength() == 1 && c->getData()[0] <= 1) { hidSuspended = c->getData()[0] == 0; ++hidSessionEpoch; }
  }
};
class HidProtocolCB : public BLECharacteristicCallbacks {
public:
  void onWrite(BLECharacteristic *c) override {
    if (c->getLength() == 1 && c->getData()[0] <= 1) { hidReportMode = c->getData()[0] == 1; ++hidSessionEpoch; }
  }
};
// Gate every report on this connection's pairing, host subscription and report mode.
bool hidMediaLinkReady() {
  return mediaInput && bleConnected && bleAuthed && hidSubscribed &&
         hidSubscriber == bleConnHandle && !hidSuspended && hidReportMode;
}
bool hidMediaReady() { return hidMediaLinkReady() && !hidReleaseOnReady && hidSeenEpoch == hidSessionEpoch; }
bool hidMediaNotify(uint16_t usage) {
  if (!hidMediaLinkReady()) return false;
  uint8_t report[2]; HidMediaKeys::encode(usage, report);
  hidNotifyAccepted = false;
  mediaInput->setValue(report, sizeof(report)); mediaInput->notify();
  return hidNotifyAccepted; // queued by BLE stack; does not confirm player state
}
void hidMediaTick() {
  static bool resetWritten = false;
  uint32_t epoch = hidSessionEpoch;
  if (hidSeenEpoch != epoch) {
    hidPulses.reset(); hidReleaseOnReady = true; resetWritten = false; hidSeenEpoch = epoch;
  }
  bool linkReady = hidMediaLinkReady();
  if (!linkReady) {
    if (!resetWritten) {
      hidPulses.reset(); hidReleaseOnReady = true;
      if (mediaInput) { const uint8_t zero[2] = {0, 0}; mediaInput->setValue(zero, sizeof(zero)); }
      resetWritten = true;
    }
  } else if (hidReleaseOnReady) {
    resetWritten = false;
    if (!hidInitialReleaseAt || millis() - hidInitialReleaseAt >= 50) {
      hidInitialReleaseAt = millis();
      if (hidMediaNotify(0)) hidReleaseOnReady = false;
    }
  } else {
    if (!hidMusicMode) hidPulses.cancelQueued();
    HidMediaKeys::Pulses::Report report;
    if (hidPulses.poll(millis(), report)) {
      bool sent = hidMediaNotify(report.usage);
      hidPulses.submitted(report, millis(), sent);
      if (!report.release || !sent) {
        musicAck = sent ? 6 : 8; musicAckUntil = millis() + 2500; musicDirty = true;
      }
    }
  }
  bool ready = hidMediaReady();
  static int lastMode = -1;
  if (ready != hidReadyShown || lastMode != (int)hidMusicMode) {
    hidReadyShown = ready; lastMode = hidMusicMode;
    pendingHidStatus = String("HID,") + (hidMusicMode ? 1 : 0) + "," + (ready ? 1 : 0);
    Serial.printf("MEDIA HID ready=%d auth=%d sub=%d suspended=%d report=%d release=%d\n", ready, (bool)bleAuthed, (bool)hidSubscribed, (bool)hidSuspended, (bool)hidReportMode, hidReleaseOnReady);
    musicDirty = true;
  }
}
void initMediaHid(BLEServer *server) {
  mediaHid = new BLEHIDDevice(server);
  mediaHid->manufacturer()->setValue("MotoNav");
  // DIY/private device identity, not a borrowed keyboard vendor/product ID.
  const uint8_t identity[7] = {0x01, 0xFF, 0xFF, 0x01, 0x00, 0x00, 0x01};
  mediaHid->deviceInfo()->getCharacteristic(BLEUUID((uint16_t)0x2A50))->setValue(identity, sizeof(identity));
  mediaHid->hidInfo(0, 0x02); // Normally Connectable, no remote wake claim
  mediaHid->reportMap(const_cast<uint8_t *>(HidMediaKeys::REPORT_MAP), sizeof(HidMediaKeys::REPORT_MAP));
  mediaInput = mediaHid->inputReport(HidMediaKeys::REPORT_ID);
  mediaInput->setCallbacks(new HidInputCB());
  const uint8_t zero[2] = {0, 0}; mediaInput->setValue(zero, sizeof(zero));
  mediaHid->hidControl()->setCallbacks(new HidControlCB());
  mediaHid->protocolMode()->setCallbacks(new HidProtocolCB());
  mediaHid->setBatteryLevel(100); // updated from the existing fuel gauge in the loop
  mediaHid->startServices();
}

class ServerCB : public BLEServerCallbacks {
  void onConnect(BLEServer *s) {
    bleConnected = true; ++mediaConnectionEpoch;
    bleAuthed = false;
    hidSubscribed = false; hidSubscriber = 0xFFFF; hidSuspended = false; hidReportMode = true; ++hidSessionEpoch;
    bleConnAt = millis();
    authNotifies = 0;
  }
#if defined(CONFIG_NIMBLE_ENABLED)
  void onConnect(BLEServer *s, ble_gap_conn_desc *desc) { if (desc) bleConnHandle = desc->conn_handle; }
#endif
  void onDisconnect(BLEServer *s) {
    bleConnected = false; ++mediaConnectionEpoch;
    bleAuthed = false;
    bleConnHandle = 0xFFFF;
    hidSubscribed = false; hidSubscriber = 0xFFFF; ++hidSessionEpoch;
    if (!powerOffPending) BLEDevice::startAdvertising();
  }
};

// v2.6 保险：屏幕相关设置在蓝牙线程里直接处理（即使主循环被卡住也能改回来）
void saveAndRestart(const char *key, int val, bool isBool) {
  Preferences pr;
  pr.begin("motonav", false);
  if (isBool) pr.putBool(key, val != 0); else pr.putInt(key, val);
  pr.end();
  delay(100);
  ESP.restart();
}

class RxCB : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *c) {
    if (powerOffPending) return;
    size_t n = c->getLength();
    if (n == 0) return;
    const uint8_t *d0 = c->getData();
    if (n >= 4 && n < 12 && d0[0] == '#' && d0[2] == ',' && (d0[1] == 'f' || d0[1] == 'q')) {
      char num[12];
      size_t ln = n - 3;
      memcpy(num, d0 + 3, ln);
      num[ln] = 0;
      int v = atoi(num);
      if (d0[1] == 'f' && (v != 0) != fastFlushOn) saveAndRestart("fastf", v, true);
      if (d0[1] == 'q' && (v == 40 || v == 60 || v == 80) && v != qspiMHz) saveAndRestart("qspi", v, false);
      return;
    }
    if (n > RXQ_MAX) n = RXQ_MAX;
    portENTER_CRITICAL(&mux);
    int next = (rxHead + 1) % RXQ_SLOTS;
    if (next == rxTail) rxTail = (rxTail + 1) % RXQ_SLOTS;   // 队列满：丢掉最旧的一条
    rxq[rxHead].len = n;
    memcpy(rxq[rxHead].data, c->getData(), n);
    rxHead = next;
    portEXIT_CRITICAL(&mux);
  }
};
