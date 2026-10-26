#pragma once
// 抗锯齿矢量图元、文字工具和转向图标。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// ================= 基础画图工具 =================

int16_t rnd(float v) { return (int16_t)lroundf(v); }

// ================= v1.8.1 抗锯齿矢量图标 =================
// 画大图标时先把“线段 / 三角形 / 圆”记录下来，再逐像素按到图形边缘的距离算覆盖率，
// 边缘自然过渡（抗锯齿），线段之间的连接也没有缝。结果缓存起来，图标不变就直接复制。
#define PRIM_MAX   192      // v3.4 车道也用这套（多条车道图形较多）
#define ICON_BUF_W 360
#define ICON_BUF_H 360
struct Prim {
  uint8_t type;            // 0 = 线段（起点平头、终点圆头），1 = 三角形，2 = 圆
  uint16_t col;
  float ax, ay, bx, by, cx, cy, r;
  float x0, y0, x1, y1;    // 外框
};
Prim prims[PRIM_MAX];
int primN = 0;
bool recIcon = false;      // true = 正在记录图标，thickLine/arrowHead/iconCircle 只记录不直接画
uint16_t *iconBuf = nullptr;
int iconX = 0, iconY = 0, iconW = 0, iconH = 0;
uint32_t iconHash = 0;

void addPrim(const struct Prim &p) { if (primN < PRIM_MAX) prims[primN++] = p; }

void iconBegin() { primN = 0; recIcon = true; }

static inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }

// 一个像素中心 (px,py) 被图形覆盖的比例 0~1
float primAlpha(const struct Prim &p, float px, float py) {
  if (p.type == 0) {
    float dx = p.bx - p.ax, dy = p.by - p.ay;
    float len2 = dx * dx + dy * dy;
    float qx = px - p.ax, qy = py - p.ay;
    if (len2 < 1e-4f) return clamp01(p.r - sqrtf(qx * qx + qy * qy) + 0.5f);
    float h = (qx * dx + qy * dy) / len2;
    if (h < 0) {                                   // 起点：平头
      float len = sqrtf(len2);
      float perp = fabsf(qx * dy - qy * dx) / len;
      return clamp01(p.r - perp + 0.5f) * clamp01(0.5f + h * len);
    }
    if (h > 1) h = 1;                              // 终点：圆头
    float ex = qx - dx * h, ey = qy - dy * h;
    return clamp01(p.r - sqrtf(ex * ex + ey * ey) + 0.5f);
  }
  if (p.type == 1) {
    float vx[3] = {p.ax, p.bx, p.cx}, vy[3] = {p.ay, p.by, p.cy};
    float area = (vx[1] - vx[0]) * (vy[2] - vy[0]) - (vy[1] - vy[0]) * (vx[2] - vx[0]);
    float sgn = area > 0 ? 1.0f : -1.0f;
    float m = 1e9f;
    for (int i = 0; i < 3; i++) {
      int j = (i + 1) % 3;
      float ex = vx[j] - vx[i], ey = vy[j] - vy[i];
      float len = sqrtf(ex * ex + ey * ey);
      if (len < 1e-4f) return 0;
      float d = sgn * ((px - vx[i]) * ey - (py - vy[i]) * ex) / len;   // 到这条边的距离（里面为负）
      if (-d < m) m = -d;
    }
    return clamp01(m + 0.5f);
  }
  float qx = px - p.ax, qy = py - p.ay;
  return clamp01(p.r - sqrtf(qx * qx + qy * qy) + 0.5f);
}

bool iconHalo = false;              // v2.8 图标外加黑色描边（浮在地图上时用）
#define HALO_R 3

void iconRender() {
  float bx0 = 1e9f, by0 = 1e9f, bx1 = -1e9f, by1 = -1e9f;
  for (int i = 0; i < primN; i++) {
    bx0 = fminf(bx0, prims[i].x0); by0 = fminf(by0, prims[i].y0);
    bx1 = fmaxf(bx1, prims[i].x1); by1 = fmaxf(by1, prims[i].y1);
  }
  int mg = iconHalo ? HALO_R + 1 : 0;
  iconX = (int)floorf(bx0) - 1 - mg; iconY = (int)floorf(by0) - 1 - mg;
  if (iconX < 0) iconX = 0;
  if (iconY < 0) iconY = 0;
  iconW = (int)ceilf(bx1) + 2 + mg - iconX; iconH = (int)ceilf(by1) + 2 + mg - iconY;
  if (iconX + iconW > LCD_WIDTH)  iconW = LCD_WIDTH - iconX;
  if (iconY + iconH > LCD_HEIGHT) iconH = LCD_HEIGHT - iconY;
  if (iconW > ICON_BUF_W) iconW = ICON_BUF_W;
  if (iconH > ICON_BUF_H) iconH = ICON_BUF_H;
  if (iconW <= 0 || iconH <= 0) { iconW = iconH = 0; return; }
  for (int y = 0; y < iconH; y++) {
    float py = iconY + y + 0.5f;
    for (int x = 0; x < iconW; x++) {
      float px = iconX + x + 0.5f;
      float r = 0, g = 0, b = 0;                  // 背景是黑色，按顺序叠加每个图形
      for (int i = 0; i < primN; i++) {
        const Prim &p = prims[i];
        if (px < p.x0 || px > p.x1 || py < p.y0 || py > p.y1) continue;
        float a = primAlpha(p, px, py);
        if (a <= 0) continue;
        float cr = ((p.col >> 11) & 0x1F) / 31.0f, cg = ((p.col >> 5) & 0x3F) / 63.0f, cb = (p.col & 0x1F) / 31.0f;
        r += (cr - r) * a; g += (cg - g) * a; b += (cb - b) * a;
      }
      iconBuf[y * ICON_BUF_W + x] = ((uint16_t)lroundf(r * 31) << 11) | ((uint16_t)lroundf(g * 63) << 5) | (uint16_t)lroundf(b * 31);
    }
  }
  // v2.8 描边：图形周围 HALO_R 像素以内的空白处涂成接近黑色（纯黑 0 表示透明，所以用 0x0841）
  if (iconHalo) {
    for (int y = 0; y < iconH; y++) {
      for (int x = 0; x < iconW; x++) {
        if (iconBuf[y * ICON_BUF_W + x]) continue;
        bool near = false;
        for (int dy = -HALO_R; dy <= HALO_R && !near; dy++) {
          int yy = y + dy;
          if (yy < 0 || yy >= iconH) continue;
          for (int dx = -HALO_R; dx <= HALO_R; dx++) {
            int xx = x + dx;
            if (xx < 0 || xx >= iconW || dx * dx + dy * dy > HALO_R * HALO_R + 1) continue;
            uint16_t v = iconBuf[yy * ICON_BUF_W + xx];
            if (v && v != 0x0841) { near = true; break; }
          }
        }
        if (near) iconBuf[y * ICON_BUF_W + x] = 0x0841;
      }
    }
  }
}

// v1.8.2：把记录好的图标整体缩放、居中到 [top, bottom] 区域里（只缩小不放大）
void iconFitAt(float cx, float top, float bottom, float maxW) {
  if (primN == 0) return;
  float bx0 = 1e9f, by0 = 1e9f, bx1 = -1e9f, by1 = -1e9f;
  for (int i = 0; i < primN; i++) {
    bx0 = fminf(bx0, prims[i].x0 + 1); by0 = fminf(by0, prims[i].y0 + 1);
    bx1 = fmaxf(bx1, prims[i].x1 - 1); by1 = fmaxf(by1, prims[i].y1 - 1);
  }
  float w = bx1 - bx0, h = by1 - by0;
  if (w <= 0 || h <= 0) return;
  float sc = fminf(1.0f, fminf((bottom - top) / h, maxW / w));
  float ox = (bx0 + bx1) / 2, oy = (by0 + by1) / 2;
  float nx = cx, ny = (top + bottom) / 2;
  for (int i = 0; i < primN; i++) {
    Prim &p = prims[i];
    p.ax = nx + (p.ax - ox) * sc; p.ay = ny + (p.ay - oy) * sc;
    p.bx = nx + (p.bx - ox) * sc; p.by = ny + (p.by - oy) * sc;
    p.cx = nx + (p.cx - ox) * sc; p.cy = ny + (p.cy - oy) * sc;
    p.r *= sc;
    p.x0 = nx + (p.x0 + 1 - ox) * sc - 1; p.y0 = ny + (p.y0 + 1 - oy) * sc - 1;
    p.x1 = nx + (p.x1 - 1 - ox) * sc + 1; p.y1 = ny + (p.y1 - 1 - oy) * sc + 1;
  }
}

void iconFit(float top, float bottom, float maxW) { iconFitAt(CX, top, bottom, maxW); }

// 结束记录：图标和上次一样就直接用缓存，否则重新计算；然后复制到画布
void iconEnd() {
  recIcon = false;
  if (!iconBuf || primN == 0) return;
  uint32_t h = 2166136261u;                      // FNV 哈希，判断图标有没有变化
  const uint8_t *bytes = (const uint8_t *)prims;
  for (size_t i = 0; i < sizeof(Prim) * primN; i++) { h ^= bytes[i]; h *= 16777619u; }
  h ^= primN;
  h ^= iconHalo ? 0x5A5A5A5Au : 0;
  if (h != iconHash) { iconRender(); iconHash = h; }
  uint16_t *fb = gfx->getFramebuffer();
  if (!fb) return;
  for (int y = 0; y < iconH; y++) {
    const uint16_t *src = iconBuf + y * ICON_BUF_W;
    uint16_t *dst = fb + (iconY + y) * LCD_WIDTH + iconX;
    for (int x = 0; x < iconW; x++) if (src[x]) dst[x] = src[x];
  }
}

void iconCircle(float x, float y, float r, uint16_t c) {
  if (recIcon) {
    Prim p; memset(&p, 0, sizeof(p));
    p.type = 2; p.col = c; p.ax = x; p.ay = y; p.r = r;
    p.x0 = x - r - 1; p.y0 = y - r - 1; p.x1 = x + r + 1; p.y1 = y + r + 1;
    addPrim(p);
    return;
  }
  gfx->fillCircle(x, y, r, c);
}

// 有宽度的线（终点带圆头）
void thickLine(float x0, float y0, float x1, float y1, float w, uint16_t c) {
  if (recIcon) {
    Prim p; memset(&p, 0, sizeof(p));
    float r = w / 2;
    p.type = 0; p.col = c; p.ax = x0; p.ay = y0; p.bx = x1; p.by = y1; p.r = r;
    p.x0 = fminf(x0, x1) - r - 1; p.y0 = fminf(y0, y1) - r - 1;
    p.x1 = fmaxf(x0, x1) + r + 1; p.y1 = fmaxf(y0, y1) + r + 1;
    addPrim(p);
    return;
  }
  float dx = x1 - x0, dy = y1 - y0;
  float len = sqrtf(dx * dx + dy * dy);
  if (len < 1) return;
  float nx = -dy / len * w / 2, ny = dx / len * w / 2;
  gfx->fillTriangle(x0 + nx, y0 + ny, x0 - nx, y0 - ny, x1 + nx, y1 + ny, c);
  gfx->fillTriangle(x0 - nx, y0 - ny, x1 - nx, y1 - ny, x1 + nx, y1 + ny, c);
  gfx->fillCircle(x1, y1, w / 2, c);
}

// 箭头头部；deg：0 = 朝上，-90 = 朝左，90 = 朝右，180 = 朝下
void arrowHead(float x, float y, float deg, float s, uint16_t c) {
  float a = deg * PI / 180;
  float dx = sinf(a), dy = -cosf(a);
  float px = cosf(a), py = sinf(a);
  if (recIcon) {
    Prim p; memset(&p, 0, sizeof(p));
    p.type = 1; p.col = c;
    p.ax = x + dx * s;           p.ay = y + dy * s;
    p.bx = x + px * s * 0.8f;    p.by = y + py * s * 0.8f;
    p.cx = x - px * s * 0.8f;    p.cy = y - py * s * 0.8f;
    p.x0 = fminf(fminf(p.ax, p.bx), p.cx) - 1; p.y0 = fminf(fminf(p.ay, p.by), p.cy) - 1;
    p.x1 = fmaxf(fmaxf(p.ax, p.bx), p.cx) + 1; p.y1 = fmaxf(fmaxf(p.ay, p.by), p.cy) + 1;
    addPrim(p);
    return;
  }
  gfx->fillTriangle(x + dx * s, y + dy * s,
                    x + px * s * 0.8f, y + py * s * 0.8f,
                    x - px * s * 0.8f, y - py * s * 0.8f, c);
}

float min4(float a, float b, float c, float d) { return fminf(fminf(a, b), fminf(c, d)); }
float max4(float a, float b, float c, float d) { return fmaxf(fmaxf(a, b), fmaxf(c, d)); }

// 居中打印一行字（自带字体每个字符宽 6*size 像素）
void printCentered(const String &s, int x, int y, int size, uint16_t color) {
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  gfx->setCursor(x - (int)s.length() * 6 * size / 2, y);
  gfx->print(s);
}

String distText(int m) {
  return (m >= 1000) ? String(m / 1000.0, 1) + "km" : String(m) + "m";
}

// 画一条“先竖直向上、再圆滑地拐向 deg 方向”的粗线。
// (ox, oy) 是拐角；stem = 拐角以下竖直段长度；arm = 拐出段长度；fillet = 圆角半径。
void bentStroke(float ox, float oy, float stem, float arm, float deg, float fillet, float w,
                uint16_t stemCol, uint16_t armCol, float &endX, float &endY, float maxT) {
  float a = deg * PI / 180;
  float dx = sinf(a), dy = -cosf(a);
  float t = fillet * tanf(fabsf(a) / 2);
  if (fabsf(deg) > 3) {                     // v1.8.1：小角度转弯也给一段明显的圆弧，不要折角
    float minT = fminf(fillet * 0.55f, maxT);
    if (t < minT) t = minT;
  }
  if (t > maxT)        t = maxT;          // v1.8：锐角弯时圆角不要太大
  if (t > arm * 0.7f)  t = arm * 0.7f;
  if (t > stem * 0.7f) t = stem * 0.7f;
  float p1x = ox,          p1y = oy + t;
  float p2x = ox + dx * t, p2y = oy + dy * t;
  endX = ox + dx * arm;
  endY = oy + dy * arm;
  if (t < 0.5f && fabsf(deg) < 1 && stemCol == armCol) {   // 直行：一整根，不留接缝
    thickLine(ox, oy + stem, endX, endY, w, stemCol);
    return;
  }
  float lx = p1x, ly = p1y;
  const int N = 12;
  for (int i = 1; i <= N; i++) {
    float u = (float)i / N, v = 1 - u;
    float x = v * v * p1x + 2 * v * u * ox + u * u * p2x;
    float y = v * v * p1y + 2 * v * u * oy + u * u * p2y;
    thickLine(lx, ly, x, y, w, armCol);
    lx = x; ly = y;
  }
  thickLine(p2x, p2y, endX, endY, w, armCol);
  thickLine(ox, oy + stem, p1x, p1y, w, stemCol);
}

// ================= 大图标（中、近阶段），k = 缩放比例，cy = 中心 y =================

void drawTurn(float deg, uint16_t col, float cy, float k) {
  // v1.8：转角超过 90 度（锐角弯）时，竖段和拐出段加长、线条稍细、圆角变小，
  //       让箭头和竖线分得开，看起来是清楚的锐角
  float sv = constrain((fabsf(deg) - 90) / 60, 0.0f, 1.0f);
  const float W = (42 - 6 * sv) * k, STEM = (86 + 40 * sv) * k, ARM = (78 + 30 * sv) * k, HEAD = (62 - 6 * sv) * k;
  float a = deg * PI / 180;
  float dx = sinf(a), dy = -cosf(a);
  float px = cosf(a), py = sinf(a);
  float ex = dx * ARM, ey = dy * ARM;
  float tx = ex + dx * HEAD, ty = ey + dy * HEAD;
  float c1x = ex + px * HEAD * 0.8f, c1y = ey + py * HEAD * 0.8f;
  float c2x = ex - px * HEAD * 0.8f, c2y = ey - py * HEAD * 0.8f;
  float minX = fminf(min4(-W / 2, ex - W / 2, tx, c1x), c2x);
  float maxX = fmaxf(max4(W / 2, ex + W / 2, tx, c1x), c2x);
  float minY = fminf(min4(-W / 2, ey - W / 2, ty, c1y), c2y);
  float maxY = fmaxf(max4(STEM, ey + W / 2, ty, c1y), c2y);     // v1.8：锐角弯箭头朝下，也要算进外框
  float ox = CX - (minX + maxX) / 2;
  float oy = cy - (minY + maxY) / 2;
  float endX, endY;
  bentStroke(ox, oy, STEM, ARM, deg, 40 * k, W, col, col, endX, endY, (40 - 28 * sv) * k);
  arrowHead(endX, endY, deg, HEAD, col);
}

void drawUturn(uint16_t col, float cy, float k) {
  const float W = 42 * k, R = 54 * k, LEG = 130 * k, DOWN = 44 * k, HEAD = 50 * k;
  float minX = -R - HEAD * 0.8f, maxX = R + W / 2;
  float minY = -R - W / 2,       maxY = fmaxf(LEG, DOWN + HEAD);
  float ox = CX - (minX + maxX) / 2;
  float oy = cy - (minY + maxY) / 2;
  thickLine(ox + R, oy + LEG, ox + R, oy, W, col);
  float lx = ox + R, ly = oy;
  for (int i = 1; i <= 16; i++) {
    float t = PI * i / 16;
    float x = ox + R * cosf(t), y = oy - R * sinf(t);
    thickLine(lx, ly, x, y, W, col);
    lx = x; ly = y;
  }
  thickLine(ox - R, oy, ox - R, oy + DOWN, W, col);
  arrowHead(ox - R, oy + DOWN, 180, HEAD, col);
}

void drawArrive(uint16_t col, float cy, float k) {
  iconCircle(CX, cy, 88 * k, col);
  iconCircle(CX, cy, 43 * k, C_BLACK);
  iconCircle(CX, cy, 20 * k, col);
}

bool dirAngle(const String &dir, float &deg) {
  // v1.8：有实际角度、并且左右方向一致时，用实际角度
  if (!isnan(navAngle) && (dir == "L" || dir == "R" || dir == "SL" || dir == "SR")) {
    bool left = (dir == "L" || dir == "SL");
    if ((left && navAngle < 0) || (!left && navAngle > 0)) {
      deg = constrain(navAngle, -150.0f, 150.0f);
      return true;
    }
  }
  if (dir == "L")  { deg = -90; return true; }
  if (dir == "R")  { deg = 90;  return true; }
  if (dir == "SL") { deg = -45; return true; }
  if (dir == "SR") { deg = 45;  return true; }
  return false;
}

// 大图标；laneK = 有车道时的额外缩小比例
void drawBigIconAt(const String &dir, uint16_t col, float cy, float laneK,
                   float cx, float top, float bottom, float maxW) {
  float deg;
  iconBegin();
  if (dir == "U")              drawUturn(col, cy, 0.70f * laneK);
  else if (dir == "A")         drawArrive(col == C_WHITE ? C_GREEN : col, cy, 0.78f * laneK);
  else if (dirAngle(dir, deg)) drawTurn(deg, col, cy, 0.78f * laneK);
  else                         drawTurn(0, col, cy, 0.62f * laneK);   // 直行
  iconFitAt(cx, top, bottom, maxW);
  iconEnd();
}

void drawBigIcon(const String &dir, uint16_t col, float cy, float laneK, float top, float bottom) {
  drawBigIconAt(dir, col, cy, laneK, CX, top, bottom, ICON_MAX_W);
}

// ================= 远阶段预告：白色直行竖条（居中）+ 灰色转向预告 =================

void drawPreviewTurn(float deg, float cy, float k) {
  float sv = constrain((fabsf(deg) - 90) / 60, 0.0f, 1.0f);   // v1.8 锐角弯：拐出段加长
  const float W = 30 * k, STEM = 175 * k, ARM = (52 + 25 * sv) * k, HEAD = 42 * k;
  float a = deg * PI / 180;
  float dy = -cosf(a), py = sinf(a);
  float ey = dy * ARM;
  float ty = ey + dy * HEAD;
  float c1y = ey + py * HEAD * 0.8f, c2y = ey - py * HEAD * 0.8f;
  float minY = fminf(min4(-W / 2, ey - W / 2, ty, c1y), c2y);
  float maxY = fmaxf(max4(STEM, ey + W / 2, ty, c1y), c2y);
  float oy = cy - (minY + maxY) / 2;
  float endX, endY;
  bentStroke(CX, oy, STEM, ARM, deg, 26 * k, W, C_WHITE, C_DIM, endX, endY, (26 - 16 * sv) * k);
  arrowHead(endX, endY, deg, HEAD, C_DIM);
}

void drawPreviewUturn(float cy, float k) {
  const float W = 30 * k, STEM = 175 * k, R = 30 * k, DOWN = 26 * k, HEAD = 42 * k;
  float oy = cy - ((-R - W / 2) + STEM) / 2;
  float ox = CX;
  float lx = ox, ly = oy;
  for (int i = 1; i <= 12; i++) {
    float t = PI * i / 12;
    float x = ox - R + R * cosf(t), y = oy - R * sinf(t);
    thickLine(lx, ly, x, y, W, C_DIM);
    lx = x; ly = y;
  }
  thickLine(ox - 2 * R, oy, ox - 2 * R, oy + DOWN, W, C_DIM);
  arrowHead(ox - 2 * R, oy + DOWN, 180, HEAD * 0.8f, C_DIM);
  thickLine(ox, oy + STEM, ox, oy, W, C_WHITE);
}

void drawPreviewArrive(float cy, float k) {
  const float W = 30 * k, STEM = 175 * k, TR = 32 * k;
  float oy = cy - ((-2 * TR - 8 * k) + STEM) / 2;
  thickLine(CX, oy + STEM, CX, oy, W, C_WHITE);
  float ty = oy - TR - 8 * k;
  iconCircle(CX, ty, TR, C_DIM);
  iconCircle(CX, ty, TR / 2, C_BLACK);
  iconCircle(CX, ty, TR / 5, C_DIM);
}

void drawPreview(const String &dir, float cy, float k, float top, float bottom) {
  float deg;
  iconBegin();
  if (dir == "U")              drawPreviewUturn(cy, k);
  else if (dir == "A")         drawPreviewArrive(cy, k);
  else if (dirAngle(dir, deg)) drawPreviewTurn(deg, cy, k);
  iconFit(top, bottom, ICON_MAX_W);
  iconEnd();
}
