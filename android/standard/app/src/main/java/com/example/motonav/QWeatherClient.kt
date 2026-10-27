package com.example.motonav

import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import java.time.OffsetDateTime
import java.time.ZoneId
import java.util.Locale
import java.util.zip.GZIPInputStream
import kotlin.math.roundToInt

/** All weather requests use the configured account host. Credentials never enter URLs or logs. */
object QWeatherClient {
    data class Hour(val hour: Int, val icon: Int, val temperature: Int, val probability: Int)
    data class Result(val text: String, val temperature: Int, val icon: Int,
        val wind: String, val humidity: String, val hours: List<Hour>,
        val rain: String, val rainBars: List<Int>, val air: String, val aqi: Int,
        val warningTitle: String, val warningBody: String, val warningAdvice: String,
        val warningCount: Int, val warningLevel: Int, val notes: String, val attribution: String)

    fun normalizeHost(raw: String): String {
        val host = raw.trim().removePrefix("https://").trimEnd('/').lowercase(Locale.ROOT)
        require(Regex("[a-z0-9-]+(?:\\.[a-z0-9-]+)*\\.qweatherapi\\.com").matches(host)) {
            "请填写控制台设置中的 API Host（xxxx.qweatherapi.com）"
        }
        return host
    }

    private fun get(host: String, key: String, path: String): JSONObject {
        val connection = URL("https://$host$path").openConnection() as HttpURLConnection
        try {
            connection.connectTimeout = 8000; connection.readTimeout = 10000
            connection.instanceFollowRedirects = false
            connection.setRequestProperty("X-QW-Api-Key", key)
            connection.setRequestProperty("Accept-Encoding", "gzip")
            val status = connection.responseCode
            if (status != 200) throw IllegalStateException("HTTP $status")
            val input = connection.inputStream
            val stream = if (connection.contentEncoding?.contains("gzip", true) == true) GZIPInputStream(input) else input
            val body = stream.bufferedReader(Charsets.UTF_8).use { it.readText() }
            val result = JSONObject(body)
            val code = result.optString("code")
            if (code.isNotBlank() && code != "200") throw IllegalStateException("服务状态 $code")
            return result
        } finally { connection.disconnect() }
    }

    fun query(rawHost: String, key: String, lat: Double, lon: Double): Result {
        val host = normalizeHost(rawHost)
        require(key.isNotBlank()) { "请先填写和风天气 API Key" }
        val point = "%.2f/%.2f".format(Locale.US, lat, lon)
        val location = "%.2f%%2C%.2f".format(Locale.US, lon, lat)
        val notes = mutableListOf<String>()
        val attributions = linkedSetOf("和风天气 QWeather · https://www.qweather.com/", "https://developer.qweather.com/attribution.html")
        fun fetch(label: String, path: String): JSONObject? = try {
            get(host, key, path).also { json ->
                json.optJSONObject("metadata")?.optJSONArray("attributions")?.let { arr ->
                    for (i in 0 until arr.length()) attributions += arr.optString(i)
                }
            }
        } catch (e: Exception) {
            notes += "$label：${if (e is java.net.SocketTimeoutException) "请求超时" else if(e is IllegalStateException) e.message else "网络或数据异常"}"
            null
        }
        val now = fetch("实况", "/weather/v1/current/$point?lang=zh&localTime=true")
            ?: throw IllegalStateException(notes.joinToString("；"))
        // Do not retry automatically: the next screen request observes the cache/backoff.
        val hourly = fetch("逐小时", "/weather/v1/hourly/$point?hours=6&lang=zh&localTime=true")
        val rain = fetch("降水", "/v7/minutely/5m?location=$location&lang=zh")
        val air = fetch("空气", "/airquality/v1/current/$point?lang=zh")
        val alert = fetch("预警", "/weatheralert/v1/current/$point?lang=zh&localTime=true")
        return parse(now, hourly, rain, air, alert, notes.joinToString("；"), attributions.joinToString("\n"))
    }

    fun parse(now: JSONObject, hourly: JSONObject?, rain: JSONObject?, air: JSONObject?, alert: JSONObject?,
              notes: String = "", attribution: String = "和风天气 QWeather · https://www.qweather.com/"): Result {
        val condition = now.getJSONObject("condition")
        val text = condition.getString("text")
        val temp = now.getJSONObject("temperature").getDouble("value")
        require(temp.isFinite() && temp in -100.0..70.0) { "实况温度数据无效" }
        val wind = now.optJSONObject("wind")
        val direction = wind?.optJSONObject("direction")?.optInt("degree", -1) ?: -1
        val names = arrayOf("北", "东北", "东", "东南", "南", "西南", "西", "西北")
        val windName = if(direction in 0..359) names[((direction + 22) / 45) % 8] + "风" else "风向未知"
        val scale = wind?.optInt("scale", -1) ?: -1
        val humidity = now.optDouble("humidity", Double.NaN)
        val hours = mutableListOf<Hour>()
        val array = hourly?.optJSONArray("hours")
        if (array != null) for (i in 0 until array.length()) {
            val h = array.optJSONObject(i) ?: continue
            val time = try { OffsetDateTime.parse(h.getString("forecastTime")) } catch (_: Exception) { continue }
            if(time.toInstant().isBefore(java.time.Instant.now())) continue
            val local = time.atZoneSameInstant(ZoneId.systemDefault())
            val t = h.optJSONObject("temperature")?.optDouble("value", Double.NaN) ?: Double.NaN
            if(!t.isFinite()) continue
            val p = h.optJSONObject("precipitation")?.optDouble("probability", Double.NaN) ?: Double.NaN
            hours += Hour(local.hour, iconOf(h.optJSONObject("condition")?.optString("text").orEmpty(), local.hour < 6 || local.hour >= 19),
                t.roundToInt(), if(p.isFinite() && p in 0.0..1.0) (p * 100).roundToInt() else -1)
            if(hours.size == 5) break
        }
        val bars = mutableListOf<Int>()
        rain?.optJSONArray("minutely")?.let { points ->
            for (i in 0 until minOf(points.length(), 24)) {
                val value = points.optJSONObject(i)?.optDouble("precip", Double.NaN) ?: Double.NaN
                bars += if(value.isFinite() && value >= 0) (value * 100).roundToInt().coerceAtMost(9999) else -1
            }
        }
        val indexes = air?.optJSONArray("indexes")
        var index: JSONObject? = null
        if(indexes != null) for(i in 0 until indexes.length()) {
            val candidate = indexes.optJSONObject(i) ?: continue
            if(index == null || candidate.optString("code") == "cn-mee") index = candidate
            if(candidate.optString("code") == "cn-mee") break
        }
        val aqi = index?.optDouble("aqi", Double.NaN) ?: Double.NaN
        val airText = if(index != null && aqi.isFinite()) "${index.optString("name", "AQI")} ${index.optString("aqiDisplay", aqi.roundToInt().toString())} · ${index.optString("category")}" else "空气质量暂不可用"
        val alerts = mutableListOf<JSONObject>()
        alert?.optJSONArray("alerts")?.let { list ->
            for(i in 0 until list.length()) {
                val a = list.optJSONObject(i) ?: continue
                if(a.optJSONObject("messageType")?.optString("code") == "cancel") continue
                val expire = try { OffsetDateTime.parse(a.optString("expireTime")).toInstant() } catch (_: Exception) { null }
                if(expire != null && expire.isBefore(java.time.Instant.now())) continue
                alerts += a
            }
        }
        fun level(a: JSONObject): Int = when(a.optString("severity")) { "extreme" -> 4; "severe" -> 3; "moderate" -> 2; else -> 1 }
        val top = alerts.maxByOrNull { level(it) }
        val noAlertConfirmed = alert != null && (alert.optJSONArray("alerts") != null || alert.optJSONObject("metadata")?.optBoolean("zeroResult") == true)
        return Result(text, temp.roundToInt(), iconOf(text, java.time.LocalTime.now().hour.let { it < 6 || it >= 19 }),
            windName + if(scale >= 0) " ${scale}级" else "", if(humidity.isFinite() && humidity in 0.0..1.0) "湿度 ${(humidity*100).roundToInt()}%" else "湿度未知", hours,
            rain?.optString("summary")?.takeIf { it.isNotBlank() } ?: "降水预报暂不可用", bars,
            airText, if(aqi.isFinite()) aqi.roundToInt() else -1,
            top?.optString("headline") ?: if(noAlertConfirmed) "当前位置暂无有效预警" else "预警信息暂不可用",
            top?.optString("description").orEmpty(), top?.optString("instruction").orEmpty(),
            if(noAlertConfirmed) alerts.size else -1, top?.let { level(it) } ?: 0, notes, attribution)
    }
    fun iconOf(w: String, night: Boolean): Int = when {
        w.contains("雷") -> 5
        w.contains("雪") && w.contains("雨") -> 8
        w.contains("冰雹") -> 8
        w.contains("雪") -> 6
        w.contains("暴雨") || w.contains("大雨") -> 4
        w.contains("雨") -> 3
        w.contains("雾") || w.contains("霾") -> 7
        w.contains("沙") || w.contains("尘") -> 9
        w.contains("阴") -> 2
        w.contains("云") -> if (night) 11 else 1          // 多云、少云、晴间多云
        w.contains("晴") -> if (night) 10 else 0
        else -> 2
    }
}
