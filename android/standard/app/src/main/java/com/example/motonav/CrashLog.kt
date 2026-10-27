package com.example.motonav

import android.content.Context
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * 版本：跟随 App 版本，见 MainActivity.kt 里的 APP_VERSION（v5.5 新增）
 *
 * 闪退记录：App 万一闪退，把出错的地方存到手机里，下次打开在「调试信息」最上面显示，
 * 截图发出来就能知道是哪一行出的问题（不用连电脑看日志）。
 */
object CrashLog {
    private var file: File? = null
    @Volatile private var installed = false

    fun install(context: Context) {
        if (installed) return
        installed = true
        file = File(context.applicationContext.filesDir, "crash.txt")
        val old = Thread.getDefaultUncaughtExceptionHandler()
        Thread.setDefaultUncaughtExceptionHandler { t, e ->
            try {
                val time = SimpleDateFormat("MM-dd HH:mm:ss", Locale.CHINA).format(Date())
                val trace = e.stackTraceToString().lines().take(25).joinToString("\n")
                file?.writeText("$time  App $APP_VERSION  线程 ${t.name}\n$trace")
            } catch (_: Throwable) {}
            old?.uncaughtException(t, e)
        }
    }

    /** 上次闪退的记录；没有返回空 */
    fun last(): String = try { file?.takeIf { it.exists() }?.readText() ?: "" } catch (_: Throwable) { "" }

    fun clear() { try { file?.delete() } catch (_: Throwable) {} }
}
