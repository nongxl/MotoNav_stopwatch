// MotoNav 1.75：程序入口与运行调度。
// v7.12.9-junction.10：保留独立卡片框架；缺少右转数据改为灰色箭头和灰色实心圆。
// 文件职责、依赖和验证结果见工程目录的“源码结构说明.txt”。
// 实现头文件只由此入口包含，保持原编译顺序和内联优化。
#include "src/core/FirmwareConfig.h"
#include <Arduino.h>
#include "src/core/DeviceContext.h"
#include "src/core/FunctionDeclarations.h"
#include "src/connectivity/Bluetooth.h"
#include "src/display/DisplayPipeline.h"
#include "src/graphics/VectorDrawing.h"
#include "src/graphics/ProgressRings.h"
#include "src/power/Battery.h"
#include "src/graphics/StatusWidgets.h"
#include "src/graphics/LaneDrawing.h"
#include "src/graphics/TextBitmaps.h"
#include "src/protocol/TextBitmapReceiver.h"
#include "src/pages/HomePage.h"
#include "src/pages/PairingPage.h"
#include "src/pages/ArrivalAnimation.h"
#include "src/pages/IncomingCall.h"
#include "src/pages/WeatherPage.h"
#include "src/pages/PageNavigation.h"
#include "src/map/MapState.h"
#include "src/map/VectorMapReceiver.h"
#include "src/map/RasterDecoder.h"
#include "src/display/RowWorker.h"
#include "src/map/RasterRenderer.h"
#include "src/map/RouteRenderer.h"
#include "src/protocol/BulkReceiver.h"
#include "src/power/BatteryPolling.h"
#include "src/connectivity/Notifications.h"
#include "src/map/MapPrimitives.h"
#include "src/map/MapHud.h"
#include "src/map/CameraOverlay.h"
#include "src/map/NavigationMarkers.h"
#include "src/map/RadarEffects.h"
#include "src/map/MapFrame.h"
#include "src/pages/NavigationPage.h"
#include "src/protocol/CommandReceiver.h"
#include "src/connectivity/DeviceName.h"
#include "src/diagnostics/RenderBenchmarks.h"

// 初始化：硬件、画布、持久化设置及蓝牙服务。
void setup() {
  makeBleName();                                       // v7.0 先算好蓝牙名（首页要显示）
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);   // 没打开串口监视器时打印不等待（v1.2 的修复）
  Serial.printf("Reset reason=%d, sleep wake=%d\n", (int)esp_reset_reason(),
                (int)esp_sleep_get_wakeup_cause());
  Wire.begin(IIC_SDA, IIC_SCL);
  touchInit();                                         // v5.2 触摸屏
  batInit();                                           // v6.8 电源芯片（电量）

  // v2.3 读取保存的屏幕数据线速度（默认 40MHz，和官方示例一样）
  {
    Preferences pr;
    pr.begin("motonav", true);
    qspiMHz = pr.getInt("qspi", 40);
    screenRot = pr.getInt("rot", 0) & 3;                            // v6.2 圆屏方向
    setScreenDeg(pr.getInt("rotd", screenRot * 90), false);         // v6.4 任意角度
    bootBrand = BootAnimation::valid(pr.getInt("bootbrand", 0));
    hidMusicMode = pr.getBool("mediahid", false);
    fastFlushOn = pr.getBool("fastf", true);
    pr.end();
    if (qspiMHz != 40 && qspiMHz != 60 && qspiMHz != 80) qspiMHz = 40;
  }
  if (!gfx->begin(qspiMHz * 1000000)) Serial.println("gfx->begin() failed!");
  panel->setBrightness(200);

  iconBuf = (uint16_t *)ps_malloc(ICON_BUF_W * ICON_BUF_H * 2);   // v1.8.1 图标缓存
  buildRingLut();                                                   // v1.8.3 抗锯齿圆环查找表
  mapInit();                                                        // v2.0 小地图缓冲
  for (int i = 0; i < IMG_SLOTS; i++) {
    imgs[i].show = (uint8_t *)ps_malloc(IMG_MAX);
    imgs[i].recv = (uint8_t *)ps_malloc(IMG_MAX);
    imgs[i].showW = 0; imgs[i].showH = 0; imgs[i].recvId = -1;
  }

  // Begin with black instead of briefly displaying the waiting page before the intro.
  gfx->fillScreen(0);
  gfx->flush();
  initRotSpan();                                                    // v6.4 任意角度用
  initDoubleBuffer();                                               // v2.2 双缓冲 + 核心 0 刷屏
  helperInit();                                                     // v5.7 另一个核心帮忙画底图

  if (bootBrand != BootAnimation::Off && gfx->getFramebuffer()) {
    uint32_t bootAt = millis();
    while (millis() - bootAt < BootAnimation::Duration) {
      uint32_t frameAt = millis();
      BootAnimation::render(gfx->getFramebuffer(), frameAt - bootAt, bootBrand);
      flushSync();
      uint32_t spent = millis() - frameAt;
      delay(spent < 33 ? 33 - spent : 1);
    }
  }
  drawWaiting(WAIT_NOLINK);
  flushSync();
  needFlush = false;

#if FPS_SELFTEST
  runFpsSelfTest();
#endif
  BLEDevice::init(bleName);                    // v7.0 名字带地址后 4 位；名字放不进广播包时库会自动放到扫描应答里
  BLEDevice::setMTU(517);     // 允许手机协商大数据包，用来传文字图片
#if defined(CONFIG_NIMBLE_ENABLED)
  // v4.5 允许蓝牙 2M 模式（速度翻倍，手机请求时直接同意）和最长空中数据包（251 字节）
  ble_gap_set_prefered_default_le_phy(BLE_GAP_LE_PHY_1M_MASK | BLE_GAP_LE_PHY_2M_MASK,
                                      BLE_GAP_LE_PHY_1M_MASK | BLE_GAP_LE_PHY_2M_MASK);
  ble_gap_write_sugg_def_data_len(251, 2120);
#endif
  // v6.5 配对验证：圆屏显示 6 位随机配对码（每次配对都换），手机输入；要绑定 + 防中间人 + 安全连接
  {
    BLESecurity *sec = new BLESecurity();                 // 构造时设好“交换加密密钥和身份密钥”（手机换地址也认得出）
    sec->setCapability(ESP_IO_CAP_OUT);                   // 圆屏只能显示，不能输入
    sec->setAuthenticationMode(true, true, true);         // 绑定、防中间人、安全连接
    sec->setPassKey(false);                               // 随机配对码
    sec->regenPassKeyOnConnect(true);                     // 每次配对重新随机
    BLEDevice::setSecurityCallbacks(new SecCB());
  }
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCB());
  BLEService *svc = server->createService(SERVICE_UUID);
  BLECharacteristic *rx = svc->createCharacteristic(
    RX_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
#if defined(CONFIG_NIMBLE_ENABLED)
             | BLECharacteristic::PROPERTY_WRITE_ENC | BLECharacteristic::PROPERTY_WRITE_AUTHEN   // v6.5 必须配对过才能写
#endif
  );
  rx->setCallbacks(new RxCB());
  txChar = svc->createCharacteristic(TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);   // v2.0
  txChar->addDescriptor(new BLE2902());
  svc->start();
  initMediaHid(server);
  appleMedia.begin();
  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->addServiceUUID(BLEUUID((uint16_t)0x1812));
  adv->setAppearance(GENERIC_HID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();
  idlePowerOffReset(&idlePowerOffTimer, millis());
  Serial.println(String("MotoNav ready, firmware ") + FW_VERSION);
}

#include "src/power/Shutdown.h"
#include "src/graphics/JunctionOverlay.h"

// 主循环：接收消息、处理输入、调度页面、绘图和推屏。
void loop() {
  hidMediaTick();                                      // press/release without blocking navigation
  static uint32_t lyricConnectionEpoch = 0;
  if (lyricConnectionEpoch != mediaConnectionEpoch) {
    lyricConnectionEpoch = mediaConnectionEpoch; musicLyrics = false; lyricImageGeneration = 0;
    imgs[12].showW = 0; imgs[12].showH = 0; imgs[12].recvId = -1; musicDirty = true;
  }
  appleMedia.tick(bleConnected && bleAuthed && hidMusicMode, bleConnHandle, mediaConnectionEpoch, millis());
  static int lastControlMask = -1;
  int controlMask = appleMedia.controlMask();
  if (lastControlMask != controlMask) { lastControlMask = controlMask; pendingMediaStatus = String("AMC,") + controlMask; musicDirty = true; Serial.printf("MEDIA AMS controls=%d\n", controlMask); }
  if (appleMedia.controlAck) { musicAck = appleMedia.controlAck > 0 ? 9 : 10; appleMedia.controlAck = 0; musicAckUntil = millis() + 2500; musicDirty = true; }
  if (appleMedia.changed) {
    appleMedia.changed = false; musicDirty = true;
    static uint32_t lyricGeneration = 0; static uint16_t lyricTitleRevision = 0, lyricArtistRevision = 0;
    if (lyricGeneration != appleMedia.generation || lyricTitleRevision != appleMedia.values[0].revision || lyricArtistRevision != appleMedia.values[1].revision) {
      lyricImageGeneration = 0;
      lyricGeneration = appleMedia.generation; lyricTitleRevision = appleMedia.values[0].revision; lyricArtistRevision = appleMedia.values[1].revision;
      imgs[12].showW = 0; imgs[12].showH = 0; imgs[12].recvId = -1;
    }
    if (hidMusicMode) { musicHave = appleMedia.available && appleMedia.values[0].text.length(); musicPlaying = appleMedia.playback.known && appleMedia.playback.state == 1; }
  }
  static unsigned long mediaDrawAt = 0;
  if (hidMusicMode && appleMedia.available && millis() - mediaDrawAt >= 1000) { mediaDrawAt = millis(); musicDirty = true; }
  saveRotIfDue();                                      // v6.4
  // v6.5 连上 60 秒还没通过验证（不是自己的手机）：断开，让自己的手机能连
#if defined(CONFIG_NIMBLE_ENABLED)
  if (bleConnected && !bleAuthed && bleConnHandle != 0xFFFF && millis() - bleConnAt > AUTH_TIMEOUT_MS) {
    ble_gap_terminate(bleConnHandle, BLE_ERR_REM_USER_CONN_TERM);
    bleConnAt = millis();                              // 别每圈都断
  }
#endif
  // 1. 连接状态变化（先处理）；v6.5 通过验证才算连上
  bool conn = bleConnected && bleAuthed;
  if (conn != shownConn) {
    shownConn = conn;
    if (conn) {
      lastRxMs = millis();
      drawWaiting(WAIT_READY);
    } else {
      trafficLightFrame.clear();
      trafficLightLab.clear();
      bool wasNav = inNav;
      resetNavState();
      staleShown = false;
      drawWaiting(wasNav ? WAIT_LOST : WAIT_NOLINK);   // 导航途中断开 → 红色 LINK LOST
    }
  }

  // 2. 取出队列里的所有消息逐条处理（最多 30 条，避免卡住画面）
  static RxMsg local;
  for (int i = 0; i < 30; i++) {
    bool got = false;
    portENTER_CRITICAL(&mux);
    if (rxTail != rxHead) {
      local.len = rxq[rxTail].len;
      memcpy(local.data, rxq[rxTail].data, local.len);
      rxTail = (rxTail + 1) % RXQ_SLOTS;
      got = true;
    }
    portEXIT_CRITICAL(&mux);
    if (!got) break;
    handleMessage(local.data, local.len);
  }

  updateStats();
  if (pendingStat.length()) { pendingStat2 = pendingStat; pendingStat = ""; }
  sendPendingAck();                                    // v2.0 地图收齐的确认 / v2.2 帧率

  // 3. 进度环动画；小地图模式下连续刷新（v2.2 不再限制帧间隔）
  animateRing();
  // 地图帧原本连续刷新；大箭头画面也按现有帧节奏推进行程环。
  static unsigned long tripDrawAt=0;
  static float tripDrawValue=-1;
  float tripValue=tripFrac();
  if(inNav && progOn && tripOn && !cruiseOn && !ringActive && !staleShown &&
     tripValue>=0 && tripValue!=tripDrawValue && millis()-tripDrawAt>=33) {
    tripDrawAt=millis();tripDrawValue=tripValue;navDirty=true;
  }
  if(tripValue<0)tripDrawValue=-1;
  static bool wasMap = false;
  bool mapNow = inNav && mapModeActive();
  if (mapNow != wasMap) { wasMap = mapNow; navDirty = true; }   // 进出地图模式时整帧重画
  if (mapNow) navDirty = true;

  touchPoll();                                         // v5.2 按住接听 / v5.3 滑动换页
  batPoll();                                           // v6.8 电量
  // 4. 需要时重画导航画面
  // v6.5 配对：整屏显示配对码，配好 / 失败 / 60 秒 / 断开后回到原来的画面
  if (pairReq) {
    pairReq = false;
    if (sliding) dragAbort();
    pairOn = true; pairShowAt = millis();
    drawPairScreen();
  }
  if (pairOn && (pairDone || millis() - pairShowAt > AUTH_TIMEOUT_MS || !bleConnected)) {
    pairOn = false; pairDone = false;
    if (page == 2) musicDirty = true;
    else if (page == 1) wxDirty = true;
    else if (inNav) navDirty = true;
    else drawWaiting(conn ? WAIT_READY : WAIT_NOLINK);
  }
  // v5.1 来电：整屏显示（约 12 帧/秒的动画），结束后回到原来的画面
  static bool wasCall = false;
  if (callOn && (millis() - callSeenAt > 120000 || !conn)) callOn = false;      // 2 分钟没消息 / 断开：收起
  if (callOn && sliding) dragAbort();                  // v5.5 拖到一半来电：放弃换页
  if (callOn) {
    static unsigned long lastCallDraw = 0;
    if (millis() - lastCallDraw > 80) { lastCallDraw = millis(); drawCallScreen(); }
    wasCall = true;
  } else if (wasCall) {
    wasCall = false;
    if (page == 2) musicDirty = true;
    else if (page == 1) wxDirty = true;
    else if (inNav) navDirty = true;
    else drawWaiting(conn ? WAIT_READY : WAIT_NOLINK);
  }

  // v7.1 到达特效：来电、配对优先；放完 / 被点掉后回到原来的画面
  static bool wasFx = false;
  if (fxOn && (millis() - fxAt > fxDur || !conn)) fxOn = false;
  if (fxOn) {
    static unsigned long lastFxDraw = 0;
    if (!callOn && !pairOn && page == 0 && !sliding && millis() - lastFxDraw >= 33) {
      lastFxDraw = millis();
      drawArriveFx(millis() - fxAt);
    }
    wasFx = true;
  } else if (wasFx) {
    wasFx = false;
    if (!callOn && !pairOn) {
      if (page == 2) musicDirty = true;
      else if (page == 1) wxDirty = true;
      else if (inNav) navDirty = true;
      else drawWaiting(conn ? WAIT_READY : WAIT_NOLINK);
    }
  }

  musicTick();
  weatherTick();                                       // v5.3 天气页
  if (inNav && navDirty && !fxOn && !callOn && !pairOn && page == 0 && !sliding) {
    drawNavFrame();
    navDirty = false;
  }

  // 5. 导航中长时间收不到任何消息 → 数据中断提示
  if (inNav && conn && !fxOn && !callOn && !pairOn && page == 0 && !sliding && !staleShown && millis() - lastRxMs > STALE_MS) {
    staleShown = true;
    drawStaleOverlay();
  }

  // Compose over navigation, never replace the screen or alter route/arrival state.
  const uint32_t lightNow=millis();
  if (!conn || !trafficLightLab.valid(lightNow)) trafficLightLab.clear();
  const bool lightPreview=trafficLightPreviewUntil && (int32_t)(trafficLightPreviewUntil-lightNow)>0;
  if(!lightPreview)trafficLightPreviewUntil=0;
  const bool showLight=(trafficLightLab.valid(lightNow) || trafficLightFrame.valid(lightNow) || lightPreview) && conn && page==0 && !sliding && !pairOn && !callOn && !fxOn && !staleShown;
  static bool lightShown=false;
  static uint32_t lightDrawAt=0;
  animateCat();
  if(showLight) {
    if(!lightShown || needFlush || lightNow-lightDrawAt>=200) {
      drawTrafficLightLab(lightNow);lightDrawAt=lightNow;
    }
  } else if(lightShown && !pairOn && !callOn && !fxOn && !sliding) {
    if(page==2)musicDirty=true;
    else if(page==1)wxDirty=true;
    else if(inNav)navDirty=true;
    else drawWaiting(conn ? WAIT_READY : WAIT_NOLINK);
  }
  if(lightShown!=showLight)Serial.printf("TL OVERLAY visible=%d inNav=%d at=%lu valid=%d conn=%d page=%d sliding=%d pair=%d call=%d fx=%d stale=%d\n",
    showLight,inNav,lightNow,trafficLightLab.valid(lightNow),conn,page,sliding,pairOn,callOn,fxOn,staleShown);
  lightShown=showLight;

  // 6. 有变化才把画布推到屏幕（小地图帧异步推送，其他画面同步推送）
  if (needFlush && !sliding) {
    if (frameIsMap) flushAsync(); else flushSync();
    needFlush = false;
    frameIsMap = false;
  }
  idlePowerPollSupply();
  // Idle phone traffic does not count as navigation; external power inhibits off.
  const bool navigating = idlePowerOffNavigationActive(
      bleConnected && bleAuthed, inNav, cruiseOn, navDir == "A");
  const bool interacting = pairOn || pairReq || callOn || touchDown || dragging || sliding;
  if (idlePowerOffDueOnBattery(&idlePowerOffTimer, millis(), navigating, interacting, &idlePowerSupply)) {
    bool queued;
    portENTER_CRITICAL(&mux);
    queued = rxTail != rxHead;
    portEXIT_CRITICAL(&mux);
    if (!queued) {
      // Never decide from the 5-second battery UI cache. Re-read at the deadline.
      idlePowerPollSupply(true);
      if (idlePowerOffDueOnBattery(&idlePowerOffTimer, millis(), navigating, interacting, &idlePowerSupply))
        idlePowerOffShutdown();
    }
  }
  delay(mapNow ? 1 : 10);
}
