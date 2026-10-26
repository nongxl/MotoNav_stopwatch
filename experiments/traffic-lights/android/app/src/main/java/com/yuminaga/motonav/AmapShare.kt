package com.yuminaga.motonav

import android.net.Uri
import com.amap.api.navi.model.NaviLatLng
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import java.net.URLEncoder
import java.util.UUID
import kotlin.math.abs
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sqrt

/**
 * 版本：跟随 App 版本，见 MainActivity.kt 里的 APP_VERSION（v4.4 新增；v4.4.1 http 跳转改成 https；v4.4.2 支持摩托车路线、callAPP 跳转页；v4.4.4 认 shorturl&r= 格式）
 *
 * 导入高德 App 分享的路线（比如在高德里规划好的摩托车路线）：
 * 1. 从分享文字里找出短链接（surl.amap.com/…），按手机浏览器的方式一路跟着跳转，
 *    找到链接里的起点、终点和加密的路线参数 shareParam
 * 2. 用高德手机网页自己用的接口（m.amap.com/service/navigation/driving.json）把这条路线原样还原出来：
 *    整条路线的坐标、每条路的名字和长度
 * 3. 从路线上挑十几个“途经点”（每条路的中间，长路多挑几个），交给高德导航 SDK 规划，
 *    这样 SDK 算出的路线基本和分享的路线走同一批路，原来的导航功能（转向、车道、摄像头、语音、偏航重算）都能用
 *
 * 注意：第 2 步用的是高德网页内部接口，不是公开接口，高德改了就会失效（到时候要跟着改）；仅供个人使用。
 */
object AmapShare {
    @Volatile var info = "还未导入分享路线"
        private set
    private const val UA = "Mozilla/5.0 (Linux; Android 14; Pixel 7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0 Mobile Safari/537.36"
    private const val MAX_WAYPOINTS = 15        // 高德 SDK 最多 16 个途经点，留一个余量

    class Road(val name: String, val length: Int, val pts: List<DoubleArray>)   // pts: [纬度, 经度]

    class Route(
        val startName: String, val startLat: Double, val startLng: Double,
        val endName: String, val endLat: Double, val endLng: Double,
        val pts: List<DoubleArray>,             // 整条路线 [纬度, 经度]
        val roads: List<Road>,
        val distance: Int, val timeS: Int, val lights: Int,
        val restored: Boolean,                  // true = 高德原样还原了分享的路线
        val kind: String = "驾车",
        val endpointsOnly: Boolean = false     // 仅含起终点或原路线不可读时，不声称原路还原
    ) {
        var sourceUrl: String = ""
            internal set
        var nativeUri: String = ""
            internal set
        var importNote: String = ""
            internal set
        val isTravel: Boolean get() = kind == "自行车"
        /** 每个点离起点的路程（米） */
        val cum: DoubleArray by lazy {
            val c = DoubleArray(pts.size)
            for (i in 1 until pts.size) c[i] = c[i - 1] + distM(pts[i - 1][0], pts[i - 1][1], pts[i][0], pts[i][1])
            c
        }
        val total: Double get() = if (cum.isEmpty()) 0.0 else cum[cum.size - 1]
    }

    /** 分享文字里有没有高德链接 */
    fun findUrl(text: String?): String? {
        if (text.isNullOrBlank()) return null
        return Regex("""https?://[A-Za-z0-9.\-]*amap\.com/[^\s，。、"'<>）)]+""").find(text)?.value
    }

    /** 在后台线程里调用：读出分享的路线；失败时抛出带中文说明的异常 */
    fun load(text: String): Route {
        try {
            info = "正在解析高德分享链接"
            val url = findUrl(text) ?: throw Exception("没找到高德的分享链接")
            val p = resolve(url) ?: throw Exception("链接里没找到路线信息（可能不是路线分享，或者链接已过期）\n最后打开的是：${lastUrl.take(1500)}")
            val route = when {
                p.mode.contains("motor") -> loadMotor(p)
                p.mode.contains("ride") -> loadRide(p)
                p.mode.isEmpty() || p.mode.contains("car") -> loadCar(p)
                else -> throw Exception("暂不支持这类分享路线：${p.mode}")
            }
            route.sourceUrl = url
            route.nativeUri = p.nativeUri
            info = if (route.endpointsOnly) "已读取${route.kind}起终点，需重新规划"
                else "已还原${route.kind}路线：${route.pts.size} 个坐标点"
            android.util.Log.i("MotoNavShare", info)
            return route
        } catch (error: Exception) {
            info = error.message?.lineSequence()?.firstOrNull() ?: "分享路线导入失败"
            android.util.Log.w("MotoNavShare", info)
            throw error
        }
    }

    private fun checkRouteResponse(json: JSONObject, kind: String) {
        if (json.optInt("code") == 1) return
        val message = listOf("message", "msg", "info").firstNotNullOfOrNull { name ->
            json.optString(name).takeIf { it.isNotBlank() && it != "null" }
        } ?: "服务未说明原因"
        throw Exception("高德未能还原${kind}原路线（${json.optInt("code")}）：${message.take(200)}\n请重新在高德中分享这条路线后重试。")
    }

    /** v4.4.2 摩托车路线：m.amap.com/service/navigation/motor（数据里有多条备选路线，取分享时选中的那条） */
    private fun loadMotor(p: Share): Route {
        val q = "https://m.amap.com/service/navigation/motor?shareParam=${URLEncoder.encode(p.share, "UTF-8")}&uuid=${UUID.randomUUID()}"
        val j = JSONObject(get(q).second)
        checkRouteResponse(j, "摩托车")
        val data = j.optJSONObject("data") ?: throw Exception("高德返回的数据里没有路线")
        val paths = data.optJSONArray("paths") ?: throw Exception("高德返回的数据里没有路线")
        if (paths.length() == 0) throw Exception("高德返回的路线是空的")
        val pl = paths.optJSONObject(if (p.pathIdx in 0 until paths.length()) p.pathIdx else 0)
            ?: throw Exception("高德返回的数据里没有路线")
        val all = ArrayList<DoubleArray>()
        val co = pl.optJSONArray("coords")
        if (co != null) for (i in 0 until co.length()) {
            val c = co.optJSONObject(i) ?: continue
            all.add(doubleArrayOf(c.optDouble("lat"), c.optDouble("lon")))
        }
        if (all.size < 2) throw Exception("路线是空的")
        // 每条路（group）= 连续几个 segment；segment 里的 link 记着在 coords 里的起止下标
        val segs = pl.optJSONArray("segments")
        fun segRange(si: Int): IntArray? {
            val links = segs?.optJSONObject(si)?.optJSONArray("links") ?: return null
            if (links.length() == 0) return null
            val b = links.optJSONObject(0)?.optInt("begin_index", -1) ?: -1
            val e = links.optJSONObject(links.length() - 1)?.optInt("end_index", -1) ?: -1
            return if (b >= 0 && e >= b) intArrayOf(b, e) else null
        }
        val roads = ArrayList<Road>()
        val gr = pl.optJSONArray("group")
        if (gr != null) for (i in 0 until gr.length()) {
            val g = gr.optJSONObject(i) ?: continue
            val s0 = g.optInt("start_id", -1); val n = g.optInt("count", 1)
            val a = segRange(s0) ?: continue
            val b = segRange(s0 + n - 1) ?: a
            val from = a[0].coerceIn(0, all.size - 1); val to = b[1].coerceIn(from, all.size - 1)
            roads.add(Road(g.optString("name"), g.optInt("dist"), all.subList(from, to + 1).toList()))
        }
        return Route(p.sName, p.sLat, p.sLng, p.eName, p.eLat, p.eLng, dedup(all), roads,
            pl.optInt("distance"), pl.optInt("travel_time"), pl.optInt("traffic_light_count"),
            j.optBoolean("isRestoreRoute", false), "摩托车")
    }

    /** 驾车路线：m.amap.com/service/navigation/driving.json */
    private fun loadCar(p: Share): Route {
        val q = "https://m.amap.com/service/navigation/driving.json?origin=${p.sLng},${p.sLat}&originid=&origintype=" +
            "&destination=${p.eLng},${p.eLat}&destinationid=&destinationtype=&waypoints=&strategy=10" +
            "&shareParam=${URLEncoder.encode(p.share, "UTF-8")}&uuid=${UUID.randomUUID()}"
        val body = try { get(q).second } catch (error: WebVerificationRequired) {
            return Route(p.sName, p.sLat, p.sLng, p.eName, p.eLat, p.eLng,
                listOf(doubleArrayOf(p.sLat, p.sLng), doubleArrayOf(p.eLat, p.eLng)), emptyList(),
                0, 0, 0, false, "驾车", endpointsOnly = true).apply {
                importNote = "高德网页要求安全验证，暂时无法读取原路线。可导入起终点重新规划驾车路线，但可能与原先选中的路线不同。"
            }
        }
        val j = JSONObject(body)
        checkRouteResponse(j, "驾车")
        val paths = j.optJSONArray("path_list") ?: throw Exception("高德返回的数据里没有路线")
        val pl = paths.optJSONObject(if (p.pathIdx in 0 until paths.length()) p.pathIdx else 0)
            ?: throw Exception("高德返回的数据里没有路线")
        val roads = ArrayList<Road>()
        val all = ArrayList<DoubleArray>()
        val pa = pl.optJSONArray("path")
        if (pa != null) for (i in 0 until pa.length()) {
            val r = pa.optJSONObject(i) ?: continue
            val rp = ArrayList<DoubleArray>()
            val segs = r.optJSONArray("segments")
            if (segs != null) for (k in 0 until segs.length()) {
                val co = segs.optJSONObject(k)?.optJSONArray("coor") ?: continue
                for (m in 0 until co.length()) {
                    val c = co.optJSONArray(m) ?: continue
                    rp.add(doubleArrayOf(c.optDouble(1), c.optDouble(0)))
                }
            }
            roads.add(Road(r.optString("name"), r.optInt("length"), rp))
        }
        val one = pl.optJSONArray("oneRoute")
        if (one != null) for (i in 0 until one.length()) {
            val c = one.optJSONArray(i) ?: continue
            all.add(doubleArrayOf(c.optDouble(1), c.optDouble(0)))
        }
        if (all.isEmpty()) roads.forEach { all.addAll(it.pts) }
        if (all.size < 2) throw Exception("路线是空的")
        val lights = pl.optInt("traffic_lights", pl.optInt("trafficlights", 0))
        return Route(p.sName, p.sLat, p.sLng, p.eName, p.eLat, p.eLng, dedup(all), roads,
            pl.optInt("distance"), pl.optInt("drivetime"), lights, j.optBoolean("isRestoreRoute", false), "驾车")
    }

    // ---------------- 解析分享链接 ----------------

    private fun loadRide(p: Share): Route {
        if (p.rideType == "elebike") throw Exception("暂不支持电瓶车分享，请选择驾车、自行车或摩托车路线")
        val kind = "自行车"
        val q = "https://m.amap.com/service/navigation/ride.json?origin=${p.sLng},${p.sLat}" +
            "&destination=${p.eLng},${p.eLat}&rideType=bike" +
            "&shareParam=${URLEncoder.encode(p.share, "UTF-8")}&uuid=${UUID.randomUUID()}"
        val j = JSONObject(get(q).second)
        checkRouteResponse(j, kind)
        val paths = j.optJSONObject("data")?.optJSONArray("paths") ?: throw Exception("高德返回的数据里没有骑行路线")
        val path = paths.optJSONObject(if (p.pathIdx in 0 until paths.length()) p.pathIdx else 0)
            ?: throw Exception("高德返回的骑行路线是空的")
        val all = ArrayList<DoubleArray>()
        val roads = ArrayList<Road>()
        val steps = path.optJSONArray("steps")
        if (steps != null) for (i in 0 until steps.length()) {
            val step = steps.optJSONObject(i) ?: continue
            val points = step.optString("polyline").split(';').mapNotNull { value ->
                val parts = value.split(',')
                val lng = parts.getOrNull(0)?.toDoubleOrNull()
                val lat = parts.getOrNull(1)?.toDoubleOrNull()
                if (lat != null && lng != null && lat in -90.0..90.0 && lng in -180.0..180.0)
                    doubleArrayOf(lat, lng) else null
            }
            all.addAll(points)
            roads.add(Road(step.optString("road"), step.optInt("distance"), points))
        }
        val points = dedup(all)
        if (points.size < 2) throw Exception("高德返回的骑行路线没有有效坐标")
        return Route(p.sName, p.sLat, p.sLng, p.eName, p.eLat, p.eLng, points, roads,
            path.optInt("distance"), path.optInt("duration"), 0, j.optBoolean("isRestoreRoute"), kind, p.endpointsOnly)
    }

    /** mode = 网址里的 navigation/xxx（carmap 驾车、motormap 摩托车……），pathIdx = 分享时选中的第几条路线 */
    private class Share(val sLat: Double, val sLng: Double, val sName: String,
                        val eLat: Double, val eLng: Double, val eName: String, val share: String,
                        val mode: String, val pathIdx: Int, val rideType: String = "bike",
                        val endpointsOnly: Boolean = false) {
        var nativeUri: String = ""
    }

    private fun nativeLink(url: String): String = try {
        val uri = Uri.parse(url)
        val schema = uri.getQueryParameter("schema") ?: ""
        val android = uri.getQueryParameter("android") ?: ""
        when {
            schema.startsWith("amapuri://route/plan?") -> schema
            android.startsWith("androidamap?action=") -> android.replaceFirst("?", "://")
                .replaceFirst("action=", "").replaceFirst("&", "?")
            else -> ""
        }
    } catch (_: Exception) { "" }

    private var lastUrl = ""                    // 调试用：最后打开的网址

    /** 跟着跳转一路找，直到某个网址或网页内容里出现路线信息 */
    private fun resolve(start: String): Share? {
        var url = start
        val seen = HashSet<String>()
        repeat(10) {
            if (!seen.add(url)) return null
            lastUrl = url
            parse(url)?.let { it.nativeUri = nativeLink(url); return it }
            val (loc, body) = get(url, follow = false)
            if (loc != null) { url = URL(URL(url), loc).toString(); return@repeat }
            parse(body)?.let { return it }
            // 网页用脚本跳转：找里面的高德网址
            val next = Regex("""(?:https?:)?//m\.amap\.com/navigation/[^"'\s<>\\]+""").find(body)?.value
                ?: Regex("""location(?:\.href)?\s*=\s*["']([^"']+)["']""").find(body)?.groupValues?.get(1)
                ?: Regex("""http-equiv=["']refresh["'][^>]*url=([^"'>\s]+)""", RegexOption.IGNORE_CASE).find(body)?.groupValues?.get(1)
                ?: return null
            url = URL(URL(url), if (next.startsWith("//")) "https:$next" else next).toString()
        }
        return null
    }

    /**
     * 从一段文字（网址或网页）里找路线信息（可能被编码了好几层，一层层解开找）：
     * 1. v4.4.2 优先找 navigation/<类型>/__r=起点纬度,经度,名字,终点纬度,经度,名字,?,类型,第几条,,,,加密参数,...
     *    v4.4.4 也认 callAPP 跳转页里 android= 参数中的 action=shorturl&r=…（格式一样，类型 11 = 摩托车）
     * 2. 再找 shareParam=…&saddr=…&daddr=…
     */
    private fun parse(raw: String): Share? {
        var s = raw
        for (round in 0 until 5) {
            // 原生规划链接只含起终点。电瓶车明确拒绝，避免按自行车或驾车误导入。
            val nested = Regex("""[?&]schema=([^&"'\s<>]+)""").find(s)?.groupValues?.get(1)
            val native = if (nested != null) fullDecode(nested) else s
            if (native.startsWith("amapuri://route/plan?") || native.startsWith("androidamap://route?")) {
                val uri = Uri.parse(native)
                val rideType = uri.getQueryParameter("rideType")
                if (rideType == "elebike") throw Exception("暂不支持电瓶车分享，请选择驾车、自行车或摩托车路线")
                if (uri.getQueryParameter("t") == "3" && rideType in listOf("bike", null)) {
                    if (uri.getQueryParameter("dev")?.let { it != "0" } == true)
                        throw Exception("这条分享使用 GPS 坐标，请从高德 App 重新分享路线")
                    if ((uri.getQueryParameter("vian")?.toIntOrNull() ?: 0) > 0)
                        throw Exception("这条骑行分享含途经点，请重新分享完整路线链接")
                    val slat = uri.getQueryParameter("slat")?.toDoubleOrNull()
                    val slng = uri.getQueryParameter("slon")?.toDoubleOrNull()
                    val elat = uri.getQueryParameter("dlat")?.toDoubleOrNull()
                    val elng = uri.getQueryParameter("dlon")?.toDoubleOrNull()
                    if (slat != null && slng != null && elat != null && elng != null &&
                        slat in -90.0..90.0 && elat in -90.0..90.0 && slng in -180.0..180.0 && elng in -180.0..180.0)
                        return Share(slat, slng, uri.getQueryParameter("sname") ?: "分享起点",
                            elat, elng, uri.getQueryParameter("dname") ?: "分享目的地", "", "ridemap", 0,
                            rideType ?: "bike", endpointsOnly = true)
                }
            }
            // __r=…（路线页网址）或 action=shorturl&r=…（callAPP 跳转页里给安卓高德 App 的参数），两种格式一样
            for (m in Regex("""(?:__r|[?&]r)=([^&"'\s<>]+)""").findAll(s)) {
                val f = m.groupValues[1].split(',')
                if (f.size < 13) continue
                val sLat = fullDecode(f[0]).toDoubleOrNull(); val sLng = fullDecode(f[1]).toDoubleOrNull()
                val eLat = fullDecode(f[3]).toDoubleOrNull(); val eLng = fullDecode(f[4]).toDoubleOrNull()
                val share = fullDecode(f[12])
                if (sLat == null || sLng == null || eLat == null || eLng == null || share.length <= 10) continue
                // 路线类型：网址里的 navigation/xxx；没有就看第 8 项（11 = 摩托车）或 t= 参数
                val mode = Regex("""navigation/([A-Za-z]+)/""").find(s)?.groupValues?.get(1)?.lowercase()
                    ?: when (fullDecode(f[7])) { "11" -> "motormap"; "3" -> "ridemap"; "0" -> "carmap"; else -> "unsupported" }
                return Share(sLat, sLng, fullDecode(f[2]), eLat, eLng, fullDecode(f[5]), share,
                    mode, fullDecode(f[8]).toIntOrNull() ?: 0,
                    if (Regex("""rideType=elebike(?:&|$)""").containsMatchIn(s)) "elebike" else "bike")
            }
            val sp = Regex("""shareParam=([^&"'\s,<>]+)""").find(s)?.groupValues?.get(1)
            if (sp != null) {
                val share = fullDecode(sp)
                val sa = Regex("""saddr=([^&"'\s<>]+)""").find(s)?.groupValues?.get(1)
                val da = Regex("""daddr=([^&"'\s<>]+)""").find(s)?.groupValues?.get(1)
                val a = sa?.let { lngLatName(fullDecode(it)) }
                val b = da?.let { lngLatName(fullDecode(it)) }
                val mode = Regex("""navigation/([A-Za-z]+)/""").find(s)?.groupValues?.get(1)?.lowercase() ?: ""
                if (a != null && b != null) return Share(a.first, a.second, a.third, b.first, b.second, b.third, share, mode, 0,
                    if (Regex("""rideType=elebike(?:&|$)""").containsMatchIn(s)) "elebike" else "bike")
            }
            val d: String = Uri.decode(s) ?: s
            if (d == s) break
            s = d
        }
        return null
    }

    /** "经度,纬度,名字" → (纬度, 经度, 名字) */
    private fun lngLatName(s: String): Triple<Double, Double, String>? {
        val p = s.split(',')
        if (p.size < 2) return null
        val lng = p[0].toDoubleOrNull() ?: return null
        val lat = p[1].toDoubleOrNull() ?: return null
        return Triple(lat, lng, p.drop(2).joinToString(","))
    }

    private fun fullDecode(s: String): String {
        var t = s
        repeat(4) { val d: String = Uri.decode(t) ?: t; if (d == t) return t; t = d }
        return t
    }

    /** 读一个网址：返回 (跳转地址, 网页内容) */
    private fun get(url: String, follow: Boolean = true): Pair<String?, String> {
        // v4.4.1 高德跳转时会给 http:// 的地址，安卓不允许明文 http：统一换成 https（高德的网站都支持）
        val c = URL(if (url.startsWith("http://")) "https://" + url.substring(7) else url).openConnection() as HttpURLConnection
        try {
            c.instanceFollowRedirects = follow
            c.connectTimeout = 10_000; c.readTimeout = 15_000
            c.setRequestProperty("User-Agent", UA)
            c.setRequestProperty("Accept-Language", "zh-CN,zh;q=0.9")
            val code = c.responseCode
            if (!follow && code in 300..399) return c.getHeaderField("Location") to ""
            if (code >= 400) throw Exception("网络请求失败（$code）")
            val text = c.inputStream.bufferedReader(Charsets.UTF_8).use { it.readText() }
            if (url.contains("/service/navigation/") && !text.trimStart().startsWith("{")) {
                if (text.contains("punish-component") || text.contains("AWSC/CAPTCHA"))
                    throw WebVerificationRequired()
                throw Exception("高德路线服务返回了网页，未返回路线数据，请稍后重试")
            }
            return null to text
        } finally {
            c.disconnect()
        }
    }

    private class WebVerificationRequired : Exception("高德要求网页安全验证，暂时不能读取这条分享原路线。请在高德中打开分享，稍后再试。")

    private fun dedup(l: List<DoubleArray>): List<DoubleArray> {
        val out = ArrayList<DoubleArray>(l.size)
        for (p in l) {
            val q = out.lastOrNull()
            if (q == null || abs(q[0] - p[0]) > 1e-7 || abs(q[1] - p[1]) > 1e-7) out.add(p)
        }
        return out
    }

    // ---------------- 挑途经点 ----------------

    /**
     * 从当前位置往后，在每条路的中间放一个途经点（长路每 7 公里多放一个）；离起点、终点太近的不要；
     * 超过 15 个时，去掉和前后挨得最近的那些，让剩下的分布均匀
     */
    fun waypoints(r: Route, myLat: Double, myLng: Double): List<NaviLatLng> =
        waypointsAfter(r, if (myLat.isNaN()) 0.0 else progressOf(r, myLat, myLng))

    /** v4.5 只要路程 from 米以后的途经点（偏航后重新规划用） */
    fun waypointsAfter(r: Route, from: Double): List<NaviLatLng> {
        val end = r.total
        class Wp(val d: Double, val lat: Double, val lng: Double)
        val list = ArrayList<Wp>()
        var base = 0.0                                       // 这条路起点在整条路线上的路程
        for (road in r.roads) {
            val rc = DoubleArray(road.pts.size)
            for (i in 1 until road.pts.size) rc[i] = rc[i - 1] + distM(road.pts[i - 1][0], road.pts[i - 1][1], road.pts[i][0], road.pts[i][1])
            val len = if (rc.isEmpty()) 0.0 else rc[rc.size - 1]
            if (len >= 100) {
                val n = 1 + (len / 7000).toInt()
                for (k in 0 until n) {
                    val at = len * (k + 0.5) / n
                    val p = pointAt(road.pts, rc, at)
                    val g = base + at
                    if (g > from + 250 && g < end - 250) list.add(Wp(g, p[0], p[1]))
                }
            }
            base += len
        }
        list.sortBy { it.d }
        while (list.size > MAX_WAYPOINTS) {
            var bi = 0; var bv = Double.MAX_VALUE
            for (i in list.indices) {
                val prev = if (i > 0) list[i - 1].d else from
                val next = if (i < list.size - 1) list[i + 1].d else end
                val v = next - prev
                if (v < bv) { bv = v; bi = i }
            }
            list.removeAt(bi)
        }
        return list.map { NaviLatLng(it.lat, it.lng) }
    }

    /**
     * v4.5 偏航后找“回到原路线的点”：在已经走到的路程 minD 之后，离当前位置最近的那一点（不管多远），返回它的路程（米）
     */
    fun progressAhead(r: Route, lat: Double, lng: Double, minD: Double): Double {
        var best = Double.MAX_VALUE; var bd = max(0.0, minD)
        val kx = cos(Math.toRadians(lat)) * 111_320.0; val ky = 110_540.0
        for (i in 0 until r.pts.size - 1) {
            if (r.cum[i + 1] < minD) continue
            val ax = (r.pts[i][1] - lng) * kx; val ay = (r.pts[i][0] - lat) * ky
            val bx = (r.pts[i + 1][1] - lng) * kx; val by = (r.pts[i + 1][0] - lat) * ky
            val dx = bx - ax; val dy = by - ay
            val l2 = dx * dx + dy * dy
            val t = if (l2 > 0) ((-ax * dx - ay * dy) / l2).coerceIn(0.0, 1.0) else 0.0
            val px = ax + t * dx; val py = ay + t * dy
            val d = sqrt(px * px + py * py)
            val at = r.cum[i] + t * sqrt(l2)
            if (d < best && at >= minD) { best = d; bd = at }
        }
        return bd
    }

    /** v4.5 当前位置在原路线上（60 米以内）时返回走到的路程，否则 null */
    fun progressNear(r: Route, lat: Double, lng: Double, near: Double = 60.0): Double? {
        var best = Double.MAX_VALUE; var bd = 0.0
        val kx = cos(Math.toRadians(lat)) * 111_320.0; val ky = 110_540.0
        for (i in 0 until r.pts.size - 1) {
            val ax = (r.pts[i][1] - lng) * kx; val ay = (r.pts[i][0] - lat) * ky
            if (abs(ax) > 3000 || abs(ay) > 3000) continue                 // 粗筛：远的不算
            val bx = (r.pts[i + 1][1] - lng) * kx; val by = (r.pts[i + 1][0] - lat) * ky
            val dx = bx - ax; val dy = by - ay
            val l2 = dx * dx + dy * dy
            val t = if (l2 > 0) ((-ax * dx - ay * dy) / l2).coerceIn(0.0, 1.0) else 0.0
            val px = ax + t * dx; val py = ay + t * dy
            val d = sqrt(px * px + py * py)
            if (d < best) { best = d; bd = r.cum[i] + t * sqrt(l2) }
        }
        return if (best <= near) bd else null
    }

    /** 当前位置在路线上走到了哪里（米）；离路线超过 300 米就当作还没上路线（0） */
    fun progressOf(r: Route, lat: Double, lng: Double): Double {
        var best = Double.MAX_VALUE; var bd = 0.0
        val kx = cos(Math.toRadians(lat)) * 111_320.0; val ky = 110_540.0
        for (i in 0 until r.pts.size - 1) {
            val ax = (r.pts[i][1] - lng) * kx; val ay = (r.pts[i][0] - lat) * ky
            val bx = (r.pts[i + 1][1] - lng) * kx; val by = (r.pts[i + 1][0] - lat) * ky
            val dx = bx - ax; val dy = by - ay
            val l2 = dx * dx + dy * dy
            val t = if (l2 > 0) ((-ax * dx - ay * dy) / l2).coerceIn(0.0, 1.0) else 0.0
            val px = ax + t * dx; val py = ay + t * dy
            val d = sqrt(px * px + py * py)
            if (d < best) { best = d; bd = r.cum[i] + t * sqrt(l2) }
        }
        return if (best <= 300) bd else 0.0
    }

    /**
     * 高德 SDK 算出来的路线和分享的路线有多吻合：分享路线（从当前位置往后）每隔 40 米取一个点，
     * 看离 SDK 路线是否在 45 米以内。返回 (吻合比例 0~1, 不吻合的长度 米)
     */
    fun match(r: Route, sdk: List<NaviLatLng>, myLat: Double, myLng: Double): Pair<Double, Int> {
        if (sdk.size < 2) return 0.0 to r.total.toInt()
        val from = if (myLat.isNaN()) 0.0 else progressOf(r, myLat, myLng)
        val lat0 = r.pts[0][0]
        val kx = cos(Math.toRadians(lat0)) * 111_320.0; val ky = 110_540.0
        val sx = DoubleArray(sdk.size) { sdk[it].longitude * kx }
        val sy = DoubleArray(sdk.size) { sdk[it].latitude * ky }
        var ok = 0; var all = 0
        var d = from + 20
        var seg = 0
        while (d < r.total - 20) {
            while (seg < r.cum.size - 2 && r.cum[seg + 1] < d) seg++
            val p = pointAt(r.pts, r.cum, d, seg)
            val x = p[1] * kx; val y = p[0] * ky
            var near = false
            for (i in 0 until sdk.size - 1) {
                // 先粗筛
                if (min(sx[i], sx[i + 1]) - 45 > x || max(sx[i], sx[i + 1]) + 45 < x ||
                    min(sy[i], sy[i + 1]) - 45 > y || max(sy[i], sy[i + 1]) + 45 < y) continue
                val dx = sx[i + 1] - sx[i]; val dy = sy[i + 1] - sy[i]
                val l2 = dx * dx + dy * dy
                val t = if (l2 > 0) (((x - sx[i]) * dx + (y - sy[i]) * dy) / l2).coerceIn(0.0, 1.0) else 0.0
                val ex = sx[i] + t * dx - x; val ey = sy[i] + t * dy - y
                if (ex * ex + ey * ey <= 45.0 * 45.0) { near = true; break }
            }
            if (near) ok++
            all++
            d += 40
        }
        if (all == 0) return 1.0 to 0
        return ok.toDouble() / all to (all - ok) * 40
    }

    /** 折线上路程为 d 的点 */
    private fun pointAt(pts: List<DoubleArray>, cum: DoubleArray, d: Double, hint: Int = 0): DoubleArray {
        if (pts.isEmpty()) return doubleArrayOf(0.0, 0.0)
        var i = hint.coerceIn(0, max(0, pts.size - 2))
        while (i < pts.size - 2 && cum[i + 1] < d) i++
        if (pts.size == 1) return pts[0]
        val seg = cum[i + 1] - cum[i]
        val t = if (seg > 0) ((d - cum[i]) / seg).coerceIn(0.0, 1.0) else 0.0
        return doubleArrayOf(pts[i][0] + (pts[i + 1][0] - pts[i][0]) * t, pts[i][1] + (pts[i + 1][1] - pts[i][1]) * t)
    }

    fun distM(lat1: Double, lng1: Double, lat2: Double, lng2: Double): Double {
        val k = Math.PI / 180
        val x = (lng2 - lng1) * k * cos((lat1 + lat2) / 2 * k)
        val y = (lat2 - lat1) * k
        return sqrt(x * x + y * y) * 6_371_000.0
    }
}

/**
 * v4.5 按导入的分享路线导航时的“跟随”：
 * - 记住在原路线上已经走到哪里（只在原路线 60 米以内时更新，最多往前跳 3 公里，防止路线自己交叉时跳错）
 * - 高德判定偏航并自动重算后，马上自己再算一次：从当前位置出发，只用“回到原路线的点”之后的途经点，
 *   这样不会被拽回去补走已经跳过的途经点，而是就近回到原路线继续走
 * - 原路线发给圆屏画成细线（MapFeeder.setGhost），导航页地图上也画一条
 */
object ShareGuide {
    @Volatile var route: AmapShare.Route? = null
        private set
    var naviType = 1                            // 1 = 真实导航，2 = 模拟导航（NavActivity 设置）
    private var ctx: android.content.Context? = null
    private var avoidHw = true
    private var maxProg = 0.0
    private var lat = Double.NaN
    private var lng = Double.NaN
    private var yawAt = 0L                      // 高德开始偏航重算的时间
    private var ourCalc = false                 // 我们自己发起的重算还没回来
    private var lastOurAt = 0L
    private fun now() = android.os.SystemClock.elapsedRealtime()

    /** 开始导航时调用（主线程）；r = null 表示这次不是按分享路线导航 */
    fun start(r: AmapShare.Route?, context: android.content.Context, avoidHighway: Boolean, myLat: Double, myLng: Double) {
        route = r
        ctx = context.applicationContext
        avoidHw = avoidHighway
        lat = myLat; lng = myLng
        maxProg = if (r != null && !myLat.isNaN()) AmapShare.progressOf(r, myLat, myLng) else 0.0
        yawAt = 0L; ourCalc = false; lastOurAt = 0L
        MapFeeder.setGhost(r?.takeIf { !it.endpointsOnly }?.pts)
    }

    fun stop() {
        if (route == null) return
        route = null
        yawAt = 0L; ourCalc = false
        MapFeeder.setGhost(null)
    }

    /** 每次定位（主线程） */
    fun onLocation(la: Double, lo: Double) {
        lat = la; lng = lo
        val r = route ?: return
        val p = AmapShare.progressNear(r, la, lo) ?: return
        if (p > maxProg && p < maxProg + 3000) maxProg = p
    }

    /** 高德判定偏航，开始自己重算（主线程） */
    fun onYaw() {
        if (route == null || route?.endpointsOnly == true) return
        yawAt = now()
        NaviBridge.extraInfo = "偏航了：等高德重算完，再按分享的路线重新规划"
    }

    /** 每次算路结束（主线程）：成功/失败 */
    fun onCalcDone(ok: Boolean) {
        val r = route ?: return
        if (ourCalc) {
            ourCalc = false
            if (ok) {
                // 导航中算路成功：确保用新路线继续导航
                try { com.amap.api.navi.AMapNavi.getInstance(ctx).startNavi(naviType) } catch (_: Throwable) {}
                NaviBridge.extraInfo = "已按分享的路线重新规划（从原路线 ${(maxProg / 1000).let { "%.1f".format(it) }} 公里处往后）"
            } else {
                NaviBridge.extraInfo = "按分享的路线重新规划失败，先沿用高德的路线"
            }
            return
        }
        if (yawAt != 0L && now() - yawAt < 30_000) {
            yawAt = 0L
            reroute(r)
        }
    }

    private fun reroute(r: AmapShare.Route) {
        if (lat.isNaN() || now() - lastOurAt < 8000) return
        val c = ctx ?: return
        val n = try { com.amap.api.navi.AMapNavi.getInstance(c) } catch (_: Throwable) { return }
        val from = AmapShare.progressAhead(r, lat, lng, maxProg - 50)
        val wps = ArrayList(AmapShare.waypointsAfter(r, from))
        val strategy = try { n.strategyConvert(false, avoidHw, false, false, false) } catch (_: Exception) { 10 }
        lastOurAt = now()
        ourCalc = try {
            ShareRouting.calculate(n, r, lat, lng, wps, strategy)
        } catch (_: Exception) { false }
        NaviBridge.extraInfo = if (ourCalc) "偏航：按分享的路线重新规划（${wps.size} 个途经点）…" else "偏航：按分享的路线重新规划没能开始"
        if (ourCalc) NaviBridge.setRerouting(true)                    // v4.9 圆屏继续显示“重新规划中”
    }
}
