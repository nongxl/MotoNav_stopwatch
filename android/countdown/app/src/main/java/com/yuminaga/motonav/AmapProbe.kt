package com.yuminaga.motonav

import android.accessibilityservice.AccessibilityService
import android.app.Notification
import android.content.BroadcastReceiver
import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Typeface
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.provider.Settings
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification
import android.text.TextUtils
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.view.accessibility.AccessibilityEvent
import android.view.accessibility.AccessibilityNodeInfo
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * 版本：跟随 App 版本，见 MainActivity.kt 里的 APP_VERSION（v3.5 新增；v3.7 通知解析修复、记录更细）
 *
 * 高德探测：看看能从高德官方 App 那里“读”到哪些导航信息，用来决定要不要改成
 * “用高德 App 导航（摩托车模式、换语音）+ 圆屏显示”。三个来源一起记录：
 * 1. 高德手机版的导航通知（通知读取权限；锁屏、灭屏也能收到）
 * 2. 高德界面上的文字（无障碍权限；需要屏幕亮着、高德显示在前台）
 * 3. 高德车机版发出的标准广播 AUTONAVI_STANDARD_BROADCAST_SEND（不需要权限）
 * 只记录，不改动任何导航功能。内容没变化时不重复记录。
 */
object AmapProbe {
    private const val MAX = 400
    private val log = ArrayDeque<String>()
    private val lastByKey = HashMap<String, String>()
    @Volatile var notifCount = 0
    @Volatile var screenCount = 0
    @Volatile var bcCount = 0
    @Volatile var image: Bitmap? = null          // 最近一条高德通知渲染出来的样子
    @Volatile var imageVer = 0
    private var receiver: BroadcastReceiver? = null

    private fun time() = SimpleDateFormat("HH:mm:ss", Locale.CHINA).format(Date())

    /** 记一条；同一来源内容和上次一样就跳过 */
    @Synchronized fun add(source: String, key: String, body: String): Boolean {
        if (lastByKey[key] == body) return false
        lastByKey[key] = body
        log.addFirst("【$source ${time()}】\n$body")
        while (log.size > MAX) log.removeLast()
        return true
    }

    @Synchronized fun text(): String = if (log.isEmpty()) "（还没有记录）" else log.joinToString("\n\n")
    @Synchronized fun size() = log.size

    @Synchronized fun clear() {
        log.clear(); lastByKey.clear()
        notifCount = 0; screenCount = 0; bcCount = 0
        image = null; imageVer++
    }

    fun putImage(b: Bitmap) { image = b; imageVer++ }

    /** 注册车机版广播接收（App 进程活着就一直收） */
    fun ensureReceiver(ctx: Context) {
        if (receiver != null) return
        val r = object : BroadcastReceiver() {
            override fun onReceive(c: Context, i: Intent) { onBroadcast(i) }
        }
        val f = IntentFilter("AUTONAVI_STANDARD_BROADCAST_SEND")
        try {
            if (Build.VERSION.SDK_INT >= 33) ctx.applicationContext.registerReceiver(r, f, Context.RECEIVER_EXPORTED)
            else ctx.applicationContext.registerReceiver(r, f)
            receiver = r
        } catch (_: Exception) {}
    }

    private fun onBroadcast(i: Intent) {
        val ex = i.extras ?: return
        @Suppress("DEPRECATION") val kt = ex.get("KEY_TYPE")
        bcCount++
        add("车机版广播", "bc:$kt", "KEY_TYPE = $kt\n" + dumpBundle(ex, setOf("KEY_TYPE")))
    }

    @Suppress("DEPRECATION")
    fun dumpBundle(b: Bundle, skip: Set<String> = emptySet()): String =
        b.keySet().sorted().filter { it !in skip }.joinToString("\n") { k -> "  $k = ${valStr(b.get(k))}" }

    fun valStr(v: Any?): String = when (v) {
        null -> "null"
        is Bitmap -> "[图片 ${v.width}×${v.height}]"
        is android.graphics.drawable.Icon -> "[图标]"
        is Bundle -> "{ " + dumpBundle(v).replace("\n", "; ") + " }"
        is Array<*> -> v.joinToString(" | ") { valStr(it) }
        is CharSequence -> v.toString()
        else -> v.toString()
    }.take(300)

    fun notifEnabled(ctx: Context): Boolean =
        (Settings.Secure.getString(ctx.contentResolver, "enabled_notification_listeners") ?: "").contains(ctx.packageName)

    fun screenEnabled(ctx: Context): Boolean =
        (Settings.Secure.getString(ctx.contentResolver, Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES) ?: "")
            .contains(ctx.packageName + "/")
}

/** 读高德的通知（系统设置里打开“通知使用权”后自动运行） */
class AmapNotifyListener : NotificationListenerService() {
    override fun onListenerConnected() {
        MusicBridge.init(this)
        MusicBridge.refresh(true)
        AmapProbe.add("系统", "nl", "通知读取已连接")
        try { activeNotifications?.forEach { onNotificationPosted(it) } } catch (_: Exception) {}
    }

    override fun onNotificationPosted(sbn: StatusBarNotification?) {
        if (sbn == null || !sbn.packageName.startsWith("com.autonavi")) return
        val n = sbn.notification ?: return
        val sb = StringBuilder()
        sb.append("应用 = ${sbn.packageName}　编号 = ${sbn.id}　常驻 = ${sbn.isOngoing}　类别 = ${n.category}　频道 = ${if (Build.VERSION.SDK_INT >= 26) n.channelId else "-"}\n")
        n.extras?.let { sb.append(AmapProbe.dumpBundle(it)).append('\n') }
        sb.append("  通知界面：").append(viewTexts(n)).append('\n')
        AmapProbe.notifCount++
        AmapProbe.add("高德通知", "n:${sbn.packageName}:${sbn.id}", sb.toString().trim())
    }

    /**
     * 高德的导航通知可能是自定义样式：展开成真正的界面，读出上面所有文字。
     * v3.7 先读文字、记下用的是哪种样式；画成图片单独尝试（失败不影响文字）
     */
    @Suppress("DEPRECATION")
    private fun viewTexts(n: Notification): String {
        var kind = ""
        val rv = when {
            n.bigContentView != null -> { kind = "自定义（展开）"; n.bigContentView }
            n.contentView != null -> { kind = "自定义"; n.contentView }
            else -> try {
                kind = "系统标准样式"
                val b = Notification.Builder.recoverBuilder(this, n)
                b.createBigContentView() ?: b.createContentView()
            } catch (_: Exception) { null }
        } ?: return "（没有通知界面）"
        val v = try { rv.apply(this, FrameLayout(this)) } catch (e: Exception) { return "样式=$kind（展开失败：${e.message}）" }
        val texts = ArrayList<String>()
        val ids = ArrayList<String>()
        var imgs = 0
        fun walk(x: View) {
            val id = try { if (x.id != View.NO_ID) x.resources.getResourceEntryName(x.id) else null } catch (_: Exception) { null }
            if (x is TextView && !x.text.isNullOrBlank()) texts.add((if (id != null) "[$id]" else "") + x.text)
            if (x is ImageView) {
                imgs++
                if (id != null) ids.add(id)
                x.contentDescription?.let { if (it.isNotBlank()) texts.add("图片说明:$it") }
            }
            if (x is ViewGroup) for (i in 0 until x.childCount) walk(x.getChildAt(i))
        }
        walk(v)
        renderImage(v)
        return "样式=$kind　文字：" + texts.joinToString(" | ") +
            (if (imgs > 0) "　图片 $imgs 个${if (ids.isNotEmpty()) "（${ids.joinToString(",")}）" else ""}" else "")
    }

    /** 把通知界面画成图片给探测页看；有些图片只能用硬件绘制，失败就算了 */
    private fun renderImage(v: View) {
        try {
            val w = 1000
            v.measure(View.MeasureSpec.makeMeasureSpec(w, View.MeasureSpec.EXACTLY), View.MeasureSpec.makeMeasureSpec(0, View.MeasureSpec.UNSPECIFIED))
            val h = v.measuredHeight.coerceIn(1, 1200)
            v.layout(0, 0, w, h)
            val bmp = if (Build.VERSION.SDK_INT >= 28) {
                val pic = android.graphics.Picture()
                val c = pic.beginRecording(w, h)
                c.drawColor(Color.parseColor("#3A3644"))
                v.draw(c)
                pic.endRecording()
                Bitmap.createBitmap(pic)
            } else {
                val b = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
                val c = Canvas(b); c.drawColor(Color.parseColor("#3A3644")); v.draw(c); b
            }
            AmapProbe.putImage(bmp)
        } catch (_: Throwable) {}
    }
}

/** 读高德界面上的文字（系统设置 → 无障碍里打开后运行；屏幕亮着、高德在前台时才有内容） */
class AmapScreenReader : AccessibilityService() {
    private var lastAt = 0L

    override fun onServiceConnected() { AmapProbe.add("系统", "ac", "屏幕读取已连接") }

    override fun onAccessibilityEvent(e: AccessibilityEvent?) {
        val pkg = e?.packageName?.toString() ?: return
        if (!pkg.startsWith("com.autonavi")) return
        val now = SystemClock.elapsedRealtime()
        if (now - lastAt < 1500) return                      // 最多 1.5 秒读一次
        lastAt = now
        val roots = try { windows.mapNotNull { it.root } } catch (_: Exception) { emptyList() }
            .ifEmpty { listOfNotNull(rootInActiveWindow) }
        val out = ArrayList<String>()
        fun walk(n: AccessibilityNodeInfo?, depth: Int) {
            if (n == null || depth > 40 || out.size > 150) return
            val t = n.text?.toString()
            val d = n.contentDescription?.toString()
            val id = n.viewIdResourceName?.substringAfter(":id/")
            if (!t.isNullOrBlank() || !d.isNullOrBlank()) {
                out.add(buildString {
                    if (id != null) append("[$id]")
                    if (!t.isNullOrBlank()) append(t)
                    if (!d.isNullOrBlank()) append("（说明:$d）")
                })
            } else if (id != null) out.add("<$id>")          // v3.7 没有文字的控件也记下名字，看看导航面板在不在

            for (i in 0 until n.childCount) walk(n.getChild(i), depth + 1)
        }
        var wins = 0
        for (r in roots) if (r.packageName?.toString()?.startsWith("com.autonavi") == true) { wins++; walk(r, 0) }
        if (wins > 1) out.add(0, "（高德窗口 $wins 个，可能有悬浮窗）")
        if (out.isEmpty()) return
        AmapProbe.screenCount++
        AmapProbe.add("高德界面", "s:$pkg", "应用 = $pkg\n  " + out.joinToString(" / "))
    }

    override fun onInterrupt() {}
}

/** 探测页面：权限开关、三个来源的计数、最近一条通知的样子、全部记录（可一键复制） */
class ProbeActivity : AppCompatActivity() {
    private val S = NavStyle
    private fun dp(v: Int) = S.dpi(this, v)
    private val handler = Handler(Looper.getMainLooper())
    private lateinit var notifState: TextView
    private lateinit var screenState: TextView
    private lateinit var bcState: TextView
    private lateinit var logText: TextView
    private lateinit var img: ImageView
    private var shownVer = -1
    private var shownSize = -1

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        supportActionBar?.hide()
        window.statusBarColor = S.BG
        window.navigationBarColor = S.BG
        AmapProbe.ensureReceiver(this)

        val root = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(16), dp(20), dp(16), dp(24)) }
        root.addView(TextView(this).apply { text = "高德探测"; textSize = 24f; typeface = S.BOLD; setTextColor(S.TEXT) })
        root.addView(TextView(this).apply {
            text = "用法：打开下面的权限 → 去高德地图开始导航（摩托车，可用模拟导航）→ 导航中按手机的「主页」回到桌面，" +
                "等 30 秒（高德可能会弹出小悬浮窗）→ 再锁屏 30 秒 → 回到这里点「复制全部记录」，粘贴发给 Claude。"
            textSize = 13f; setTextColor(S.SUB); setLineSpacing(0f, 1.2f); setPadding(0, dp(6), 0, dp(12))
        })

        // 三个来源
        val card = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL; background = S.round(this@ProbeActivity, S.CARD_SOLID, 18f, S.STROKE)
            setPadding(dp(14), dp(10), dp(14), dp(10))
        }
        notifState = stateRow(card, "① 高德通知（锁屏也能收）", "开启权限") {
            startActivity(Intent("android.settings.ACTION_NOTIFICATION_LISTENER_SETTINGS"))
        }
        screenState = stateRow(card, "② 高德界面文字（需亮屏）", "开启权限") {
            startActivity(Intent(Settings.ACTION_ACCESSIBILITY_SETTINGS))
        }
        bcState = stateRow(card, "③ 车机版广播（不需权限）", null, null)
        root.addView(card)

        // 最近一条高德通知的样子
        root.addView(TextView(this).apply { text = "最近一条高德通知的样子"; textSize = 14f; setTextColor(S.TEXT); setPadding(0, dp(16), 0, dp(6)) })
        img = ImageView(this).apply { adjustViewBounds = true; background = S.round(this@ProbeActivity, S.CARD2, 10f) }
        root.addView(img, LinearLayout.LayoutParams(-1, -2))

        // 按钮
        val btns = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; setPadding(0, dp(14), 0, dp(10)) }
        btns.addView(button("复制全部记录", true) {
            val cm = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
            cm.setPrimaryClip(ClipData.newPlainText("高德探测", "MotoNav $APP_VERSION 高德探测记录\n\n" + AmapProbe.text()))
            Toast.makeText(this, "已复制，可以直接粘贴发送", Toast.LENGTH_SHORT).show()
        }, LinearLayout.LayoutParams(0, dp(46), 1f).apply { rightMargin = dp(8) })
        btns.addView(button("清空", false) { AmapProbe.clear(); refresh() }, LinearLayout.LayoutParams(dp(90), dp(46)))
        root.addView(btns)

        logText = TextView(this).apply {
            textSize = 11.5f; setTextColor(S.SUB); typeface = Typeface.MONOSPACE; setTextIsSelectable(true)
            setLineSpacing(0f, 1.15f)
        }
        root.addView(logText)

        setContentView(ScrollView(this).apply {
            setBackgroundColor(S.BG); addView(root); clipToPadding = true
            setOnApplyWindowInsetsListener { v, ins ->          // 避开状态栏和底部导航条
                @Suppress("DEPRECATION")
                val t = if (Build.VERSION.SDK_INT >= 30) ins.getInsets(android.view.WindowInsets.Type.systemBars()).top else ins.systemWindowInsetTop
                @Suppress("DEPRECATION")
                val b = if (Build.VERSION.SDK_INT >= 30) ins.getInsets(android.view.WindowInsets.Type.systemBars()).bottom else ins.systemWindowInsetBottom
                v.setPadding(0, t, 0, b); ins
            }
        })
        handler.post(tick)
    }

    private fun stateRow(parent: LinearLayout, title: String, btn: String?, onClick: (() -> Unit)?): TextView {
        val row = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL; setPadding(0, dp(6), 0, dp(6)) }
        val col = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        col.addView(TextView(this).apply { text = title; textSize = 14.5f; setTextColor(S.TEXT) })
        val st = TextView(this).apply { textSize = 12f; setTextColor(S.SUB); isSingleLine = true; ellipsize = TextUtils.TruncateAt.END }
        col.addView(st)
        row.addView(col, LinearLayout.LayoutParams(0, -2, 1f))
        if (btn != null && onClick != null) row.addView(button(btn, false, onClick), LinearLayout.LayoutParams(-2, dp(36)))
        parent.addView(row)
        return st
    }

    private fun button(t: String, primary: Boolean, onClick: () -> Unit) = TextView(this).apply {
        text = t; textSize = 14f; gravity = Gravity.CENTER; typeface = S.MEDIUM
        setTextColor(if (primary) Color.WHITE else S.TEXT)
        background = if (primary) S.gradient(this@ProbeActivity, 14f) else S.round(this@ProbeActivity, S.CARD2, 14f, S.STROKE)
        setPadding(dp(14), 0, dp(14), 0)
        setOnClickListener { onClick() }
    }

    private val tick = object : Runnable {
        override fun run() { refresh(); handler.postDelayed(this, 1000) }
    }

    private fun refresh() {
        val nOn = AmapProbe.notifEnabled(this)
        val sOn = AmapProbe.screenEnabled(this)
        notifState.text = (if (nOn) "已开启" else "未开启") + " · 收到 ${AmapProbe.notifCount} 次"
        notifState.setTextColor(if (nOn) S.GREEN else S.AMBER)
        screenState.text = (if (sOn) "已开启" else "未开启（可选）") + " · 读到 ${AmapProbe.screenCount} 次"
        screenState.setTextColor(if (sOn) S.GREEN else S.SUB)
        bcState.text = if (AmapProbe.bcCount > 0) "收到 ${AmapProbe.bcCount} 条" else "还没收到（装了车机版并开始导航才会有）"
        bcState.setTextColor(if (AmapProbe.bcCount > 0) S.GREEN else S.SUB)
        if (AmapProbe.imageVer != shownVer) { shownVer = AmapProbe.imageVer; img.setImageBitmap(AmapProbe.image) }
        val sz = AmapProbe.size()
        if (sz != shownSize) { shownSize = sz; logText.text = AmapProbe.text() }
    }

    override fun onDestroy() { handler.removeCallbacksAndMessages(null); super.onDestroy() }
}
