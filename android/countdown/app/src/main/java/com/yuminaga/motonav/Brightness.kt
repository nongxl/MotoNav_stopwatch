package com.yuminaga.motonav

import android.content.Context
import android.os.Handler
import android.os.Looper
import java.util.Calendar
import java.util.TimeZone
import kotlin.math.acos
import kotlin.math.asin
import kotlin.math.atan
import kotlin.math.cos
import kotlin.math.floor
import kotlin.math.sin
import kotlin.math.tan

/**
 * 版本：跟随 App 版本，见 MainActivity.kt 里的 APP_VERSION（v1.7 新增）
 *
 * 圆屏亮度：
 * - 自动：按当地日出日落时间切换白天/夜间亮度（位置来自导航定位；还没有位置时按 6:30~18:30 算白天）
 * - 手动：App 里拖动滑块，固定亮度
 * 每分钟检查一次，亮度变化时发 "#b,亮度" 给圆屏。
 * v2.8 同时检查小地图该不该换夜间配色（见 MapFeeder.applyNight）。
 */
object Brightness {
    const val DAY_LEVEL = 255
    const val NIGHT_LEVEL = 70

    private const val PREFS = "motonav"
    private val handler = Handler(Looper.getMainLooper())
    private var ctx: Context? = null

    @Volatile var auto = true
        private set
    @Volatile var manualLevel = 200
        private set
    @Volatile var current = -1
    @Volatile var lat = Double.NaN
    @Volatile var lng = Double.NaN
    @Volatile var desc = ""

    /** 读取上次保存的设置（App 界面打开时也会调用，好让开关和滑块显示正确） */
    fun load(context: Context) {
        if (ctx != null) return
        ctx = context.applicationContext
        val sp = ctx!!.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
        auto = sp.getBoolean("bright_auto", true)
        manualLevel = sp.getInt("bright_manual", 200)
    }

    fun start(context: Context) {
        load(context)
        handler.removeCallbacks(tick)
        handler.post(tick)
    }

    fun stop() { handler.removeCallbacks(tick) }

    fun changeAuto(on: Boolean) {
        auto = on
        save()
        apply()
    }

    fun changeManual(level: Int) {
        manualLevel = level.coerceIn(5, 255)
        auto = false
        save()
        apply()
    }

    fun updateLocation(latitude: Double, longitude: Double) {
        lat = latitude
        lng = longitude
    }

    private fun save() {
        ctx?.getSharedPreferences(PREFS, Context.MODE_PRIVATE)?.edit()
            ?.putBoolean("bright_auto", auto)?.putInt("bright_manual", manualLevel)?.apply()
    }

    private val tick = object : Runnable {
        override fun run() {
            apply()
            handler.postDelayed(this, 60_000)
        }
    }

    /** 计算并发送当前亮度 */
    fun apply() {
        val level = if (auto) {
            if (isDaytime()) DAY_LEVEL else NIGHT_LEVEL
        } else manualLevel
        if (level != current) {
            current = level
            BleLink.send("#b,$level")
        }
        MapFeeder.applyNight()                    // v2.8 小地图夜间配色也跟着日出日落切换
    }

    /** v2.8 现在是不是白天（给小地图夜间配色用） */
    fun isDaytimeNow(): Boolean = isDaytime()

    private fun isDaytime(): Boolean {
        val cal = Calendar.getInstance()
        val nowMin = cal.get(Calendar.HOUR_OF_DAY) * 60 + cal.get(Calendar.MINUTE)
        if (lat.isNaN() || lng.isNaN()) {
            desc = "白天 6:30~18:30（还没有定位）"
            return nowMin in 390 until 1110
        }
        val times = sunTimes(lat, lng, cal)
        if (times == null) {
            desc = "极昼/极夜，按白天处理"
            return true
        }
        val (rise, set) = times
        desc = "日出 ${fmt(rise)}，日落 ${fmt(set)}"
        return nowMin in rise until set
    }

    private fun fmt(m: Int) = "%02d:%02d".format(m / 60, m % 60)

    /**
     * 计算当天日出、日落的本地时间（从 0 点起的分钟数）。
     * 算法：NOAA 简化公式，精度在几分钟以内，足够用来切换昼夜亮度。
     */
    private fun sunTimes(latitude: Double, longitude: Double, cal: Calendar): Pair<Int, Int>? {
        val dayOfYear = cal.get(Calendar.DAY_OF_YEAR)
        val tzHours = TimeZone.getDefault().getOffset(cal.timeInMillis) / 3_600_000.0
        fun calc(rising: Boolean): Double? {
            val lngHour = longitude / 15.0
            val t = dayOfYear + ((if (rising) 6.0 else 18.0) - lngHour) / 24.0
            val m = 0.9856 * t - 3.289
            var l = m + 1.916 * sin(Math.toRadians(m)) + 0.020 * sin(Math.toRadians(2 * m)) + 282.634
            l = (l % 360 + 360) % 360
            var ra = Math.toDegrees(atan(0.91764 * tan(Math.toRadians(l))))
            ra = (ra % 360 + 360) % 360
            val lQuad = floor(l / 90.0) * 90.0
            val raQuad = floor(ra / 90.0) * 90.0
            ra = (ra + (lQuad - raQuad)) / 15.0
            val sinDec = 0.39782 * sin(Math.toRadians(l))
            val cosDec = cos(asin(sinDec))
            val cosH = (cos(Math.toRadians(90.833)) - sinDec * sin(Math.toRadians(latitude))) /
                (cosDec * cos(Math.toRadians(latitude)))
            if (cosH > 1 || cosH < -1) return null
            var h = if (rising) 360 - Math.toDegrees(acos(cosH)) else Math.toDegrees(acos(cosH))
            h /= 15.0
            val localT = h + ra - 0.06571 * t - 6.622
            val ut = ((localT - lngHour) % 24 + 24) % 24
            return ((ut + tzHours) % 24 + 24) % 24
        }
        val rise = calc(true) ?: return null
        val set = calc(false) ?: return null
        return Pair((rise * 60).toInt(), (set * 60).toInt())
    }
}
