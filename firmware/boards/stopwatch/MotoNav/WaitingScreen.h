#pragma once
#include <Arduino.h>
#include "AvatarEngine.h"

// ================= v4.6 首页像素骑手（代替 v3.6 的猫猫） =================
// 原创的像素女骑手 + 小摩托：连上手机时围巾和马尾随风飘、偶尔眨眼、每隔几秒挥挥手；
// 没连上时摩托车灯熄灭、她闭眼打瞌睡，头上飘 z
// v4.6 首页像素骑手（原创角色）：40×26，每格放大 RIDER_S 倍；由 rider2.py 生成
#define RIDER_W 40
#define RIDER_H 26
#define RIDER_FRAMES 7
// 帧：stand0、stand1、blink0、blink1、wave0、wave1、sleep
const char *const RIDER[RIDER_FRAMES][RIDER_H] = {
  { // stand0
    "........................................",
    "........KKKKK...........................",
    "......KKHHHHHKK.........................",
    ".....KHhhHHHHHHK........................",
    "....KHhHHHHHHHHHK.......................",
    "....KHVVVVVVVVVHK.......................",
    "....KVvvVVVVVVVVK.......................",
    "....KHKKKKKKKKKHK.......................",
    "....KRRRSSRSSRRRKK......................",
    "...KRRSSEESEESSRRKK.....................",
    "...KRRSSEESEESSRRKRK....................",
    "...KRRSBSSSSSBSRRKRK....................",
    "....KRRSSSKSSSRRK.RK.................KK.",
    ".....KRKKSSSKKRK..K.................KcK.",
    ".....KJJJFFFJFFK........KKKKKKK.....KcK.",
    "....KJJJJJFJJKFFK......KsssssssKKKKKMMK.",
    "...KJJKJJJFJJJKKJK.....KMMMMMMMMMMMMMMKK",
    "...KJjKJJJJJJJKjJK....KMMWMMMMMMMMMMMLLK",
    "...KGGKJJJJJJJKGGK....KMMMMMmmmmMMMMMLLK",
    "....KK.KPPPPPPK.KK....KmmMMmmmmmmmMMMMKK",
    ".......KPPPPPPK........KTTKcCCCCCcKKTTK.",
    ".......KPPKKPPK.......KTttTKcCKKCcKTttTK",
    ".......KPpKKpPK.......KtWWtKKccccKKtWWtK",
    ".......KPPKKPPK.......KtWWtK......KtWWtK",
    "......KbbbKKbbbK......KTttTK......KTttTK",
    "......KKKKKKKKKK.......KTTK........KTTK.",
  },
  { // stand1
    "........................................",
    "........KKKKK...........................",
    "......KKHHHHHKK.........................",
    ".....KHhhHHHHHHK........................",
    "....KHhHHHHHHHHHK.......................",
    "....KHVVVVVVVVVHK.......................",
    "....KVvvVVVVVVVVK.......................",
    "....KHKKKKKKKKKHK.......................",
    "....KRRRSSRSSRRRKKK.....................",
    "...KRRSSEESEESSRRKRK....................",
    "...KRRSSEESEESSRRKRRK...................",
    "...KRRSBSSSSSBSRRKRRK...................",
    "....KRRSSSKSSSRRK..KK................KK.",
    ".....KRKKSSSKKRK....................KcK.",
    ".....KJJJFFFJFFFK.......KKKKKKK.....KcK.",
    "....KJJJJJFJJKFFFK.....KsssssssKKKKKMMK.",
    "...KJJKJJJFJJJKKKK.....KMMMMMMMMMMMMMMKK",
    "...KJjKJJJJJJJKjJK....KMMWMMMMMMMMMMMLLK",
    "...KGGKJJJJJJJKGGK....KMMMMMmmmmMMMMMLLK",
    "....KK.KPPPPPPK.KK....KmmMMmmmmmmmMMMMKK",
    ".......KPPPPPPK........KTTKcCCCCCcKKTTK.",
    ".......KPPKKPPK.......KTttTKcCKKCcKTttTK",
    ".......KPpKKpPK.......KtWWtKKccccKKtWWtK",
    ".......KPPKKPPK.......KtWWtK......KtWWtK",
    "......KbbbKKbbbK......KTttTK......KTttTK",
    "......KKKKKKKKKK.......KTTK........KTTK.",
  },
  { // blink0
    "........................................",
    "........KKKKK...........................",
    "......KKHHHHHKK.........................",
    ".....KHhhHHHHHHK........................",
    "....KHhHHHHHHHHHK.......................",
    "....KHVVVVVVVVVHK.......................",
    "....KVvvVVVVVVVVK.......................",
    "....KHKKKKKKKKKHK.......................",
    "....KRRRSSRSSRRRKK......................",
    "...KRRSSSSSSSSSRRKK.....................",
    "...KRRSSKKSKKSSRRKRK....................",
    "...KRRSBSSSSSBSRRKRK....................",
    "....KRRSSSKSSSRRK.RK.................KK.",
    ".....KRKKSSSKKRK..K.................KcK.",
    ".....KJJJFFFJFFK........KKKKKKK.....KcK.",
    "....KJJJJJFJJKFFK......KsssssssKKKKKMMK.",
    "...KJJKJJJFJJJKKJK.....KMMMMMMMMMMMMMMKK",
    "...KJjKJJJJJJJKjJK....KMMWMMMMMMMMMMMLLK",
    "...KGGKJJJJJJJKGGK....KMMMMMmmmmMMMMMLLK",
    "....KK.KPPPPPPK.KK....KmmMMmmmmmmmMMMMKK",
    ".......KPPPPPPK........KTTKcCCCCCcKKTTK.",
    ".......KPPKKPPK.......KTttTKcCKKCcKTttTK",
    ".......KPpKKpPK.......KtWWtKKccccKKtWWtK",
    ".......KPPKKPPK.......KtWWtK......KtWWtK",
    "......KbbbKKbbbK......KTttTK......KTttTK",
    "......KKKKKKKKKK.......KTTK........KTTK.",
  },
  { // blink1
    "........................................",
    "........KKKKK...........................",
    "......KKHHHHHKK.........................",
    ".....KHhhHHHHHHK........................",
    "....KHhHHHHHHHHHK.......................",
    "....KHVVVVVVVVVHK.......................",
    "....KVvvVVVVVVVVK.......................",
    "....KHKKKKKKKKKHK.......................",
    "....KRRRSSRSSRRRKKK.....................",
    "...KRRSSSSSSSSSRRKRK....................",
    "...KRRSSKKSKKSSRRKRRK...................",
    "...KRRSBSSSSSBSRRKRRK...................",
    "....KRRSSSKSSSRRK..KK................KK.",
    ".....KRKKSSSKKRK....................KcK.",
    ".....KJJJFFFJFFFK.......KKKKKKK.....KcK.",
    "....KJJJJJFJJKFFFK.....KsssssssKKKKKMMK.",
    "...KJJKJJJFJJJKKKK.....KMMMMMMMMMMMMMMKK",
    "...KJjKJJJJJJJKjJK....KMMWMMMMMMMMMMMLLK",
    "...KGGKJJJJJJJKGGK....KMMMMMmmmmMMMMMLLK",
    "....KK.KPPPPPPK.KK....KmmMMmmmmmmmMMMMKK",
    ".......KPPPPPPK........KTTKcCCCCCcKKTTK.",
    ".......KPPKKPPK.......KTttTKcCKKCcKTttTK",
    ".......KPpKKpPK.......KtWWtKKccccKKtWWtK",
    ".......KPPKKPPK.......KtWWtK......KtWWtK",
    "......KbbbKKbbbK......KTttTK......KTttTK",
    "......KKKKKKKKKK.......KTTK........KTTK.",
  },
  { // wave0
    "........................................",
    "........KKKKK...........................",
    "......KKHHHHHKK.........................",
    ".....KHhhHHHHHHK........................",
    "....KHhHHHHHHHHHK.......................",
    "....KHVVVVVVVVVHK.......................",
    "....KVvvVVVVVVVVK.......................",
    "....KHKKKKKKKKKHK.......................",
    "....KRRRSSRSSRRRKK......................",
    "KK.KRRSSEESEESSRRKK.....................",
    "GGKKRRSSEESEESSRRKRK....................",
    "GGKKRRSBSSSSSBSRRKRK....................",
    "KJK.KRRSSSKSSSRRK.RK.................KK.",
    ".KJK.KRKKSSSKKRK..K.................KcK.",
    ".KJJKKJJJFFFJFFK........KKKKKKK.....KcK.",
    "..KJKJJJJJFJJKFFK......KsssssssKKKKKMMK.",
    ".....KKJJJFJJJKKJK.....KMMMMMMMMMMMMMMKK",
    ".....KKJJJJJJJKjJK....KMMWMMMMMMMMMMMLLK",
    ".....KKJJJJJJJKGGK....KMMMMMmmmmMMMMMLLK",
    ".......KPPPPPPK.KK....KmmMMmmmmmmmMMMMKK",
    ".......KPPPPPPK........KTTKcCCCCCcKKTTK.",
    ".......KPPKKPPK.......KTttTKcCKKCcKTttTK",
    ".......KPpKKpPK.......KtWWtKKccccKKtWWtK",
    ".......KPPKKPPK.......KtWWtK......KtWWtK",
    "......KbbbKKbbbK......KTttTK......KTttTK",
    "......KKKKKKKKKK.......KTTK........KTTK.",
  },
  { // wave1
    "........................................",
    "........KKKKK...........................",
    "......KKHHHHHKK.........................",
    ".....KHhhHHHHHHK........................",
    "....KHhHHHHHHHHHK.......................",
    "....KHVVVVVVVVVHK.......................",
    "....KVvvVVVVVVVVK.......................",
    "....KHKKKKKKKKKHK.......................",
    "....KRRRSSRSSRRRKKK.....................",
    ".KKKRRSSEESEESSRRKRK....................",
    "KGGKRRSSEESEESSRRKRRK...................",
    "KGGKRRSBSSSSSBSRRKRRK...................",
    ".KJKKRRSSSKSSSRRK..KK................KK.",
    ".KJK.KRKKSSSKKRK....................KcK.",
    ".KJJKKJJJFFFJFFFK.......KKKKKKK.....KcK.",
    "..KJKJJJJJFJJKFFFK.....KsssssssKKKKKMMK.",
    ".....KKJJJFJJJKKKK.....KMMMMMMMMMMMMMMKK",
    ".....KKJJJJJJJKjJK....KMMWMMMMMMMMMMMLLK",
    ".....KKJJJJJJJKGGK....KMMMMMmmmmMMMMMLLK",
    ".......KPPPPPPK.KK....KmmMMmmmmmmmMMMMKK",
    ".......KPPPPPPK........KTTKcCCCCCcKKTTK.",
    ".......KPPKKPPK.......KTttTKcCKKCcKTttTK",
    ".......KPpKKpPK.......KtWWtKKccccKKtWWtK",
    ".......KPPKKPPK.......KtWWtK......KtWWtK",
    "......KbbbKKbbbK......KTttTK......KTttTK",
    "......KKKKKKKKKK.......KTTK........KTTK.",
  },
  { // sleep
    "........................................",
    "........KKKKK...........................",
    "......KKHHHHHKK.........................",
    ".....KHhhHHHHHHK........................",
    "....KHhHHHHHHHHHK.......................",
    "....KHVVVVVVVVVHK.......................",
    "....KVvvVVVVVVVVK.......................",
    "....KHKKKKKKKKKHK.......................",
    "....KRRRSSRSSRRRKK......................",
    "...KRRSSSSSSSSSRRKK.....................",
    "...KRRSSKKSKKSSRRKRK....................",
    "...KRRSBSSSSSBSRRKRK....................",
    "....KRRSSSSSSSRRK.RK.................KK.",
    ".....KRKKSSSKKRK..K.................KcK.",
    ".....KJJJFFFJJJK........KKKKKKK.....KcK.",
    "....KJJJJJFJJJJJK......KsssssssKKKKKMMK.",
    "...KJJKJJJFJJJKJJK.....KMMMMMMMMMMMMMMKK",
    "...KJjKJJJJJJJKjJK....KMMWMMMMMMMMMMMllK",
    "...KGGKJJJJJJJKGGK....KMMMMMmmmmMMMMMllK",
    "....KK.KPPPPPPK.KK....KmmMMmmmmmmmMMMMKK",
    ".......KPPPPPPK........KTTKcCCCCCcKKTTK.",
    ".......KPPKKPPK.......KTttTKcCKKCcKTttTK",
    ".......KPpKKpPK.......KtWWtKKccccKKtWWtK",
    ".......KPPKKPPK.......KtWWtK......KtWWtK",
    "......KbbbKKbbbK......KTttTK......KTttTK",
    "......KKKKKKKKKK.......KTTK........KTTK.",
  },
};
uint16_t riderColor(char ch) {
  switch (ch) {
    case 'K': return 0x18C6;
    case 'H': return 0x2E57;
    case 'h': return 0xAFBD;
    case 'V': return 0x29ED;
    case 'v': return 0x7D1C;
    case 'S': return 0xFEB5;
    case 'E': return 0x18C6;
    case 'B': return 0xFCB2;
    case 'R': return 0x9225;
    case 'J': return 0xFC65;
    case 'j': return 0xD2E2;
    case 'F': return 0xFF07;
    case 'G': return 0x39E8;
    case 'P': return 0x3A91;
    case 'p': return 0x29AC;
    case 'b': return 0x6A25;
    case 'W': return 0xFFFF;
    case 'M': return 0xE187;
    case 'm': return 0xA0E5;
    case 'C': return 0xCE7A;
    case 'c': return 0x8432;
    case 'T': return 0x2966;
    case 't': return 0x94B4;
    case 'L': return 0xFF8F;
    case 'l': return 0x6B6C;
    case 's': return 0x4185;
    default: return 0;
  }
}

#define RIDER_S  5
#define RIDER_X0 (CX - RIDER_W * RIDER_S / 2)
#define RIDER_Y0 60
bool onWaitScreen = false;
bool sliding = false;                       // v5.5 正在拖动/滑动换页：别的画面先别画
int waitSt = 0;
int riderPrevF = -1, riderPrevZ = -2;
bool catForce = false;                      // 首页刚重画：马上画一次

// f = 第几帧；zph = 睡觉时 z 的动画帧（-1 = 不睡）
void drawRider(int f, int zph) {
  gfx->fillRect(RIDER_X0 - 4, RIDER_Y0 - 40, RIDER_W * RIDER_S + 8, RIDER_H * RIDER_S + 42, C_BLACK);
  drawStatus();                             // v7.2 上面擦黑的那块盖住了蓝牙点和电量：马上补画（z 字母画在它上面）
  for (int y = 0; y < RIDER_H; y++) {
    const char *row = RIDER[f][y];
    for (int x = 0; row[x] && x < RIDER_W; x++) {
      uint16_t c = riderColor(row[x]);
      if (c || row[x] == 'K') gfx->fillRect(RIDER_X0 + x * RIDER_S, RIDER_Y0 + y * RIDER_S, RIDER_S, RIDER_S, c ? c : 0x18C6);
    }
  }
  if (zph >= 0) {                           // 三个 z 从她头顶往右上方飘
    for (int k = 0; k <= zph; k++)
      printCentered("z", CX + 4 + k * 13, RIDER_Y0 + 2 - k * 13, 2 + (k > 0 ? 1 : 0), k == zph ? C_CYAN : C_GREY);
  }
}

// 每 0.1 秒检查一次，样子变了才重画（函数名沿用 v3.6）
void animateCat() {
  if (!onWaitScreen || sliding) return;
  static unsigned long last = 0;
  unsigned long now = millis();
  if (!catForce && now - last < 100) return;
  catForce = false;
  last = now;
  int f, zph = -1;
  int t = (int)((now / 400) % 2);           // 围巾、马尾飘动
  if (waitSt == WAIT_NOLINK) {
    f = 6;                                  // 睡觉
    zph = (int)((now / 700) % 3);
  } else if (waitSt == WAIT_READY && (now % 5000) < 1500) {
    f = 4 + (int)((now / 250) % 2);         // 挥手
  } else if ((now % 3600) < 160) {
    f = 2 + t;                              // 眨眼
  } else {
    f = t;
  }
  if (f == riderPrevF && zph == riderPrevZ) return;
  riderPrevF = f; riderPrevZ = zph;
  drawRider(f, zph);
  needFlush = true;
}

// 页面：导航左滑天气，右滑音乐，上滑进入倾角仪表，下滑进入伴侣模式
int page = 0;                               // 0 = 导航 / 首页，1 = 天气，2 = 音乐，3 = 倾角与G值仪表，4 = 伴侣模式
unsigned long pageAt = 0;
bool leanDirty = false;
void drawLeanMeter();
void leanMeterTap(int x, int y);
void drawWeather();
void weatherTap();
void drawMusic();
void musicTap(int x, int y);

// 页面顺序对应滑动位置：音乐在左(2)，主页居中(0)，天气在右(1)，倾角仪表在下(3)，伴侣模式在上(4)
void drawPageDots() {
  if (page == 0 && inNav) return;
  const int pageOrder[] = {2, 0, 1};
  for (int i = 0; i < 3; i++) {
    aaDisc(CX + (i - 1) * 18, 436, 3.5f, page == pageOrder[i] ? C_WHITE : 0x4208);
  }
  // 底部第4点：倾角与G值仪表在下方(3)，直观提示用户下方还有一个页面
  aaDisc(CX, 451, 3.5f, page == 3 ? C_WHITE : 0x4208);
  // 顶部第5点：伴侣模式在上方(4)，移至最顶部边缘避免与居中电池图标重叠
  aaDisc(CX, 10, 3.5f, page == 4 ? C_WHITE : 0x4208);
}

void drawWaiting(int st) {
  waitSt = st;
  if (sliding) return;                                                 // v5.5 正在滑动：滑完再画
  if (page == 2) { onWaitScreen = false; musicDirty = true; return; }
  if (page == 1) { onWaitScreen = false; wxDirty = true; return; }   // v5.3 正在看天气：先不画首页，滑回来时再画
  if (page == 3) { onWaitScreen = false; leanDirty = true; return; } // 正在看倾角仪表：先不画首页，滑回来时再画
  if (page == 4) { onWaitScreen = false; return; }                   // 正在伴侣模式：不画首页
  gfx->fillScreen(C_BLACK);
  onWaitScreen = true; waitSt = st;          // v3.6 首页猫猫 → v4.6 像素骑手
  riderPrevF = -1; riderPrevZ = -2;
  catForce = true;
  animateCat();
  printCentered("MotoNav", CX, 200, 6, st == WAIT_READY ? C_CYAN : C_GREY);
  if (st == WAIT_READY) {
    printCentered("ready", CX, 275, 3, C_GREEN);
  } else if (st == WAIT_NOLINK) {
    printCentered("no link", CX, 275, 3, C_GREY);
  } else {
    drawRedRing();
    printCentered("LINK LOST", CX, 275, 4, C_RED);
  }
  printCentered(bleName, CX, 332, 2, C_GREY);                          // v7.0 这块圆屏的蓝牙名
  printCentered(String("fw ") + FW_VERSION + "  " + String(qspiMHz) + "MHz", CX, 360, 2, C_GREY);
  drawStatus();
  drawPageDots();
  needFlush = true;
}

