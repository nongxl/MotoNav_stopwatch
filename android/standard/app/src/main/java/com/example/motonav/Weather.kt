package com.example.motonav

import android.content.Context
import android.location.Address
import android.location.Geocoder
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import com.amap.api.location.AMapLocation
import com.amap.api.location.AMapLocationClient
import com.amap.api.location.AMapLocationClientOption
import com.amap.api.services.core.LatLonPoint
import com.amap.api.services.geocoder.GeocodeSearch
import com.amap.api.services.geocoder.RegeocodeQuery
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.URL
import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Date
import java.util.Locale
import java.util.concurrent.Executors

/** 和风天气：按圆屏请求查询，10分钟缓存；高德仅用于定位及区县解析。 */
object Weather {
    private var qResult: QWeatherClient.Result? = null
    private var retryAt = 0L
    private var weatherCoordinateIsAmap = true
    private fun qPrefs() = ctx!!.getSharedPreferences("qweather", Context.MODE_PRIVATE)
    fun host() = if(ctx == null) "" else qPrefs().getString("host", "").orEmpty()
    fun key() = if(ctx == null) "" else qPrefs().getString("key", "").orEmpty()
    fun save(host: String, key: String) {
        val normalized = QWeatherClient.normalizeHost(host)
        require(key.isNotBlank() && key.none { it.isWhitespace() }) { "请填写有效的 API Key" }
        qPrefs().edit().putString("host", normalized).putString("key", key.trim()).apply()
        ++requestId; busy = false; retryAt = 0; lastMsg = null; qResult = null; lastAt = 0
        BleLink.clearWeather()
        info = "和风配置已保存，滑到天气页查询"
    }
    fun detailText(): String = qResult?.let {
        "$place\n${it.text} ${it.temperature}°\n${it.wind} · ${it.humidity}\n\n降水：${it.rain}\n空气：${it.air}\n\n${it.warningTitle}\n${it.warningBody}\n${it.warningAdvice}\n\n${it.notes}\n\n${it.attribution}\n预警可能延迟，请以官方最新发布为准。"
    } ?: info
    @Volatile var info = "（还没查过：在圆屏上左右滑到天气页就会查）"
        private set(value) { field = value; android.util.Log.i("MotoNavWeather", value) }
    private var ctx: Context? = null
    private val main = Handler(Looper.getMainLooper())
    private var busy = false
    private var busyAt = 0L
    private var lastAt = 0L
    private var lastMsg: String? = null
    private var lastImg3: TextImage.Img? = null
    private var lastImg4: TextImage.Img? = null
    private const val FRESH_MS = 10 * 60 * 1000L
    private var requestId = 0
    private var placePending = false
    private var placeErr = ""
    private var placeFixAgeS = 0L
    private val geocodeWorker = Executors.newSingleThreadExecutor { task ->
        Thread(task, "MotoNav-weather-area").apply { isDaemon = true }
    }

    // 查询中间结果
    private var place = ""
    private var live: Array<String>? = null           // 天气, 温度, 风向, 风力, 湿度, 更新时间
    private var fc: List<IntArray>? = null            // 每天：星期, 图标, 最高, 最低
    private var hourly: List<IntArray>? = null        // v5.6 每小时：几点, 图标, 温度, 降雨概率
    private var hourlyErr = ""
    private var omCur: DoubleArray? = null            // v7.3 Open-Meteo 实况：温度, 天气代码, 风速km/h, 风向°, 湿度%, 是否白天
    private var amapErr = ""                          // v7.3 高德那边失败的原因（给调试信息看）
    private var liveDone = false
    private var fcDone = false
    private var hourlyDone = false
    private const val HOURS = 5

    fun init(context: Context) {
        if (ctx != null) return
        ctx = context.applicationContext
        NaviBridge.initPrivacy(ctx!!)                    // 高德要求先声明隐私同意，没开过导航也要能查天气
    }

    /** 圆屏要天气了 */
    fun request() {
        main.post {
            if (host().isBlank() || key().isBlank()) { info = "请在手机地图 Key 中填写和风 API Host 与 API Key"; BleLink.clearWeather(); return@post }
            val now = SystemClock.elapsedRealtime()
            if (now < retryAt) return@post
            if (lastMsg != null && now - lastAt < FRESH_MS) { push(); return@post }
            if (busy && now - busyAt < 90_000) return@post
            refresh()
        }
    }

    private fun refresh() {
        val c = ctx ?: run { info = "App 还没准备好"; return }
        val id = ++requestId
        busy = true; busyAt = SystemClock.elapsedRealtime()
        info = "正在定位…"
        place = ""; placePending = false; placeErr = ""; placeFixAgeS = 0L
        live = null; fc = null; hourly = null
        liveDone = true; fcDone = true; hourlyDone = false; hourlyErr = ""
        amapErr = ""; omCur = null; qResult = null; weatherCoordinateIsAmap = true
        if (!Keys.hasAmap(c)) { amapErr = "没填高德 key"; phoneLocate(id); return }
        var answered = false
        main.postDelayed({ if (id == requestId && !answered) { answered = true; amapErr = "高德定位 15 秒没回音"; phoneLocate(id) } }, 15_000)
        try {
            val client = AMapLocationClient(c)
            client.setLocationOption(AMapLocationClientOption()
                .setOnceLocation(true).setNeedAddress(true).setHttpTimeOut(15000)
                .setLocationMode(AMapLocationClientOption.AMapLocationMode.Hight_Accuracy))
            client.setLocationListener { loc: AMapLocation? ->
                main.post {
                    try { client.stopLocation(); client.onDestroy() } catch (_: Exception) {}
                    if (id != requestId || answered) return@post
                    answered = true
                    if (loc == null || loc.errorCode != 0) {
                        // v7.3 高德定位失败（常见：key 的 SHA1 和安装包对不上）：改用手机自己的定位
                        amapErr = "高德定位失败 ${loc?.errorCode} ${loc?.errorInfo?.take(40) ?: ""}"
                        phoneLocate(id)
                        return@post
                    }
                    val code = loc.adCode ?: ""
                    place = WeatherArea.format(loc.city, loc.district)
                    if (loc.district.isNullOrBlank()) resolvePlace(loc.latitude, loc.longitude, id, GeocodeSearch.AMAP)
                    hourly = null; hourlyDone = false
                    queryHourly(loc.latitude, loc.longitude, id)            // v5.6
                    // All weather comes from QWeather; AMap is used here only for the location.
                }
            }
            client.startLocation()
        } catch (e: Exception) {
            if (id == requestId && !answered) { answered = true; amapErr = "高德定位没能启动：${e.message}"; phoneLocate(id) }
        }
    }

    /** v7.3 不用高德：手机自己的定位（先用最近一次的位置，没有就现定位一次，最多等 20 秒） */
    @android.annotation.SuppressLint("MissingPermission")
    private fun phoneLocate(id: Int) {
        val c = ctx ?: return
        info = "使用手机定位查询和风天气…"; weatherCoordinateIsAmap = false
        place = ""
        live = null; fc = null; liveDone = true; fcDone = true            // 不查高德天气了，实况改用和风天气
        hourly = null; hourlyDone = false
        val lm = c.getSystemService(Context.LOCATION_SERVICE) as android.location.LocationManager
        val last = try {
            lm.getProviders(true).mapNotNull { lm.getLastKnownLocation(it) }
                .filter { System.currentTimeMillis() - it.time in 0..600_000L }.maxByOrNull { it.time }
        } catch (_: Exception) { null }
        fun useFix(location: android.location.Location) {
            placeFixAgeS = ((System.currentTimeMillis() - location.time) / 1000).coerceAtLeast(0)
            resolvePlace(location.latitude, location.longitude, id)
            queryHourly(location.latitude, location.longitude, id)
        }
        if (last != null && System.currentTimeMillis() - last.time <= 120_000L) {
            useFix(last)
            return
        }
        var got = false
        val l = object : android.location.LocationListener {
            override fun onLocationChanged(loc: android.location.Location) {
                if (got) return
                got = true
                try { lm.removeUpdates(this) } catch (_: Exception) {}
                if (id != requestId) return
                useFix(loc)
            }
            @Deprecated("") override fun onStatusChanged(p: String?, s: Int, b: android.os.Bundle?) {}
            override fun onProviderEnabled(p: String) {}
            override fun onProviderDisabled(p: String) {}
        }
        try {
            for (pv in listOf(android.location.LocationManager.NETWORK_PROVIDER, android.location.LocationManager.GPS_PROVIDER))
                if (lm.isProviderEnabled(pv)) lm.requestLocationUpdates(pv, 0L, 0f, l, Looper.getMainLooper())
        } catch (e: Exception) { }
        main.postDelayed({
            if (!got) { got = true; try { lm.removeUpdates(l) } catch (_: Exception) {}
                if (id != requestId) return@postDelayed
                if (last != null && System.currentTimeMillis() - last.time in 0..600_000L) {
                    useFix(last)
                    return@postDelayed
                }
                busy = false
                info = "定位失败：高德（$amapErr），手机定位也没拿到位置（打开手机的定位开关试试）" }
        }, 20_000)
    }

    /** 手机定位只有经纬度；在后台补查区县，不阻塞天气请求和界面。 */
    private fun resolvePlace(lat: Double, lng: Double, id: Int, coordinateType: String = GeocodeSearch.GPS) {
        val c = ctx ?: return
        placePending = true
        fun finish(area: String?, error: String) {
            main.post {
                if (id != requestId || !placePending) return@post
                placePending = false
                place = area ?: "区域未获取"
                placeErr = error.ifBlank { if (area == null || area.contains("未获取")) "定位服务未返回区县" else "" }
                done()
            }
        }
        main.postDelayed({
            if (id == requestId && placePending) finish(null, "区县解析超时")
        }, 20_000)
        geocodeWorker.execute {
            var amapAreaError = ""
            if (Keys.hasAmap(c)) {
                try {
                    val query = RegeocodeQuery(LatLonPoint(lat, lng), 200f, coordinateType)
                    val address = GeocodeSearch(c).getFromLocation(query)
                    if (!address?.district.isNullOrBlank()) {
                        finish(WeatherArea.format(address.city, address.district), "")
                        return@execute
                    }
                    amapAreaError = "高德未返回区县"
                } catch (e: Exception) { amapAreaError = "高德区域：${e.message}" }
            }
            try {
                if (!Geocoder.isPresent()) { finish(null, "手机未提供地址解析服务；$amapAreaError"); return@execute }
                // 系统 Geocoder 使用 GPS 原生坐标，高德定位点则先转换。
                val gps = if (coordinateType == GeocodeSearch.AMAP) MapGeo.gcjToWgs(lat, lng) else doubleArrayOf(lat, lng)
                val geocoder = Geocoder(c, Locale.SIMPLIFIED_CHINESE)
                fun fromAddress(addresses: List<Address>) {
                    val area = addresses.firstOrNull()?.let { WeatherArea.fromGeocoder(it.locality, it.subLocality, it.subAdminArea) }
                    finish(area, if (area == null || area.contains("未获取")) amapAreaError else "")
                }
                if (Build.VERSION.SDK_INT >= 33) {
                    geocoder.getFromLocation(gps[0], gps[1], 1, object : Geocoder.GeocodeListener {
                        override fun onGeocode(addresses: MutableList<Address>) = fromAddress(addresses)
                        override fun onError(errorMessage: String?) = finish(null, "${errorMessage ?: "区县解析失败"}；$amapAreaError")
                    })
                } else {
                        @Suppress("DEPRECATION")
                    val addresses = geocoder.getFromLocation(gps[0], gps[1], 1).orEmpty()
                    fromAddress(addresses)
                }
            } catch (e: Exception) { finish(null, "${e.message ?: "区县解析失败"}；$amapAreaError") }
        }
    }

    private fun queryHourly(lat: Double, lng: Double, id: Int) {
        val host = host(); val key = key()
        Thread {
            try {
                // QWeather requires GCJ-02 in mainland China; system GPS fixes are converted.
                val gps = if (weatherCoordinateIsAmap) doubleArrayOf(lat, lng) else MapGeo.wgsToGcj(lat, lng)
                val data = QWeatherClient.query(host, key, gps[0], gps[1])
                main.post {
                    if (id != requestId) return@post
                    qResult = data
                    live = arrayOf(data.text, data.temperature.toString(), data.wind, "", data.humidity.removePrefix("湿度 ").removeSuffix("%"), java.time.LocalTime.now().toString().take(5))
                    hourly = data.hours.map { intArrayOf(it.hour, it.icon, it.temperature, it.probability) }
                    liveDone = true; fcDone = true; hourlyDone = true
                    done()
                }
            } catch (e: Exception) {
                main.post {
                    if (id != requestId) return@post
                    busy = false
                    retryAt = SystemClock.elapsedRealtime() + 120_000L
                    info = "和风天气查询失败：" + if (e is IllegalArgumentException || e is IllegalStateException) e.message else "网络或数据异常"
                    BleLink.clearWeather()
                }
            }
        }.start()
    }
    /** 实况、按天预报、逐小时预报都回来了：拼好发给圆屏 */
    private fun done() {
        if (!liveDone || !fcDone || !hourlyDone) return
        busy = false
        val l = live ?: return
        val icon = iconOf(l[0], isNight())
        val temp = l[1].toDoubleOrNull()?.let { Math.round(it).toInt() } ?: 0
        val time = Regex("(\\d{1,2}:\\d{2})").find(l[5])?.value ?: ""
        val days = fc ?: emptyList()
        val hrs = hourly
        val sb = StringBuilder("#W,$icon,$temp,$time")
        if (hrs != null) {
            sb.append(",H,${hrs.size}")                              // v5.6 逐小时
            for (h in hrs) sb.append(",${h[0]},${h[1]},${h[2]},${h[3]}")
        } else {
            sb.append(",D,${days.size}")                             // 查不到逐小时：退回按天
            for (d in days) sb.append(",${d[0]},${d[1]},${d[2]},${d[3]}")
        }
        lastMsg = sb.toString()

        val wind = when {
            l[2].isBlank() || l[2] == "无风向" -> ""
            l[2].endsWith("风") || l[2] == "旋转不定" -> l[2]
            else -> "${l[2]}风"
        }
        val power = if (l[3].isNotBlank()) " ${l[3]}级" else ""
        val hum = qResult?.humidity.orEmpty()
        val area = if (placePending) "区域获取中" else place.ifBlank { "区域未获取" } +
            if (placeFixAgeS > 120) "（上次定位）" else ""
        lastImg3 = TextImage.render(listOf(area, l[0]).filter { it.isNotBlank() }.joinToString(" · "), 330, 34f, 18f)
        lastImg4 = TextImage.render(listOf(qResult?.wind.orEmpty(), hum).filter { it.isNotBlank() }.joinToString(" · "), 340, 26f, 18f)
        lastAt = SystemClock.elapsedRealtime()
        info = "$area ${l[0]} ${temp}°；和风逐小时 ${hrs?.size ?: 0} 个；" + qResult?.let { "${it.rain}；${it.air}；${it.warningTitle}；${it.notes}" }.orEmpty()
        push()
    }

    private fun push() {
        val m = lastMsg ?: return
        fun img(slot: Int, im: TextImage.Img?) =
            if (im == null) BleLink.sendImage(slot, 0, 0, ByteArray(0)) else BleLink.sendImage(slot, im.w, im.h, im.data)
        try {
            img(3, lastImg3)
            img(4, lastImg4)
            qResult?.let { data ->
                for ((slot, text) in listOf(7 to data.rain, 8 to data.air, 9 to data.warningTitle, 10 to data.warningBody.take(60), 11 to data.warningAdvice.take(60))) {
                    val image = TextImage.render(text, 330, 24f, 18f)
                    img(slot, image)
                }
                BleLink.send("#WX,${data.aqi},${data.warningCount},${data.warningLevel},${data.rainBars.size},${data.warningTitle.hashCode() and 0x7fffffff}" + data.rainBars.joinToString("") { ",$it" })
            }
            BleLink.send(m)
        } catch (e: Throwable) {
            info = "发给圆屏出错：$e"
        }
    }

    private fun isNight(): Boolean {
        val h = Calendar.getInstance().get(Calendar.HOUR_OF_DAY)
        return h >= 19 || h < 6
    }

    /** 高德天气文字 → 圆屏图标编号（见固件 v5.3 天气页） */
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
