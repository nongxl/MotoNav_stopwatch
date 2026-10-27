#pragma once
#include <Arduino.h>

// v7.8 音乐页：歌名/歌手图片栏 5/6；#Z,有会话,播放中,按钮位掩码。
void drawMusic() {
  gfx->fillScreen(C_BLACK);
  onWaitScreen = false;
  frameIsMap = false;
  printCentered("MUSIC", CX, 65, 4, C_CYAN);
  bool linked = bleConnected && bleAuthed;
  if (hidMusicMode && linked && !appleMedia.available) {
    printCentered("PHONE MEDIA KEYS", CX, 158, 2, C_WHITE);
    printCentered("OPEN MUSIC APP ON PHONE", CX, 210, 2, C_GREY);
  } else if (linked) {
    drawTextImg(5, 158, 0xFF);
    drawTextImg(6, 210, 0xBB);
  } else printCentered("PHONE OFFLINE", CX, 170, 3, C_GREY);
  bool showLyrics = linked && hidMusicMode && musicLyrics && appleMedia.available && imgs[12].showW > 0;
  if (showLyrics) drawTextImg(12, 242, 0xDD);
  const char *feedback[] = {"", "SENT", "PHONE RECEIVED", "DONE", "NO RESPONSE", "NO PLAYER", "HID SENT", "HID NOT READY", "HID SEND FAILED", "AMS SENT", "AMS SEND FAILED", "AMS QUEUED"};
  String state = linked && musicHave ? (musicPlaying ? "PLAYING" : "PAUSED") : "NO PLAYER";
  if (hidMusicMode) {
    state = !hidMediaReady() ? (appleMedia.controlMask() ? "AMS MEDIA READY" : "PAIR HID IN PHONE SETTINGS") : !appleMedia.available ? "HID READY / AMS UNAVAILABLE"
      : !appleMedia.playback.known ? "STATE UNKNOWN" : appleMedia.playback.state == 1 ? "PLAYING" : appleMedia.playback.state == 0 ? "PAUSED" : "SEEKING";
  }
  if (musicAck > 0 && musicAck <= 11) state = feedback[musicAck];
  printCentered(state, CX, showLyrics ? 272 : 257, 2, musicAck == 4 ? C_ORANGE : C_GREY);
  for (int i = 0; i < 3; i++) {
    int x = CX + (i - 1) * 100, y = 335;
    bool enabled = hidMusicMode ? (hidMediaReady() || appleMedia.controlReady(i)) : linked && musicHave;
    uint16_t c = enabled ? C_WHITE : C_GREY;
    aaDisc(x, y, 43, enabled && musicPressed == i ? 0x0470 : enabled ? 0x19A7 : 0x1082);
    if (i == 1) {
      if (hidMusicMode && !appleMedia.playback.known) {
        gfx->fillTriangle(x - 19, y - 15, x - 19, y + 15, x + 1, y, c);
        gfx->fillRect(x + 7, y - 15, 5, 30, c); gfx->fillRect(x + 16, y - 15, 5, 30, c);
      } else if (musicPlaying) {
        gfx->fillRect(x - 13, y - 15, 8, 30, c);
        gfx->fillRect(x + 5, y - 15, 8, 30, c);
      } else gfx->fillTriangle(x - 10, y - 17, x - 10, y + 17, x + 17, y, c);
    } else {
      int sign = i == 0 ? -1 : 1;
      gfx->fillTriangle(x - sign * 12, y - 15, x - sign * 12, y + 15, x + sign * 10, y, c);
      gfx->fillRect(x + sign * 15 - 3, y - 15, 6, 30, c);
    }
  }
  if (hidMusicMode && appleMedia.available) {
    double duration = AppleMediaData::duration(appleMedia.values[3].text.c_str());
    if (duration > 0 && appleMedia.playback.known) {
      double elapsed = appleMedia.playback.elapsed + appleMedia.playback.rate * (millis() - appleMedia.playbackAt) / 1000.0;
      int sec = (int)constrain(elapsed, 0.0, duration), total = (int)duration;
      char time[32]; snprintf(time, sizeof(time), "%d:%02d / %d:%02d", sec/60, sec%60, total/60, total%60);
      printCentered(time, CX, 392, 2, C_GREY);
    }
  } else printCentered("SWIPE LEFT TO RETURN", CX, 402, 2, C_GREY);
  drawPageDots();
  needFlush = true;
}

