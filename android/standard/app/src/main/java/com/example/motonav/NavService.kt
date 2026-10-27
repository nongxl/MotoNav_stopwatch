package com.example.motonav

import android.app.Notification
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.app.Service
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import androidx.core.app.NotificationCompat

/**
 * 版本：跟随 App 版本，见 MainActivity.kt 里的 APP_VERSION（v1.7：启动时开始自动亮度；v2.0：读取小地图开关）
 *
 * 后台常驻服务：通知栏显示一个常驻图标，让安卓在锁屏、App 退到后台时
 * 也不杀掉 MotoNav，并保持 CPU 运行，保证蓝牙和导航数据持续发给圆屏。
 */
class NavService : Service() {
    companion object {
        private const val CHANNEL_ID = "motonav_running"
        private const val NOTIFY_ID = 1
        @Volatile var running = false

        fun start(context: Context) {
            val i = Intent(context, NavService::class.java)
            if (Build.VERSION.SDK_INT >= 26) context.startForegroundService(i) else context.startService(i)
        }

        fun stop(context: Context) {
            context.stopService(Intent(context, NavService::class.java))
        }
    }

    private var wakeLock: PowerManager.WakeLock? = null

    override fun onBind(intent: Intent?): IBinder? = null

    override fun onCreate() {
        super.onCreate()
        CrashLog.install(this)            // v5.5 闪退记录
        createChannel()
        try {
            if (Build.VERSION.SDK_INT >= 29) {
                startForeground(
                    NOTIFY_ID, buildNotification(),
                    ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION or ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE
                )
            } else {
                startForeground(NOTIFY_ID, buildNotification())
            }
        } catch (e: Exception) {
            // 权限还没给全时会走到这里：先停掉，授权后再启动
            BleLink.status = "后台服务启动失败，请先点「1. 授予权限」：${e.message}"
            stopSelf()
            return
        }
        running = true

        // 保持 CPU 运行（屏幕可以关），否则锁屏后蓝牙发送和距离推算会变慢
        val pm = getSystemService(Context.POWER_SERVICE) as PowerManager
        wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "MotoNav:nav").apply {
            setReferenceCounted(false)
            acquire()
        }

        BleLink.startAuto(this)
        Brightness.start(this)
        MapFeeder.load(this)              // v2.0 读取小地图开关
        AmapCaptureHost.serviceStarted(this)
        CallWatcher.start(this)           // v5.0 来电显示
        Weather.init(this)                // v5.4 圆屏天气页
        AmapProbe.ensureReceiver(this)    // v3.5 高德探测：接收车机版广播
        NaviVoice.load(this)              // v3.8 导航语音
    }

    override fun onStartCommand(intent: Intent?, flags: Int, startId: Int): Int = START_STICKY

    override fun onDestroy() {
        WebMotorGuide.stop()
        RideRecorder.stop()
        AmapCaptureHost.serviceStopped(this)
        running = false
        CallWatcher.stop()                // v5.0
        Brightness.stop()
        BleLink.stopAuto()
        try { wakeLock?.release() } catch (_: Exception) {}
        wakeLock = null
        super.onDestroy()
    }

    private fun createChannel() {
        if (Build.VERSION.SDK_INT >= 26) {
            val ch = NotificationChannel(CHANNEL_ID, "MotoNav 运行状态", NotificationManager.IMPORTANCE_LOW)
            ch.description = "MotoNav 在后台保持和圆屏的连接"
            (getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager).createNotificationChannel(ch)
        }
    }

    private fun buildNotification(): Notification {
        val open = PendingIntent.getActivity(
            this, 0,
            Intent(this, MainActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP),
            PendingIntent.FLAG_IMMUTABLE
        )
        return NotificationCompat.Builder(this, CHANNEL_ID)
            .setSmallIcon(android.R.drawable.ic_dialog_map)
            .setContentTitle("MotoNav $APP_VERSION 正在运行")
            .setContentText("保持与圆屏的连接，点此打开")
            .setContentIntent(open)
            .setOngoing(true)
            .setPriority(NotificationCompat.PRIORITY_LOW)
            .build()
    }
}
