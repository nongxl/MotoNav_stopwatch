package com.yuminaga.motonav

import android.net.Uri
import android.webkit.CookieManager
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import java.util.UUID

/** PC motorcycle route. Its geometry remains authoritative; SDK driving queries are metadata only. */
data class MotorPoint(val lat: Double, val lng: Double, val name: String = "", val id: String = "")
data class MotorRequest(val start: MotorPoint, val end: MotorPoint, val vias: List<MotorPoint> = emptyList()) {
    fun pageUrl(): String = Uri.parse("https://www.amap.com/ssr/dir").buildUpon()
        .appendQueryParameter("type", "4")
        .appendQueryParameter("fname", start.name.ifBlank { "起点" })
        .appendQueryParameter("flat", start.lat.toString()).appendQueryParameter("flon", start.lng.toString())
        .appendQueryParameter("dname", end.name.ifBlank { "终点" })
        .appendQueryParameter("dlat", end.lat.toString()).appendQueryParameter("dlon", end.lng.toString())
        .appendQueryParameter("did", end.id)
        .apply {
            if (vias.isNotEmpty()) {
                appendQueryParameter("vlat", vias.joinToString("|") { it.lat.toString() })
                appendQueryParameter("vlon", vias.joinToString("|") { it.lng.toString() })
                appendQueryParameter("vname", vias.joinToString("|") { it.name })
            }
        }.build().toString()
}
data class MotorStep(val instruction: String, val road: String, val action: String,
                     val distance: Int, val duration: Int, val lights: Int,
                     val startIndex: Int, val endIndex: Int) {
    val dir: String get() = when {
        "掉头" in action -> "U"
        "左前" in action || "靠左" in action -> "SL"
        "右前" in action || "靠右" in action -> "SR"
        "左" in action -> "L"
        "右" in action -> "R"
        else -> "S" // Destination-ahead is guidance, never a completion event.
    }
}
data class WebMotorRoute(val request: MotorRequest, val points: List<DoubleArray>,
                         val steps: List<MotorStep>, val distance: Int, val seconds: Int, val lights: Int,
                         val key: String = "webmotor-" + UUID.randomUUID()) {
    fun tracker() = MotorTrack(points.toTypedArray())
}

object WebMotorRoutes {
    const val PAGE = "https://www.amap.com/ssr/dir?type=4"
    const val UA = "Mozilla/5.0 (Linux; Android 14) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36"

    fun isPage(url: String): Boolean = runCatching {
        val u = Uri.parse(url)
        u.scheme == "https" && u.host == "www.amap.com" && u.path == "/ssr/dir" && u.getQueryParameter("type") == "4"
    }.getOrDefault(false)

    fun request(url: String): MotorRequest {
        require(isPage(url)) { "请在高德网页选择摩托车，设置起终点后再读取；或粘贴 type=4 的完整路线链接。" }
        val u = Uri.parse(url)
        fun point(prefix: String): MotorPoint {
            val longName = if (prefix == "f") "from" else "to"
            val nested = u.getQueryParameter("$longName[lnglat]")?.split(',')
            val lat = nested?.getOrNull(1)?.toDoubleOrNull() ?: u.getQueryParameter("${prefix}lat")?.toDoubleOrNull()
            val lng = nested?.getOrNull(0)?.toDoubleOrNull() ?: u.getQueryParameter("${prefix}lon")?.toDoubleOrNull()
            require(lat != null && lng != null && MotorTrack.valid(lat, lng)) { "网页链接缺少有效起终点坐标，请在网页完整选定起点和终点。" }
            return MotorPoint(lat, lng, u.getQueryParameter("$longName[name]") ?: u.getQueryParameter("${prefix}name").orEmpty(),
                u.getQueryParameter("$longName[id]") ?: u.getQueryParameter("${prefix}id").orEmpty())
        }
        val latText = u.getQueryParameter("vlat").orEmpty()
        val lngText = u.getQueryParameter("vlon").orEmpty()
        val names = u.getQueryParameter("vname").orEmpty().split("|")
        val lats = latText.split("|"); val lngs = lngText.split("|")
        require(latText.isBlank() == lngText.isBlank() && lats.size == lngs.size && lats.size <= 16) { "途经点格式无效" }
        val vias = if (latText.isBlank()) {
            require(u.getQueryParameter("vid").isNullOrBlank()) { "途经点只有 ID，缺少坐标；请在网页加载完成后重新读取。" }
            val nestedVias = ArrayList<MotorPoint>()
            for (i in 0 until 16) {
                val value = u.getQueryParameter("via[$i][lnglat]") ?: break
                val xy = value.split(',')
                val la = xy.getOrNull(1)?.toDoubleOrNull(); val lo = xy.getOrNull(0)?.toDoubleOrNull()
                require(la != null && lo != null && MotorTrack.valid(la, lo)) { "途经点坐标无效" }
                nestedVias.add(MotorPoint(la,lo,u.getQueryParameter("via[$i][name]").orEmpty(),u.getQueryParameter("via[$i][id]").orEmpty()))
            }
            require(u.getQueryParameter("via[16][lnglat]") == null) { "途经点过多" }
            nestedVias
        } else lats.indices.map { i ->
            val la = lats[i].toDoubleOrNull(); val lo = lngs[i].toDoubleOrNull()
            require(la != null && lo != null && MotorTrack.valid(la, lo)) { "途经点坐标无效" }
            MotorPoint(la, lo, names.getOrNull(i).orEmpty())
        }
        return MotorRequest(point("f"), point("d"), vias)
    }

    fun load(request: MotorRequest): List<WebMotorRoute> {
        require(MotorTrack.valid(request.start.lat, request.start.lng) && MotorTrack.valid(request.end.lat, request.end.lng)) { "起终点坐标无效" }
        if (Thread.currentThread().isInterrupted) throw java.io.InterruptedIOException("规划已取消")
        val uri = Uri.parse("https://amap-pc-ssr.amap.com/ssr/api/getMotorRoutePlan").buildUpon()
            .appendQueryParameter("fromX", request.start.lng.toString()).appendQueryParameter("fromY", request.start.lat.toString())
            .appendQueryParameter("toX", request.end.lng.toString()).appendQueryParameter("toY", request.end.lat.toString())
            .apply {
                if (request.start.id.isNotBlank()) appendQueryParameter("originid", request.start.id)
                if (request.end.id.isNotBlank()) appendQueryParameter("destinationid", request.end.id)
                if (request.vias.isNotEmpty()) appendQueryParameter("waypoints", request.vias.joinToString(";") { "${it.lng},${it.lat}" })
            }.build()
        val connection = URL(uri.toString()).openConnection() as HttpURLConnection
        connection.connectTimeout = 15000; connection.readTimeout = 20000
        connection.instanceFollowRedirects = false
        connection.setRequestProperty("User-Agent", UA)
        connection.setRequestProperty("Referer", "https://www.amap.com/")
        // Share only cookies scoped by Android to this exact HTTPS service. Never export or log them.
        val cookies = CookieManager.getInstance()
        cookies.getCookie(uri.toString())?.takeIf { it.isNotBlank() }?.let { connection.setRequestProperty("Cookie", it) }
        try {
            val status = connection.responseCode
            connection.headerFields.entries.filter { it.key.equals("Set-Cookie", true) }.flatMap { it.value.orEmpty() }
                .forEach { cookies.setCookie(uri.toString(), it) }
            cookies.flush()
            require(status != 401 && status != 403) { "高德要求登录或网页验证" }
            require(status != 429) { "请求较频繁，请稍后再试" }
            require(status == 200) { "高德网页算路失败（HTTP ${connection.responseCode}），请在网页重试。" }
            val data = connection.inputStream.use { stream ->
                val out = java.io.ByteArrayOutputStream()
                val buffer = ByteArray(8192)
                while (true) {
                    if (Thread.currentThread().isInterrupted) throw java.io.InterruptedIOException("规划已取消")
                    val count = stream.read(buffer)
                    if (count < 0) break
                    require(out.size() + count <= 8 * 1024 * 1024) { "路线数据过大" }
                    out.write(buffer, 0, count)
                }
                out.toByteArray()
            }
            require(data.size <= 8 * 1024 * 1024) { "路线数据过大" }
            return parse(String(data, Charsets.UTF_8), request)
        } finally { connection.disconnect() }
    }

    fun parse(text: String, request: MotorRequest): List<WebMotorRoute> {
        val json = try { JSONObject(text) } catch (_: Exception) { error("高德返回了验证页或无效数据，请回网页检查。") }
        require(json.optInt("code") == 1) { "摩托车算路失败：${json.optString("message", "服务未说明原因").take(160)}" }
        val paths = json.optJSONArray("data") ?: error("网页没有返回摩托车路线")
        require(paths.length() in 1..10) { "网页未返回有效路线" }
        return (0 until paths.length()).map { routeIndex ->
            val path = paths.getJSONObject(routeIndex)
            val rawSteps = path.optJSONArray("steps") ?: error("路线缺少转向指令")
            require(rawSteps.length() in 1..10000) { "路线分段无效" }
            val points = ArrayList<DoubleArray>(); val steps = ArrayList<MotorStep>()
            for (i in 0 until rawSteps.length()) {
                val step = rawSteps.getJSONObject(i)
                val startIndex = (points.size - 1).coerceAtLeast(0)
                val poly = step.optString("polyline")
                require(poly.isNotBlank()) { "第 ${i + 1} 段缺少路线坐标" }
                for (pair in poly.split(';')) {
                    val xy = pair.split(',')
                    val lng = xy.getOrNull(0)?.toDoubleOrNull(); val lat = xy.getOrNull(1)?.toDoubleOrNull()
                    require(lat != null && lng != null && MotorTrack.valid(lat, lng)) { "路线坐标无效" }
                    val last = points.lastOrNull()
                    if (last == null || last[0] != lat || last[1] != lng) {
                        require(points.size < 150000) { "路线坐标过多" }
                        if (last != null) require(MotorTrack.distance(last[0], last[1], lat, lng) < 5000) { "路线坐标不连续" }
                        points.add(doubleArrayOf(lat, lng))
                    }
                }
                val cost = step.optJSONObject("cost")
                steps.add(MotorStep(step.optString("instruction"), step.optString("road_name"),
                    step.optJSONObject("navi")?.optString("action").orEmpty(), step.optInt("step_distance"),
                    cost?.optInt("duration") ?: 0, cost?.optInt("traffic_lights") ?: 0, startIndex, points.lastIndex))
            }
            val cost = path.optJSONObject("cost")
            WebMotorRoute(request, points, steps, path.optInt("distance"), cost?.optInt("duration") ?: 0,
                cost?.optInt("traffic_lights") ?: 0).also { it.tracker() }
        }
    }
}
