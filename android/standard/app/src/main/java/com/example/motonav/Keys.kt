package com.example.motonav

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import java.security.MessageDigest

/**
 * 版本：跟随 App 版本，见 MainActivity.kt 里的 APP_VERSION（v6.2 新增；v6.5 去掉内置 key，只用 App 里填的）
 *
 * 高德 key 在 App 里填写（保存在手机上），App 里不带任何内置 key。
 * - 第一次打开、或打开时还有 key 没填，自动弹出填写页
 * - 高德 key 在 App 启动时交给高德各个 SDK（地图、定位、搜索、导航），所以改了高德 key 要重启 App 才生效
 *
 * 别人用这个 App：在高德开放平台建一个“Android 平台”的 key，填这个 App 的包名和 SHA1（App 的 key 设置页里能直接复制），
 */
object Keys {
    private const val PREFS = "keys"
    private const val K_AMAP = "amap"

    /** 启动时实际交给高德 SDK 的 key（用来判断改了之后要不要重启） */
    @Volatile var appliedAmap = ""
        private set
    /** 交给高德 SDK 的结果，调试信息里显示 */
    @Volatile var applyInfo = ""
        private set

    private fun prefs(c: Context) = c.applicationContext.getSharedPreferences(PREFS, Context.MODE_PRIVATE)

    /** App 里填的高德 key（没填返回空） */
    fun userAmap(c: Context) = prefs(c).getString(K_AMAP, "").orEmpty().trim()

    /** 实际使用的 key：只有 App 里填的（v6.5 起不再有内置 key） */
    fun amap(c: Context) = userAmap(c)

    fun hasAmap(c: Context) = amap(c).isNotEmpty()
    /** 地图服务配置完成 */
    fun allFilled(c: Context) = hasAmap(c)

    /** 界面上显示用：只露出头尾几位 */
    fun mask(k: String) = when {
        k.isEmpty() -> "没填"
        k.length <= 10 -> "已填"
        else -> "已填 ${k.take(4)}…${k.takeLast(4)}"
    }

    /** 保存。返回 true 表示高德 key 变了、需要重启 App 才生效 */
    fun save(c: Context, amapKey: String): Boolean {
        prefs(c).edit().putString(K_AMAP, amapKey.trim()).remove("tdt").apply()
        return amap(c) != appliedAmap
    }

    /**
     * 把高德 key 交给各个高德 SDK。必须在用任何高德功能之前调用（MotoNavApp 启动时调一次）。
     * 用反射调用：哪个 SDK 没有这个方法也不会编译失败或闪退，结果写在 applyInfo 里。
     */
    fun apply(c: Context) {
        prefs(c).edit().remove("tdt").apply()
        val ctx = c.applicationContext
        val k = amap(ctx)
        appliedAmap = k
        if (k.isEmpty()) { applyInfo = "高德 key 没填"; return }
        val ok = ArrayList<String>()
        val bad = ArrayList<String>()
        fun call(name: String, block: () -> Unit) {
            try { block(); ok += name } catch (e: Throwable) { bad += "$name(${e.javaClass.simpleName})" }
        }
        call("地图") {
            Class.forName("com.amap.api.maps.MapsInitializer")
                .getMethod("setApiKey", String::class.java).invoke(null, k)
        }
        call("定位") {
            Class.forName("com.amap.api.location.AMapLocationClient")
                .getMethod("setApiKey", String::class.java).invoke(null, k)
        }
        call("搜索") {
            val cls = Class.forName("com.amap.api.services.core.ServiceSettings")
            val inst = cls.getMethod("getInstance").invoke(null)
            cls.getMethod("setApiKey", String::class.java).invoke(inst, k)
        }
        call("导航") {
            Class.forName("com.amap.api.navi.AMapNavi")
                .getMethod("setApiKey", Context::class.java, String::class.java).invoke(null, ctx, k)
        }
        applyInfo = "高德 key ${mask(k)}" +
            "，已交给：${ok.joinToString("、").ifEmpty { "无" }}" +
            (if (bad.isNotEmpty()) "；没成功：${bad.joinToString("、")}" else "")
    }

    /** 这个 App 的签名 SHA1（高德 Android key 要填），格式 AA:BB:... */
    fun signSha1(c: Context): String = try {
        val pm = c.packageManager
        val sigBytes: ByteArray? = if (Build.VERSION.SDK_INT >= 28) {
            val info = pm.getPackageInfo(c.packageName, PackageManager.GET_SIGNING_CERTIFICATES)
            info.signingInfo?.apkContentsSigners?.firstOrNull()?.toByteArray()
        } else {
            @Suppress("DEPRECATION")
            pm.getPackageInfo(c.packageName, PackageManager.GET_SIGNATURES).signatures?.firstOrNull()?.toByteArray()
        }
        if (sigBytes == null) "读不到" else
            MessageDigest.getInstance("SHA1").digest(sigBytes).joinToString(":") { "%02X".format(it) }
    } catch (e: Exception) { "读不到（${e.message}）" }

    /** 重启 App（高德 key 改了之后用） */
    fun restartApp(a: Activity) {
        val launch = a.packageManager.getLaunchIntentForPackage(a.packageName) ?: return
        val i = Intent.makeRestartActivityTask(launch.component)
        a.startActivity(i)
        Runtime.getRuntime().exit(0)
    }
}
