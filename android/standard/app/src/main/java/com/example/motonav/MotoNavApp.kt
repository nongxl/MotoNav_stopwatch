package com.example.motonav

import android.app.Application

/**
 * 版本：跟随 App 版本，见 MainActivity.kt 里的 APP_VERSION（v6.2 新增）
 *
 * App 一启动（不管是从桌面图标、分享的高德路线，还是系统把后台服务拉起来）最先走这里：
 * 先把 App 里填的高德 key 交给高德各个 SDK，再声明隐私合规，之后才会用到任何高德功能。
 */
class MotoNavApp : Application() {
    override fun onCreate() {
        super.onCreate()
        RideRecorder.init(this)
        BootBrand.load(this)
        MusicBridge.init(this)
        CrashLog.install(this)
        Keys.apply(this)
        NaviBridge.initPrivacy(this)
        AmapCaptureHost.install(this)
    }
}
