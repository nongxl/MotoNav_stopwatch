package com.example.motonav

import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.roundToInt
import kotlin.math.sin
import kotlin.math.sqrt

/**
 * 版本：跟随 App 版本，见 MainActivity.kt 里的 APP_VERSION（v2.0 新增）
 *
 * 小地图用到的纯计算部分（不依赖安卓，可以单独测试）：
 * - WGS-84（GPS 原生坐标）和 GCJ-02（高德用的坐标）互相换算
 * - 经纬度 ↔ 平面米坐标（以本次运行的第一个定位点为原点）
 * - 折线简化（Douglas-Peucker）、多边形拼接、裁剪
 * - 把地图块、导航路线打包成发给圆屏的二进制数据
 */
object MapGeo {
    // ---------------- 坐标换算 ----------------
    private const val A = 6378245.0
    private const val EE = 0.00669342162296594323

    private fun outOfChina(lat: Double, lng: Double) =
        lng < 72.004 || lng > 137.8347 || lat < 0.8293 || lat > 55.8271

    private fun tLat(x: Double, y: Double): Double {
        var r = -100.0 + 2.0 * x + 3.0 * y + 0.2 * y * y + 0.1 * x * y + 0.2 * sqrt(abs(x))
        r += (20.0 * sin(6.0 * x * Math.PI) + 20.0 * sin(2.0 * x * Math.PI)) * 2.0 / 3.0
        r += (20.0 * sin(y * Math.PI) + 40.0 * sin(y / 3.0 * Math.PI)) * 2.0 / 3.0
        r += (160.0 * sin(y / 12.0 * Math.PI) + 320.0 * sin(y * Math.PI / 30.0)) * 2.0 / 3.0
        return r
    }

    private fun tLng(x: Double, y: Double): Double {
        var r = 300.0 + x + 2.0 * y + 0.1 * x * x + 0.1 * x * y + 0.1 * sqrt(abs(x))
        r += (20.0 * sin(6.0 * x * Math.PI) + 20.0 * sin(2.0 * x * Math.PI)) * 2.0 / 3.0
        r += (20.0 * sin(x * Math.PI) + 40.0 * sin(x / 3.0 * Math.PI)) * 2.0 / 3.0
        r += (150.0 * sin(x / 12.0 * Math.PI) + 300.0 * sin(x / 30.0 * Math.PI)) * 2.0 / 3.0
        return r
    }

    /** WGS-84 → GCJ-02，返回 [lat, lng] */
    fun wgsToGcj(lat: Double, lng: Double): DoubleArray {
        if (outOfChina(lat, lng)) return doubleArrayOf(lat, lng)
        var dLat = tLat(lng - 105.0, lat - 35.0)
        var dLng = tLng(lng - 105.0, lat - 35.0)
        val radLat = lat / 180.0 * Math.PI
        var magic = sin(radLat)
        magic = 1 - EE * magic * magic
        val sqrtMagic = sqrt(magic)
        dLat = (dLat * 180.0) / ((A * (1 - EE)) / (magic * sqrtMagic) * Math.PI)
        dLng = (dLng * 180.0) / (A / sqrtMagic * cos(radLat) * Math.PI)
        return doubleArrayOf(lat + dLat, lng + dLng)
    }

    /** GCJ-02 → WGS-84（迭代两次，误差小于 1 米） */
    fun gcjToWgs(lat: Double, lng: Double): DoubleArray {
        var wLat = lat
        var wLng = lng
        repeat(2) {
            val g = wgsToGcj(wLat, wLng)
            wLat -= g[0] - lat
            wLng -= g[1] - lng
        }
        return doubleArrayOf(wLat, wLng)
    }

    // ---------------- 平面坐标（米） ----------------
    // 以参考点为原点：x 向东，y 向北。几十公里范围内误差很小，够地图用。
    class Frame(val refLat: Double, val refLng: Double, val id: Int) {
        private val kx = cos(Math.toRadians(refLat)) * 111320.0
        private val ky = 110574.0
        fun x(lng: Double) = (lng - refLng) * kx
        fun y(lat: Double) = (lat - refLat) * ky
        fun lng(x: Double) = refLng + x / kx
        fun lat(y: Double) = refLat + y / ky
    }

    // ---------------- 地图要素 ----------------
    // 道路等级（线）：1 高速/快速路，2 主干道，3 次干道，4 支路/小区路，5 辅路/匝道
    // 面：10 绿地公园，11 水面
    const val CLS_PARK = 10
    const val CLS_WATER = 11

    /** 一个要素：pts 是平面米坐标 [x0,y0,x1,y1,...] */
    class Feature(val cls: Int, val poly: Boolean, val pts: DoubleArray)

    /** Douglas-Peucker 折线简化，tol = 允许偏差（米） */
    fun simplify(pts: DoubleArray, tol: Double): DoubleArray {
        val n = pts.size / 2
        if (n <= 2) return pts
        val keep = BooleanArray(n)
        keep[0] = true; keep[n - 1] = true
        val stack = ArrayDeque<IntArray>()
        stack.addLast(intArrayOf(0, n - 1))
        val tol2 = tol * tol
        while (stack.isNotEmpty()) {
            val (s, e) = stack.removeLast()
            if (e <= s + 1) continue
            val ax = pts[2 * s]; val ay = pts[2 * s + 1]
            val bx = pts[2 * e]; val by = pts[2 * e + 1]
            val dx = bx - ax; val dy = by - ay
            val len2 = dx * dx + dy * dy
            var maxD = -1.0; var idx = -1
            for (i in s + 1 until e) {
                val px = pts[2 * i] - ax; val py = pts[2 * i + 1] - ay
                val d2 = if (len2 < 1e-9) px * px + py * py else {
                    val c = px * dy - py * dx
                    c * c / len2
                }
                if (d2 > maxD) { maxD = d2; idx = i }
            }
            if (maxD > tol2) {
                keep[idx] = true
                stack.addLast(intArrayOf(s, idx))
                stack.addLast(intArrayOf(idx, e))
            }
        }
        val out = ArrayList<Double>()
        for (i in 0 until n) if (keep[i]) { out.add(pts[2 * i]); out.add(pts[2 * i + 1]) }
        return out.toDoubleArray()
    }

    /**
     * 把多段首尾相接的线拼成闭合环（大型公园、河流常由多段边界组成）。
     * 端点距离小于 0.5 米就算相接。
     */
    fun joinRings(parts: List<DoubleArray>): List<DoubleArray> {
        val pool = parts.filter { it.size >= 4 }.toMutableList()
        val rings = ArrayList<DoubleArray>()
        fun close(ax: Double, ay: Double, bx: Double, by: Double) =
            (ax - bx) * (ax - bx) + (ay - by) * (ay - by) < 0.25
        while (pool.isNotEmpty()) {
            var cur = pool.removeAt(0)
            var progress = true
            while (progress) {
                val n = cur.size
                if (close(cur[0], cur[1], cur[n - 2], cur[n - 1])) break      // 已闭合
                progress = false
                for (i in pool.indices) {
                    val p = pool[i]
                    val m = p.size
                    val ex = cur[n - 2]; val ey = cur[n - 1]
                    when {
                        close(ex, ey, p[0], p[1]) -> { cur = cur + p.copyOfRange(2, m) }
                        close(ex, ey, p[m - 2], p[m - 1]) -> { cur = cur + reversePts(p).copyOfRange(2, m) }
                        close(cur[0], cur[1], p[m - 2], p[m - 1]) -> { cur = p + cur.copyOfRange(2, n) }
                        close(cur[0], cur[1], p[0], p[1]) -> { cur = reversePts(p) + cur.copyOfRange(2, n) }
                        else -> continue
                    }
                    pool.removeAt(i)
                    progress = true
                    break
                }
            }
            rings.add(cur)          // 拼不完整的也保留（按首尾直接相连来填充）
        }
        return rings
    }

    private fun reversePts(p: DoubleArray): DoubleArray {
        val n = p.size / 2
        val r = DoubleArray(p.size)
        for (i in 0 until n) { r[2 * i] = p[2 * (n - 1 - i)]; r[2 * i + 1] = p[2 * (n - 1 - i) + 1] }
        return r
    }

    /** 把折线裁剪到方框里（保留进出方框的那一段），返回若干段 */
    fun clipLine(pts: DoubleArray, x0: Double, y0: Double, x1: Double, y1: Double): List<DoubleArray> {
        val n = pts.size / 2
        // 一段线的外框和方框有重叠，就保留这段的两个端点（两端都在框外但穿过框的长直路也不会丢）
        val keep = BooleanArray(n)
        for (i in 0 until n - 1) {
            val ax = pts[2 * i]; val ay = pts[2 * i + 1]; val bx = pts[2 * i + 2]; val by = pts[2 * i + 3]
            if (maxOf(ax, bx) >= x0 && minOf(ax, bx) <= x1 && maxOf(ay, by) >= y0 && minOf(ay, by) <= y1) {
                keep[i] = true; keep[i + 1] = true
            }
        }
        val out = ArrayList<DoubleArray>()
        var i = 0
        while (i < n) {
            if (!keep[i]) { i++; continue }
            val start = i
            while (i < n && keep[i]) i++
            if (i - start >= 2) out.add(pts.copyOfRange(2 * start, 2 * i))
        }
        return out
    }

    // ---------------- 打包 ----------------

    private class Buf {
        val b = java.io.ByteArrayOutputStream()
        fun u8(v: Int) { b.write(v and 0xFF) }
        fun u16(v: Int) { u8(v); u8(v shr 8) }
        fun i32(v: Int) { u8(v); u8(v shr 8); u8(v shr 16); u8(v shr 24) }
        val size get() = b.size()
    }

    /**
     * 地图块：只保留方框（中心 cx,cy，半边长 half 米）附近的内容，简化后以中心为原点、1 米为单位打包。
     * 格式：u16 坐标系编号, i32 原点 x, i32 原点 y（米）, u16 要素数,
     *       每个要素：u8 类别（面 +0x80）, u16 点数, 点数 × (i16 x, i16 y)
     * 超过 maxBytes 时依次去掉辅路、支路，保证圆屏放得下。
     */
    fun encodeChunk(frameId: Int, cx: Double, cy: Double, half: Double,
                    feats: List<Feature>, maxBytes: Int, maxFeat: Int): ByteArray {
        val ox = cx.roundToInt(); val oy = cy.roundToInt()
        val m = 150.0
        val bx0 = cx - half - m; val bx1 = cx + half + m
        val by0 = cy - half - m; val by1 = cy + half + m
        // 先处理成整数点
        class Q(val cls: Int, val poly: Boolean, val q: IntArray)
        val all = ArrayList<Q>()
        for (f in feats) {
            val pieces: List<DoubleArray> = if (f.poly) {
                var minX = Double.MAX_VALUE; var maxX = -Double.MAX_VALUE
                var minY = Double.MAX_VALUE; var maxY = -Double.MAX_VALUE
                for (i in 0 until f.pts.size / 2) {
                    minX = minOf(minX, f.pts[2 * i]); maxX = maxOf(maxX, f.pts[2 * i])
                    minY = minOf(minY, f.pts[2 * i + 1]); maxY = maxOf(maxY, f.pts[2 * i + 1])
                }
                if (maxX < bx0 || minX > bx1 || maxY < by0 || minY > by1) emptyList() else listOf(f.pts)
            } else clipLine(f.pts, bx0, by0, bx1, by1)
            for (p in pieces) {
                val s = simplify(p, if (f.poly) 3.0 else 1.5)
                val n = s.size / 2
                val q = ArrayList<Int>()
                var lx = Int.MIN_VALUE; var ly = Int.MIN_VALUE
                for (i in 0 until n) {
                    val x = (s[2 * i] - ox).roundToInt().coerceIn(-32000, 32000)
                    val y = (s[2 * i + 1] - oy).roundToInt().coerceIn(-32000, 32000)
                    if (x == lx && y == ly) continue
                    q.add(x); q.add(y); lx = x; ly = y
                }
                val np = q.size / 2
                if (np < (if (f.poly) 3 else 2)) continue
                // 单个要素点数上限（超长的切成几段）
                var st = 0
                while (st < np - 1) {
                    val en = minOf(np, st + 4000)
                    all.add(Q(f.cls, f.poly, q.subList(2 * st, 2 * en).toIntArray()))
                    if (f.poly) break
                    st = en - 1
                }
            }
        }
        // 面先画（在底下），然后道路从细到粗
        all.sortWith(compareBy<Q>({ if (it.poly) 0 else 1 }, { -it.cls }))
        fun size(list: List<Q>) = 12 + list.sumOf { 3 + it.q.size * 2 }
        var list: List<Q> = all
        for (drop in intArrayOf(5, 4, 3)) {
            if (size(list) <= maxBytes && list.size <= maxFeat) break
            list = list.filter { it.poly || it.cls < drop }
        }
        while ((size(list) > maxBytes || list.size > maxFeat) && list.isNotEmpty()) list = list.dropLast(1)
        val b = Buf()
        b.u16(frameId); b.i32(ox); b.i32(oy); b.u16(list.size)
        for (f in list) {
            b.u8(f.cls or (if (f.poly) 0x80 else 0))
            b.u16(f.q.size / 2)
            for (v in f.q) b.u16(v)
        }
        return b.b.toByteArray()
    }

    /**
     * 导航路线：u16 坐标系编号, i32 起点 x, i32 起点 y（米）, u16 点数, 之后每点 (i16 dx, i16 dy) 相对上一点
     */
    /**
     * v3.3 lights：路线上红绿灯的平面坐标（x, y 成对，米），可为 null。
     * 路线点后面追加：u16 灯数，每个灯 i32 x, i32 y（米）, u16 所在段号, u8 段内比例（0~255），按路线先后排序。
     */
    fun encodeRoute(frameId: Int, pts: DoubleArray, maxPts: Int, lights: DoubleArray? = null): ByteArray {
        var s = simplify(pts, 2.0)
        var tol = 2.0
        while (s.size / 2 > maxPts && tol < 50) { tol *= 1.6; s = simplify(pts, tol) }
        val q = ArrayList<Int>()
        var lx = 0; var ly = 0
        for (i in 0 until s.size / 2) {
            val x = s[2 * i].roundToInt(); val y = s[2 * i + 1].roundToInt()
            if (q.isNotEmpty()) {
                // 两点相距太远（超过 30 公里）时插入中间点，保证差值放得进 16 位
                var dx = x - lx; var dy = y - ly
                while (abs(dx) > 30000 || abs(dy) > 30000) {
                    val sx = dx.coerceIn(-30000, 30000); val sy = dy.coerceIn(-30000, 30000)
                    lx += sx; ly += sy; q.add(lx); q.add(ly)
                    dx = x - lx; dy = y - ly
                }
                if (x == lx && y == ly) continue
            }
            q.add(x); q.add(y); lx = x; ly = y
        }
        val n = minOf(q.size / 2, 65535)
        val b = Buf()
        b.u16(frameId)
        if (n == 0) { b.i32(0); b.i32(0); b.u16(0); return b.b.toByteArray() }
        b.i32(q[0]); b.i32(q[1]); b.u16(n)
        for (i in 1 until n) { b.u16(q[2 * i] - q[2 * i - 2]); b.u16(q[2 * i + 1] - q[2 * i - 1]) }
        // v3.3 红绿灯：找到离它最近的路线段，记下段号和段内位置（圆屏用来判断开没开过）
        if (lights != null && lights.size >= 2 && n >= 2) {
            val found = ArrayList<DoubleArray>()          // x, y, 段号 + 比例
            for (li in 0 until lights.size / 2) {
                val lx = lights[2 * li]; val ly = lights[2 * li + 1]
                var best = Double.MAX_VALUE; var bestU = 0.0
                for (k in 0 until n - 1) {
                    val ax = q[2 * k].toDouble(); val ay = q[2 * k + 1].toDouble()
                    val vx = q[2 * k + 2] - ax; val vy = q[2 * k + 3] - ay
                    val l2 = vx * vx + vy * vy
                    val t = if (l2 > 0) (((lx - ax) * vx + (ly - ay) * vy) / l2).coerceIn(0.0, 1.0) else 0.0
                    val ex = ax + vx * t - lx; val ey = ay + vy * t - ly
                    val d2 = ex * ex + ey * ey
                    if (d2 < best) { best = d2; bestU = k + t }
                }
                if (best <= 40.0 * 40.0) found.add(doubleArrayOf(lx, ly, bestU))
            }
            found.sortBy { it[2] }
            val cnt = minOf(found.size, 600)
            b.u16(cnt)
            for (i in 0 until cnt) {
                val f = found[i]
                val seg = minOf(f[2].toInt(), 65535)
                b.i32(f[0].roundToInt()); b.i32(f[1].roundToInt())
                b.u16(seg); b.u8(((f[2] - seg) * 255).roundToInt().coerceIn(0, 255))
            }
        }
        return b.b.toByteArray()
    }

    // ---------------- v2.3 高德图片地图 ----------------
    // 颜色类别（圆屏按同样编号上色）：0 街区，1 道路，2 绿地，3 水面
    const val PX_BLOCK = 0
    const val PX_ROAD = 1
    const val PX_PARK = 2
    const val PX_WATER = 3

    private fun lum(c: Int) = (((c shr 16) and 255) * 30 + ((c shr 8) and 255) * 59 + (c and 255) * 11) / 100

    /** 背景亮度：低饱和度像素里出现最多的亮度（就是街区底色） */
    fun backgroundLum(px: IntArray): Int {
        val hist = IntArray(256)
        var i = 0
        while (i < px.size) {
            val c = px[i]
            val r = (c shr 16) and 255; val g = (c shr 8) and 255; val b = c and 255
            val mx = maxOf(r, maxOf(g, b)); val mn = minOf(r, minOf(g, b))
            if (mx > 0 && (mx - mn) * 100 / mx < 8) hist[lum(c)]++
            i += 3
        }
        var best = 240
        for (l in 0..255) if (hist[l] > hist[best]) best = l
        return best
    }

    /** 把高德底图的一个像素颜色归类 */
    fun classifyPixel(c: Int, bgLum: Int): Int {
        val r = (c shr 16) and 255; val g = (c shr 8) and 255; val b = c and 255
        val mx = maxOf(r, maxOf(g, b)); val mn = minOf(r, minOf(g, b))
        val sat = if (mx == 0) 0.0 else (mx - mn).toDouble() / mx
        var hue = 0.0
        if (mx != mn) {
            val d = (mx - mn).toDouble()
            hue = when (mx) {
                r -> 60.0 * (((g - b) / d) % 6)
                g -> 60.0 * ((b - r) / d + 2)
                else -> 60.0 * ((r - g) / d + 4)
            }
            if (hue < 0) hue += 360.0
        }
        if (sat > 0.12 && hue in 180.0..255.0) return PX_WATER
        if (sat > 0.10 && hue in 60.0..170.0) return PX_PARK
        if (sat > 0.18 && hue in 10.0..60.0) return PX_ROAD          // 黄色、橙色的主干道
        val l = lum(c)
        if (l >= bgLum + 6) return PX_ROAD                            // 比底色亮的白色道路
        // v2.3.2 不再把深色细线（楼块轮廓、图标、描边）当成道路，它们是画面“脏”的主要来源
        return PX_BLOCK
    }

    /**
     * v2.3.2 清理杂点，让画面干净：
     * 1. 去掉孤立的小块（小于 minSize 像素的道路 / 绿地 / 水面碎块，多半是图标、楼块、文字残留）；
     * 2. 3×3 多数投票平滑一遍，去掉锯齿边上的零星像素。
     */
    fun cleanRaster(idx: ByteArray, w: Int, h: Int, minRoad: Int = 60, minArea: Int = 40): ByteArray {
        val out = idx.copyOf()
        val seen = BooleanArray(w * h)
        val stack = IntArray(w * h)
        val comp = IntArray(w * h)
        for (start in 0 until w * h) {
            val c = out[start].toInt()
            if (c == PX_BLOCK || seen[start]) continue
            // 找出这一整块（上下左右相连、同一颜色）
            var sp = 0; var cn = 0
            stack[sp++] = start; seen[start] = true
            while (sp > 0) {
                val k = stack[--sp]
                comp[cn++] = k
                val x = k % w; val y = k / w
                if (x > 0 && !seen[k - 1] && out[k - 1].toInt() == c) { seen[k - 1] = true; stack[sp++] = k - 1 }
                if (x < w - 1 && !seen[k + 1] && out[k + 1].toInt() == c) { seen[k + 1] = true; stack[sp++] = k + 1 }
                if (y > 0 && !seen[k - w] && out[k - w].toInt() == c) { seen[k - w] = true; stack[sp++] = k - w }
                if (y < h - 1 && !seen[k + w] && out[k + w].toInt() == c) { seen[k + w] = true; stack[sp++] = k + w }
            }
            val limit = if (c == PX_ROAD) minRoad else minArea
            if (cn < limit) for (i in 0 until cn) out[comp[i]] = PX_BLOCK.toByte()
        }
        // 3×3 多数投票
        val sm = out.copyOf()
        val cnt = IntArray(4)
        for (y in 1 until h - 1) for (x in 1 until w - 1) {
            cnt.fill(0)
            for (dy in -1..1) for (dx in -1..1) cnt[out[(y + dy) * w + x + dx].toInt() and 3]++
            var best = out[y * w + x].toInt() and 3
            for (c in 0..3) if (cnt[c] > cnt[best]) best = c
            if (cnt[best] >= 5) sm[y * w + x] = best.toByte()
        }
        return sm
    }

    /** 道路加粗一圈（只往街区里扩），细路在圆屏上更清楚 */
    fun dilateRoads(idx: ByteArray, w: Int, h: Int): ByteArray {
        val out = idx.copyOf()
        for (y in 1 until h - 1) {
            val r = y * w
            for (x in 1 until w - 1) {
                val k = r + x
                if (idx[k].toInt() != PX_BLOCK) continue
                if (idx[k - 1].toInt() == PX_ROAD || idx[k + 1].toInt() == PX_ROAD ||
                    idx[k - w].toInt() == PX_ROAD || idx[k + w].toInt() == PX_ROAD) out[k] = PX_ROAD.toByte()
            }
        }
        return out
    }

    /**
     * 图片地图打包：u16 坐标系编号, i32 左上角 x, i32 左上角 y（米）, u16 宽, u16 高, u16 每像素毫米数,
     * 然后是按行压缩的颜色编号：每字节 低 4 位 = 颜色，高 4 位 n：n < 15 时连续 n+1 个；
     * n = 15 时连续 16 + 下一个字节 个。
     */
    /** pal（v5.9 卫星图）：16 个 RGB565 颜色；有调色板时“每像素毫米数”最高位置 1，后面跟 32 字节调色板 */
    fun encodeRaster(frameId: Int, x0: Int, y0: Int, w: Int, h: Int, mmPerPx: Int, idx: ByteArray, pal: IntArray? = null): ByteArray {
        val b = java.io.ByteArrayOutputStream(w * h / 6 + 64)
        fun u8(v: Int) = b.write(v and 0xFF)
        fun u16(v: Int) { u8(v); u8(v shr 8) }
        fun i32(v: Int) { u8(v); u8(v shr 8); u8(v shr 16); u8(v shr 24) }
        u16(frameId); i32(x0); i32(y0); u16(w); u16(h); u16(if (pal != null) (mmPerPx or 0x8000) else mmPerPx)
        if (pal != null) for (i in 0 until 16) u16(if (i < pal.size) pal[i] else 0)
        var i = 0
        val n = w * h
        while (i < n) {
            val c = idx[i].toInt() and 15
            var run = 1
            while (i + run < n && run < 271 && (idx[i + run].toInt() and 15) == c) run++
            if (run <= 15) u8(((run - 1) shl 4) or c)
            else { u8((15 shl 4) or c); u8(run - 16) }
            i += run
        }
        return b.toByteArray()
    }
}
