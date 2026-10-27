package com.example.motonav

import android.content.Context
import android.content.SharedPreferences

/** Only the next power-on animation; never changes an active navigation screen. */
object BootBrand {
    // Keep IDs aligned with BootAnimation.h; append future brands before changing Off only
    // with a protocol migration (Off is currently 17).
    val labels = arrayOf("MotoNav 默认", "本田 Honda", "雅马哈 Yamaha", "铃木 Suzuki",
        "川崎 Kawasaki", "宝马 BMW", "杜卡迪 Ducati", "KTM", "凯旋 Triumph",
        "哈雷 Harley-Davidson", "阿普利亚 Aprilia", "春风 CFMOTO", "钱江 QJMOTOR",
        "无极 VOGE", "升仕 ZONTES", "贝纳利 Benelli", "皇家恩菲尔德 Royal Enfield", "关闭开机动画")
    private var prefs: SharedPreferences? = null
    @Volatile var selected = 0
        private set
    @Volatile private var confirmed = -1

    fun load(context: Context) {
        prefs = context.applicationContext.getSharedPreferences("motonav", Context.MODE_PRIVATE)
        selected = prefs!!.getInt("boot_brand", 0).takeIf { it in labels.indices } ?: 0
    }

    fun select(id: Int) {
        if (id !in labels.indices) return
        selected = id
        prefs?.edit()?.putInt("boot_brand", id)?.apply()
        sync()
    }

    fun sync() {
        confirmed = -1
        BleLink.send("#BOOT,$selected")
    }

    fun onReply(id: Int) { if (id in labels.indices) confirmed = id }

    fun summary(): String {
        val state = when {
            !BleLink.isConnected || !BleLink.verified -> "连接圆屏后自动同步"
            confirmed == selected -> "已保存到圆屏，下次开机生效"
            confirmed == -2 -> "圆屏保存失败，请重新选择"
            else -> "等待圆屏确认（需开机动画版固件）"
        }
        return "${labels[selected]} · $state"
    }

    fun onSaveError() { confirmed = -2 }
}
