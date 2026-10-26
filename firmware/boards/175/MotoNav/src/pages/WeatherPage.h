#pragma once
// 天气图标、页面和天气消息解析。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// ================= v5.3 天气页 =================
// 图标编号（手机按高德的天气文字归类）：0 晴 1 多云 2 阴 3 雨 4 大雨 5 雷雨 6 雪 7 雾霾 8 雨夹雪 9 沙尘 10 夜晴 11 夜多云
bool wxHave = false;
int wxIcon = 0, wxTemp = 0;
String wxTime = "";
int wxN = 0;
bool wxHourly = false;                 // v5.6 下面一排是逐小时（H）还是按天（D）
int wxWk[6], wxFi[6], wxFh[6], wxFl[6];   // 按天：星期/图标/最高/最低；逐小时：几点/图标/温度/降雨%
unsigned long wxAt = 0, wxAskAt = 0;

#define WX_SUN   0xFE60
#define WX_CLOUD 0xDEFB
#define WX_DARK  0x8C71
#define WX_RAIN  0x4D7F
#define WX_DUST  0xC4A8

// 粗线：沿线段画一串抗锯齿圆点
void wxLine(float x0, float y0, float x1, float y1, float w, uint16_t c) {
  float len = sqrtf((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
  int n = max(1, (int)(len / 1.5f));
  for (int i = 0; i <= n; i++) aaDisc(x0 + (x1 - x0) * i / n, y0 + (y1 - y0) * i / n, w / 2, c);
}

void wxSun(float cx, float cy, float r) {
  for (int k = 0; k < 8; k++) {
    float a = k * PI / 4;
    wxLine(cx + cosf(a) * r * 1.35f, cy + sinf(a) * r * 1.35f, cx + cosf(a) * r * 1.75f, cy + sinf(a) * r * 1.75f, max(2.0f, r * 0.18f), WX_SUN);
  }
  aaDisc(cx, cy, r, WX_SUN);
}

void wxMoon(float cx, float cy, float r) {
  aaDisc(cx, cy, r, WX_SUN);
  aaDisc(cx + r * 0.45f, cy - r * 0.3f, r * 0.82f, C_BLACK);
}

// 云：三个圆 + 圆角底座
void wxCloud(float cx, float cy, float s, uint16_t c) {
  aaDisc(cx - s * 0.5f, cy + s * 0.08f, s * 0.42f, c);
  aaDisc(cx + s * 0.05f, cy - s * 0.2f, s * 0.58f, c);
  aaDisc(cx + s * 0.55f, cy + s * 0.12f, s * 0.38f, c);
  int r = (int)(s * 0.25f);
  gfx->fillRoundRect((int)(cx - s * 0.92f), (int)(cy + s * 0.02f), (int)(s * 1.84f), (int)(s * 0.5f), r, c);
}

// 画一个天气图标；s ≈ 图标半宽
void drawWxIcon(int id, float cx, float cy, float s) {
  switch (id) {
    case 0: wxSun(cx, cy, s * 0.55f); break;
    case 10: wxMoon(cx, cy, s * 0.7f); break;
    case 1: case 11:
      if (id == 1) wxSun(cx + s * 0.3f, cy - s * 0.3f, s * 0.42f); else wxMoon(cx + s * 0.3f, cy - s * 0.35f, s * 0.5f);
      wxCloud(cx - s * 0.12f, cy + s * 0.2f, s * 0.78f, WX_CLOUD);
      break;
    case 2: wxCloud(cx, cy, s * 0.95f, WX_CLOUD); break;
    case 3: case 4: case 8: {
      int drops = id == 4 ? 4 : 3;
      for (int k = 0; k < drops; k++) {
        float x = cx + (k - (drops - 1) / 2.0f) * s * 0.42f;
        if (id == 8 && k % 2) aaDisc(x - s * 0.08f, cy + s * 0.72f, s * 0.11f, C_WHITE);
        else wxLine(x, cy + s * 0.5f, x - s * 0.16f, cy + s * 0.92f, max(2.0f, s * 0.1f), WX_RAIN);
      }
      wxCloud(cx, cy - s * 0.2f, s * 0.9f, id == 4 ? WX_DARK : WX_CLOUD);
      break;
    }
    case 5: {
      wxCloud(cx, cy - s * 0.25f, s * 0.9f, WX_DARK);
      float x = cx, y = cy + s * 0.2f;           // 闪电
      gfx->fillTriangle((int)(x + s * 0.12f), (int)y, (int)(x - s * 0.22f), (int)(y + s * 0.42f), (int)(x + s * 0.02f), (int)(y + s * 0.42f), WX_SUN);
      gfx->fillTriangle((int)(x + s * 0.1f), (int)(y + s * 0.3f), (int)(x - s * 0.12f), (int)(y + s * 0.85f), (int)(x + s * 0.2f), (int)(y + s * 0.3f), WX_SUN);
      break;
    }
    case 6:
      wxCloud(cx, cy - s * 0.2f, s * 0.9f, WX_CLOUD);
      for (int k = 0; k < 3; k++) aaDisc(cx + (k - 1) * s * 0.42f, cy + s * 0.68f + (k % 2) * s * 0.18f, s * 0.11f, C_WHITE);
      break;
    case 7: case 9:
      for (int k = 0; k < 4; k++) {
        float y = cy - s * 0.45f + k * s * 0.3f, off = (k % 2) * s * 0.18f;
        wxLine(cx - s * 0.75f + off, y, cx + s * 0.6f + off, y, max(3.0f, s * 0.16f), id == 9 ? WX_DUST : WX_DARK);
      }
      break;
    default: wxCloud(cx, cy, s * 0.95f, WX_CLOUD);
  }
}

// 温度大字 + 度数小圆圈
void drawTemp(int t, int cx, int y, int size, uint16_t c) {
  String v = String(t);
  int w = v.length() * 6 * size;
  printCentered(v, cx - size * 2, y, size, c);
  float ox = cx - size * 2 + w / 2.0f + size * 1.6f, oy = y + size * 2.5f;
  for (int a = 0; a < 360; a += 12) aaDisc(ox + cosf(a * PI / 180) * size * 1.3f, oy + sinf(a * PI / 180) * size * 1.3f, max(1.5f, size * 0.32f), c);
}

const char *WX_WEEK[] = {"", "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};

// QWeather extension: no strings in BLE packets; Chinese summaries use image slots 7-11.
int wxDetail = 0, wxAQI = -1, wxAlerts = -1, wxLevel = 0, wxRainN = 0;
int wxRain[24];
unsigned long wxAlertTag = 0;
bool wxExtra = false;
void weatherTap() { if (wxHave && wxExtra) { wxDetail = (wxDetail + 1) % 3; wxDirty = true; pageAt = millis(); } }
void parseWeatherExtra(const String &m) {
  char buffer[240]; m.toCharArray(buffer, sizeof(buffer));
  char *save = nullptr; strtok_r(buffer, ",", &save);
  int values[29], n = 0; char *token;
  while ((token = strtok_r(nullptr, ",", &save)) && n < 29) values[n++] = atoi(token);
  if (n < 5 || values[3] < 0 || values[3] > 24 || n < 5 + values[3]) return;
  wxAQI = values[0]; wxAlerts = values[1]; wxLevel = values[2]; wxRainN = values[3];
  unsigned long tag = (unsigned long) values[4];
  if (wxAlerts > 0 && tag != wxAlertTag && page == 1) wxDetail = 2;
  wxAlertTag = tag;
  for (int i = 0; i < wxRainN; ++i) wxRain[i] = values[5+i];
  wxExtra = true; wxDirty = true;
}
bool drawWeatherExtra() {
  if (!wxExtra || wxDetail == 0) return false;
  gfx->fillScreen(C_BLACK);
  if (wxDetail == 1) {
    printCentered("RAIN / AIR", CX, 55, 3, C_CYAN);
    drawTextImg(7, 105, 0xFF);
    int peak = 0;
    for (int i=0; i<wxRainN; ++i) peak = max(peak, wxRain[i]);
    if (wxRainN > 0) {
      for (int i=0; i<wxRainN; ++i) {
        int x = 95 + i * 276 / wxRainN;
        int height = wxRain[i] < 0 ? 2 : wxRain[i] * 72 / max(1, peak);
        gfx->fillRect(x, 225-height, max(2, 260/wxRainN), max(2,height), wxRain[i] < 0 ? C_GREY : C_CYAN);
      }
      printCentered("NEXT 2H  MAX " + String(peak / 100.0f, 2) + " mm/5m", CX, 240, 2, C_GREY);
    } else printCentered("RAIN DATA UNAVAILABLE", CX, 190, 2, C_GREY);
    uint16_t airColor = wxAQI < 0 ? C_GREY : C_CYAN;
    drawTextImg(8, 285, 0xFF);
    printCentered(wxAQI < 0 ? "AQI --" : "AQI " + String(wxAQI), CX, 335, 3, airColor);
  } else {
    printCentered(wxAlerts > 0 ? "WEATHER ALERT" : "ALERT STATUS", CX, 65, 3, wxLevel >= 3 ? C_ORANGE : C_CYAN);
    drawTextImg(9, 125, 0xFF);
    drawTextImg(10, 200, 0xDC);
    drawTextImg(11, 270, 0xDC);
    if (wxAlerts > 1) printCentered(String(wxAlerts) + " ALERTS - DETAILS ON PHONE", CX, 330, 1, C_GREY);
    printCentered("MAY BE DELAYED - CHECK OFFICIAL", CX, 365, 1, C_GREY);
  }
  printCentered("QWEATHER - TAP TO SWITCH", CX, 400, 1, C_GREY);
  drawPageDots(); needFlush = true;
  return true;
}

void drawWeather() {
  if (wxHave && drawWeatherExtra()) return;
  onWaitScreen = false;
  frameIsMap = false;
  gfx->fillScreen(C_BLACK);
  if (!wxHave) {
    drawWxIcon(1, CX, 190, 60);
    printCentered(bleConnected ? "LOADING..." : "NO PHONE", CX, 290, 3, C_GREY);
    printCentered("WEATHER", CX, 70, 2, C_GREY);
    drawPageDots();
    needFlush = true;
    return;
  }
  // 地名 · 天气
  drawTextImg(3, 56, 0xFF);
  // 大图标 + 温度
  drawWxIcon(wxIcon, CX - 78, 165, 58);
  drawTemp(wxTemp, CX + 82, 135, 8, C_WHITE);
  // 风 · 湿度
  drawTextImg(4, 232, 0xC0);
  if (wxHourly) {
    if (wxN <= 0) {
      printCentered("HOURLY UNAVAILABLE", CX, 322, 2, C_GREY);
      printCentered("CHECK PHONE WEATHER DETAILS", CX, 354, 1, C_GREY);
    }
    // v5.6 接下来几个小时：几点 / 图标 / 温度 / 降雨概率（≥10% 才显示）
    for (int i = 0; i < wxN && i < 6; i++) {
      int x = CX + (int)((i - (wxN - 1) / 2.0f) * 76);
      char hh[8]; snprintf(hh, sizeof(hh), "%02d:00", wxWk[i]);
      printCentered(hh, x, 280, 2, C_GREY);
      drawWxIcon(wxFi[i], x, 322, 19);
      String t = String(wxFh[i]);
      printCentered(t, x - 4, 348, 2, C_WHITE);
      float ox = x - 4 + t.length() * 6 + 3, oy = 351;      // 小度数圈
      for (int a = 0; a < 360; a += 30) aaDisc(ox + cosf(a * PI / 180) * 2.6f, oy + sinf(a * PI / 180) * 2.6f, 0.9f, C_WHITE);
      if (wxFl[i] >= 10) printCentered(String(wxFl[i]) + "%", x, 372, 2, WX_RAIN);
    }
  } else {
    // 按天（手机查不到逐小时时）
    for (int i = 0; i < wxN && i < 4; i++) {
      int x = CX + (int)((i - (wxN - 1) / 2.0f) * 92);
      String lb = i == 0 ? String("TODAY") : String(WX_WEEK[wxWk[i] >= 1 && wxWk[i] <= 7 ? wxWk[i] : 0]);
      printCentered(lb, x, 282, 2, i == 0 ? C_CYAN : C_GREY);
      drawWxIcon(wxFi[i], x, 328, 20);
      printCentered(String(wxFh[i]) + "/" + String(wxFl[i]), x, 360, 2, C_WHITE);
    }
  }
  if (wxTime.length()) printCentered("QWEATHER " + wxTime + (wxAlerts > 0 ? " ! TAP" : " TAP"), CX, 406, 1, 0x5AEB);
  drawPageDots();
  needFlush = true;
}

// "#W,图标,温度,更新时间,H/D,个数,{…4 个数…}..."
void parseWeather(const String &m) {
  char buf[220];
  m.toCharArray(buf, sizeof(buf));
  char *save = nullptr;
  char *tok = strtok_r(buf, ",", &save);           // "#W"
  String f[32]; int n = 0;
  while ((tok = strtok_r(nullptr, ",", &save)) && n < 32) f[n++] = tok;
  if (n < 5) return;
  wxIcon = f[0].toInt(); wxTemp = f[1].toInt(); wxTime = f[2];
  wxHourly = f[3] == "H";
  wxN = min(wxHourly ? 6 : 4, (int)f[4].toInt());
  for (int i = 0; i < wxN; i++) {
    int b = 5 + i * 4;
    if (b + 3 >= n) { wxN = i; break; }
    wxWk[i] = f[b].toInt(); wxFi[i] = f[b + 1].toInt(); wxFh[i] = f[b + 2].toInt(); wxFl[i] = f[b + 3].toInt();
  }
  wxHave = true;
  wxAt = millis();
  wxDirty = true;
}
