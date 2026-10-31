#pragma once
#include <Arduino.h>

extern uint16_t *coverBuf;
extern bool coverValid;
extern uint32_t musicDuration;
extern uint32_t musicPos;
extern uint32_t musicPosAt;

static inline void formatMusicTime(uint32_t sec, char *buf, size_t len) {
  uint32_t m = sec / 60;
  uint32_t s = sec % 60;
  snprintf(buf, len, "%02lu:%02lu", (unsigned long)m, (unsigned long)s);
}

// 音乐页：支持彩色大专辑封面(180x180)、黑胶唱盘降级、歌曲进度条与时长、歌名/歌手与大播控按钮
void drawMusic() {
  gfx->fillScreen(C_BLACK);
  onWaitScreen = false;
  frameIsMap = false;

  printCentered("MUSIC", CX, 22, 2, C_CYAN);
  bool linked = bleConnected && bleAuthed;

  // 1. 专辑封面区 (Y: 38 ~ 218，大小 180x180)
  const int covSize = 180;
  const int covX = CX - covSize / 2; // 116
  const int covY = 38;

  if (linked && coverValid && coverBuf != nullptr) {
    gfx->fillRoundRect(covX - 3, covY - 3, covSize + 6, covSize + 6, 12, 0x18C3);
    gfx->drawRoundRect(covX - 3, covY - 3, covSize + 6, covSize + 6, 12, 0x4A69);
    gfx->draw16bitRGBBitmap(covX, covY, coverBuf, covSize, covSize);
  } else {
    // 拟物黑胶唱片（按 180 比例放大）
    const int discR = 86;
    const int discY = covY + covSize / 2;
    aaDisc(CX, discY, discR, 0x10A2);
    gfx->drawCircle(CX, discY, 68, 0x2124);
    gfx->drawCircle(CX, discY, 48, 0x2124);
    aaDisc(CX, discY, 32, 0x249F);
    aaDisc(CX, discY, 8, C_BLACK);
    gfx->drawCircle(CX, discY, 8, 0xDEFB);
  }

  // 2. 封面下方：歌曲进度条 (Y: 228 ~ 238)
  if (linked && musicHave && musicDuration > 0) {
    uint32_t curSec = musicPos;
    if (musicPlaying) {
      uint32_t elapsed = (millis() - musicPosAt) / 1000;
      curSec = min(musicDuration, musicPos + elapsed);
    }
    const int barW = 200;
    const int barH = 4;
    const int barX = CX - barW / 2; // 106
    const int barY = 228;

    // 进度条背景
    gfx->fillRoundRect(barX, barY, barW, barH, 2, 0x2124);

    // 进度指示
    float progress = constrain((float)curSec / (float)musicDuration, 0.0f, 1.0f);
    int fillW = max(4, (int)(barW * progress));
    gfx->fillRoundRect(barX, barY, fillW, barH, 2, 0x07FF); // C_CYAN
    gfx->fillCircle(barX + fillW, barY + barH / 2, 3, C_WHITE); // 进度小白点

    // 左右两侧时间文字 (当前时间 / 总时长)
    char timeCur[16], timeTotal[16];
    formatMusicTime(curSec, timeCur, sizeof(timeCur));
    formatMusicTime(musicDuration, timeTotal, sizeof(timeTotal));
    gfx->setTextColor(0x9CD3, C_BLACK);
    gfx->setTextSize(1);
    gfx->setCursor(barX - 36, barY - 2);
    gfx->print(timeCur);
    gfx->setCursor(barX + barW + 8, barY - 2);
    gfx->print(timeTotal);
  }

  // 3. 歌名与歌手 (Y: 250, 282)
  if (hidMusicMode && linked && !appleMedia.available) {
    printCentered("PHONE MEDIA KEYS", CX, 252, 2, C_WHITE);
    printCentered("OPEN MUSIC APP ON PHONE", CX, 284, 2, C_GREY);
  } else if (linked) {
    drawTextImg(5, 252, 0xFF);
    drawTextImg(6, 284, 0xBB);
  } else {
    printCentered("PHONE OFFLINE", CX, 260, 2, C_GREY);
  }

  // 4. 底部播放控制触控按键 (Y: 345, 半径 40)
  for (int i = 0; i < 3; i++) {
    int x = CX + (i - 1) * 96, y = 345;
    bool enabled = hidMusicMode ? (hidMediaReady() || appleMedia.controlReady(i)) : linked && musicHave;
    uint16_t c = enabled ? C_WHITE : C_GREY;
    aaDisc(x, y, 40, enabled && musicPressed == i ? 0x0470 : enabled ? 0x19A7 : 0x1082);
    if (i == 1) {
      if (hidMusicMode && !appleMedia.playback.known) {
        gfx->fillTriangle(x - 17, y - 13, x - 17, y + 13, x + 1, y, c);
        gfx->fillRect(x + 6, y - 13, 4, 26, c); gfx->fillRect(x + 14, y - 13, 4, 26, c);
      } else if (musicPlaying) {
        gfx->fillRect(x - 11, y - 13, 7, 26, c);
        gfx->fillRect(x + 4, y - 13, 7, 26, c);
      } else gfx->fillTriangle(x - 8, y - 15, x - 8, y + 15, x + 15, y, c);
    } else {
      int sign = i == 0 ? -1 : 1;
      gfx->fillTriangle(x - sign * 10, y - 13, x - sign * 10, y + 13, x + sign * 9, y, c);
      gfx->fillRect(x + sign * 13 - 3, y - 13, 5, 26, c);
    }
  }

  // 5. 底部页面点指示
  drawPageDots();
  needFlush = true;
}
