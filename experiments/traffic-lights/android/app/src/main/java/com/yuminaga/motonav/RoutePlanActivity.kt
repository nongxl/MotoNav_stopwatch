package com.yuminaga.motonav

import android.app.Activity
import android.content.ClipboardManager
import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.drawable.GradientDrawable
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.text.Editable
import android.text.TextUtils
import android.text.TextWatcher
import android.view.Gravity
import android.view.KeyEvent
import android.view.View
import android.view.ViewGroup
import android.view.WindowInsets
import android.view.inputmethod.EditorInfo
import android.view.inputmethod.InputMethodManager
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import android.widget.Toast
import androidx.activity.OnBackPressedCallback
import androidx.appcompat.app.AppCompatActivity
import com.amap.api.maps.AMap
import com.amap.api.maps.CameraUpdateFactory
import com.amap.api.maps.MapView
import com.amap.api.maps.model.BitmapDescriptorFactory
import com.amap.api.maps.model.LatLng
import com.amap.api.maps.model.LatLngBounds
import com.amap.api.maps.model.Marker
import com.amap.api.maps.model.MarkerOptions
import com.amap.api.maps.model.MyLocationStyle
import com.amap.api.maps.model.Polyline
import com.amap.api.maps.model.PolylineOptions
import com.amap.api.navi.AMapNavi
import com.amap.api.navi.model.AMapNaviPath
import com.amap.api.navi.model.NaviLatLng
import com.amap.api.navi.view.RouteOverLay
import com.amap.api.services.core.LatLonPoint
import com.amap.api.services.help.Inputtips
import com.amap.api.services.help.InputtipsQuery
import com.amap.api.services.help.Tip
import org.json.JSONArray
import org.json.JSONObject
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.sqrt

/**
 * 版本：跟随 App 版本，见 MainActivity.kt 里的 APP_VERSION（v3.0 新增）
 *
 * 搜索目的地 + 选路线（代替高德组件的规划页）：
 * 1. 顶部搜索框：输入时显示高德联想提示，没输入时显示“最近去过”（全键盘直接打字，回车选第一个）
 * 2. 选好目的地后同时规划最多 3 条路线，画在地图上，底部卡片对比 时间 / 距离 / 红绿灯 / 收费
 * 3. “不走高速”默认打开（摩托车），可关；“模拟”打开后用模拟导航（在家测试用）
 * 4. 点“开始导航”进入导航页 NavActivity
 * 6. v4.9 “+ 途经点”：在路线页加最多 3 个途经点（比如先去加油站），按顺序经过
 * 5. v4.4 导入高德 App 分享的路线（比如高德里规划的摩托车路线）：在高德里分享给 MotoNav，或者复制链接后
 *    点搜索页最上面的“粘贴高德分享的路线”。读出原路线后，在路上挑十几个途经点让高德 SDK 照着走，
 *    地图上虚线是原路线，卡片上显示两条路线的吻合程度（见 AmapShare.kt）
 * 8. v6.8 不再边打边搜：点右边“搜索”或按回车才搜（同一串字再按回车 = 选第一个结果）
 * 7. v6.3 首页就能搜索：首页选好的目的地 / 粘贴的分享路线直接带进来，跳过搜索页，返回直接回首页
 */
class RoutePlanActivity : AppCompatActivity() {
    private val S = NavStyle
    private fun dp(v: Int) = S.dpi(this, v)
    private val handler = Handler(Looper.getMainLooper())

    private lateinit var mapView: MapView
    private var aMap: AMap? = null
    private var navi: AMapNavi? = null

    private var myLat = Double.NaN
    private var myLng = Double.NaN
    private var centered = false

    // 目的地
    private var destName = ""
    private var destAddr = ""
    private var destLat = 0.0
    private var destLng = 0.0
    private var destMarker: Marker? = null

    // 路线
    private val overlays = LinkedHashMap<Int, RouteOverLay>()
    private var routeIds = IntArray(0)
    private var selected = -1
    private var calculating = false
    private var calcSeq = 0
    private var starting = false
    private val routeOwner = Any()
    private var waitingForOrigin = false
    private var waitingOriginRetry = 1

    private fun logPlan(event: String) = android.util.Log.i("MotoNavRoute",
        "$event page=${System.identityHashCode(routeOwner)} seq=$calcSeq epoch=${NaviBridge.navigationEpoch()}")

    // v4.9 自己加的途经点（最多 3 个）
    private class Via(val name: String, val lat: Double, val lng: Double)
    private val vias = ArrayList<Via>()
    private val viaMarkers = ArrayList<Marker>()
    private var addingVia = false
    private lateinit var viaRow: LinearLayout
    private lateinit var viaText: TextView
    private lateinit var viaClear: TextView

    // v4.4 导入的高德分享路线
    private var shareRoute: AmapShare.Route? = null
    private var sharePoly: Polyline? = null
    private var importSeq = 0

    // 设置
    private var routeMode = LocalRouting.DRIVE
    private val bicycle: Boolean get() = shareRoute?.isTravel == true || (shareRoute == null && routeMode == LocalRouting.BICYCLE)
    private lateinit var modeChip: TextView
    private var avoidHighway = true
    private var emulate = false

    // 界面
    private lateinit var root: FrameLayout
    private lateinit var scrim: View
    private lateinit var searchBar: LinearLayout
    private lateinit var input: EditText
    private lateinit var clearBtn: TextView
    private lateinit var panel: ScrollView
    private lateinit var listBox: LinearLayout
    private lateinit var sheet: LinearLayout
    private lateinit var sheetTitle: TextView
    private lateinit var sheetSub: TextView
    private lateinit var statusText: TextView
    private lateinit var cardsRow: LinearLayout
    private lateinit var startBtn: TextView
    private lateinit var amapShareBtn: TextView
    private lateinit var hwChip: TextView
    private lateinit var emuChip: TextView
    private lateinit var locBtn: TextView
    private var insetTop = 0
    private var insetBottom = 0
    private var suppressSearch = false
    private var fromHome = false                 // v6.3 从首页带目的地进来的：返回直接回首页
    private var tips: List<Tip> = emptyList()

    @Suppress("DEPRECATION")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        supportActionBar?.hide()
        NaviBridge.initPrivacy(this)
        MapFeeder.load(this)
        avoidHighway = getSharedPreferences("motonav", Context.MODE_PRIVATE).getBoolean("avoid_hw", true)

        routeMode = LocalRouting.normalize(savedInstanceState?.getInt("direct_route_mode")
            ?: intent.getIntExtra(EXTRA_MODE, getSharedPreferences("motonav", MODE_PRIVATE).getInt("direct_route_mode", LocalRouting.DRIVE)))
        // 地图铺满整个屏幕（包括状态栏下面），上下的卡片按系统栏留出空间
        window.statusBarColor = Color.TRANSPARENT
        window.navigationBarColor = S.CARD_SOLID
        if (Build.VERSION.SDK_INT >= 30) window.setDecorFitsSystemWindows(false)
        else window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_LAYOUT_STABLE or View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN

        root = FrameLayout(this).apply { setBackgroundColor(S.BG) }
        mapView = MapView(this)
        mapView.onCreate(savedInstanceState)
        root.addView(mapView, FrameLayout.LayoutParams(-1, -1))
        setupMap()

        scrim = View(this).apply {
            background = GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM,
                intArrayOf(Color.parseColor("#E60E0A14"), Color.TRANSPARENT))
        }
        root.addView(scrim, FrameLayout.LayoutParams(-1, dp(90)))

        buildPanel()
        buildSheet()
        buildSearchBar()
        buildLocButton()
        setContentView(root)

        root.setOnApplyWindowInsetsListener { _, ins ->
            var top: Int; var bottom: Int; var ime = 0
            if (Build.VERSION.SDK_INT >= 30) {
                val sb = ins.getInsets(WindowInsets.Type.systemBars())
                top = sb.top; bottom = sb.bottom
                ime = ins.getInsets(WindowInsets.Type.ime()).bottom
            } else {
                top = ins.systemWindowInsetTop; bottom = ins.systemWindowInsetBottom
            }
            insetTop = top; insetBottom = bottom
            (searchBar.layoutParams as FrameLayout.LayoutParams).topMargin = top + dp(10)
            searchBar.requestLayout()
            scrim.layoutParams.height = top + dp(90)
            panel.setPadding(0, top + dp(74), 0, max(bottom, ime) + dp(12))
            sheet.setPadding(dp(14), dp(14), dp(14), bottom + dp(12))
            ins
        }

        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() = goBack()
        })

        showHistory()
        // A supplied destination calls calculate(), which attaches once itself.
        if (!handleHomeIntent(intent)) {
            navi = NaviBridge.attach(this)
            input.requestFocus()
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        handleHomeIntent(intent)
    }

    /** v6.3 首页搜好的目的地 / 粘贴的分享路线直接带进来：跳过搜索页，直接规划路线。返回 true = 带了东西 */
    private fun handleHomeIntent(i: Intent?): Boolean {
        if (i == null) return false
        fromHome = i.getBooleanExtra(EXTRA_FROM_HOME, false)
        if (i.hasExtra(EXTRA_MODE)) routeMode = LocalRouting.normalize(i.getIntExtra(EXTRA_MODE, LocalRouting.DRIVE))
        refreshMode()
        i.getStringExtra(EXTRA_SHARE)?.let { importShare(it); return true }      // v4.4 从高德分享过来的
        if (i.hasExtra(EXTRA_LAT)) {
            chooseDest(i.getStringExtra(EXTRA_NAME) ?: "目的地", i.getStringExtra(EXTRA_ADDR) ?: "",
                i.getDoubleExtra(EXTRA_LAT, 0.0), i.getDoubleExtra(EXTRA_LNG, 0.0))
            return true
        }
        return false
    }

    companion object {
        const val EXTRA_SHARE = "share_text"
        const val EXTRA_MODE = "direct_route_mode"
        // v6.3 首页直接带目的地进来
        const val EXTRA_NAME = "dest_name"
        const val EXTRA_ADDR = "dest_addr"
        const val EXTRA_LAT = "dest_lat"
        const val EXTRA_LNG = "dest_lng"
        const val EXTRA_FROM_HOME = "from_home"
    }

    // ================= v4.4 导入高德分享的路线 =================

    private fun pasteShare() {
        val cm = getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
        val t = try { cm.primaryClip?.getItemAt(0)?.coerceToText(this)?.toString() } catch (_: Exception) { null }
        if (AmapShare.findUrl(t) == null) {
            showMessage("剪贴板里没有高德的分享链接\n\n在高德 App 里规划好路线 → 分享 → 复制链接（或直接分享给 MotoNav），再回来点这里")
            handler.postDelayed({ if (panel.visibility == View.VISIBLE && input.text.isNullOrBlank()) showHistory() }, 4000)
            return
        }
        importShare(t!!)
    }

    private fun importShare(text: String) {
        if (AmapShare.findUrl(text) == null) { showMessage("分享的内容里没有高德链接"); return }
        val seq = ++importSeq
        if (sheet.visibility == View.VISIBLE) toSearchMode()
        hideKeyboard()
        showMessage("正在读取高德分享的路线…")
        Thread {
            val res = try { Result.success(AmapShare.load(text)) } catch (e: Throwable) { Result.failure(e) }
            runOnUiThread {
                if (seq != importSeq || isFinishing) return@runOnUiThread
                res.onSuccess { onShareLoaded(it) }
                res.onFailure {
                    showMessage("没能读出路线（App $APP_VERSION）：${it.message}\n\n可以在高德里重新分享一次再试；还不行就长按这段文字全选复制发给我")
                    AmapShare.findUrl(text)?.let { url ->
                        addRow(row("用高德打开分享", "查看原分享或在高德中导航", 0.0, 0.0, S.AMBER, showDist = false) { openAmapShare(url) })
                    }
                }
            }
        }.start()
    }

    private fun onShareLoaded(r: AmapShare.Route) {
        if (r.importNote.isNotBlank()) {
            androidx.appcompat.app.AlertDialog.Builder(this)
                .setTitle("原驾车路线暂时无法读取")
                .setMessage(r.importNote)
                .setPositiveButton("按起终点规划") { _, _ -> showSharedRoute(r) }
                .setNeutralButton("用高德打开") { _, _ -> openAmapShare(r.sourceUrl, r.nativeUri) }
                .setNegativeButton("取消", null).show()
            return
        }
        showSharedRoute(r)
    }

    private fun showSharedRoute(r: AmapShare.Route) {
        if (!r.restored && !r.endpointsOnly) {
            showMessage("高德没能还原这条分享的路线（链接可能过期了）\n\n请在高德 App 里重新规划并分享一次")
            return
        }
        // 原路线画成细虚线，压在 SDK 路线下面，方便对比
        sharePoly?.remove()
        sharePoly = if (r.endpointsOnly && r.pts.size <= 2) null else try {
            aMap?.addPolyline(PolylineOptions().addAll(r.pts.map { LatLng(it[0], it[1]) })
                .width(S.dp(this, 5f)).color(Color.parseColor("#CCFFB547")).setDottedLine(!r.endpointsOnly).zIndex(20f))
        } catch (_: Throwable) { null }
        val info = "高德${r.kind}" + (if (r.distance > 0) " · ${S.distText(r.distance)} · ${S.durText(r.timeS)}" else "") +
            (if (r.lights > 0) " · ${r.lights}灯" else "") +
            (if (r.endpointsOnly) "\n按起终点重新规划，不保证与原路线相同" else "")
        chooseDest(r.endName.ifBlank { "分享的目的地" }, info, r.endLat, r.endLng, share = r)
        // 算路权限不足时仍可查看导入线路，不等待 SDK 成功才缩放。
        sheet.post {
            if (shareRoute !== r) return@post
            try {
                val bounds = LatLngBounds.builder()
                for (point in r.pts) bounds.include(LatLng(point[0], point[1]))
                aMap?.animateCamera(CameraUpdateFactory.newLatLngBoundsRect(bounds.build(),
                    dp(36), dp(36), insetTop + dp(84), sheet.height + dp(28)))
            } catch (_: Exception) { }
        }
    }

    private fun clearShare() {
        shareRoute = null
        if (::amapShareBtn.isInitialized) amapShareBtn.visibility = View.GONE
        sharePoly?.remove(); sharePoly = null
    }

    // ================= 地图 =================

    private fun setupMap() {
        val m = mapView.map ?: return
        aMap = m
        m.setMapType(if (MapFeeder.nightNow) AMap.MAP_TYPE_NIGHT else AMap.MAP_TYPE_NORMAL)
        m.uiSettings.setZoomControlsEnabled(false)
        m.uiSettings.setMyLocationButtonEnabled(false)
        m.uiSettings.setTiltGesturesEnabled(false)
        val style = MyLocationStyle()
            .myLocationType(MyLocationStyle.LOCATION_TYPE_LOCATION_ROTATE_NO_CENTER)
            .interval(2000)
            .myLocationIcon(BitmapDescriptorFactory.fromBitmap(myDot()))
            .strokeColor(Color.TRANSPARENT)
            .radiusFillColor(Color.parseColor("#2243E1F0"))
        m.setMyLocationStyle(style)
        m.setMyLocationEnabled(true)
        m.setOnMyLocationChangeListener { loc ->
            if (loc != null && loc.latitude != 0.0 && loc.longitude != 0.0) {
                myLat = loc.latitude; myLng = loc.longitude
                if(waitingForOrigin && NaviBridge.ownsRoute(routeOwner) && !isFinishing && !isDestroyed) {
                    waitingForOrigin=false
                    logPlan("origin-ready")
                    calculate(waitingOriginRetry)
                }
                if (!centered && destMarker == null) {
                    centered = true
                    m.moveCamera(CameraUpdateFactory.newLatLngZoom(LatLng(myLat, myLng), 15f))
                }
            }
        }
    }

    /** 我的位置：青色圆点 + 白圈 */
    private fun myDot(): Bitmap {
        val s = dp(26)
        val b = Bitmap.createBitmap(s, s, Bitmap.Config.ARGB_8888)
        val c = Canvas(b); val p = Paint(Paint.ANTI_ALIAS_FLAG)
        p.color = Color.parseColor("#5543E1F0"); c.drawCircle(s / 2f, s / 2f, s / 2f, p)
        p.color = Color.WHITE; c.drawCircle(s / 2f, s / 2f, s * 0.3f, p)
        p.color = S.CYAN; c.drawCircle(s / 2f, s / 2f, s * 0.21f, p)
        return b
    }

    /** 目的地：粉色水滴形图钉 */
    private fun pin(): Bitmap {
        val w = dp(34); val h = dp(44)
        val b = Bitmap.createBitmap(w, h, Bitmap.Config.ARGB_8888)
        val c = Canvas(b); val p = Paint(Paint.ANTI_ALIAS_FLAG)
        val r = w * 0.42f; val cx = w / 2f; val cy = r + dp(2)
        val path = Path()
        path.addCircle(cx, cy, r, Path.Direction.CW)
        path.moveTo(cx - r * 0.62f, cy + r * 0.78f); path.lineTo(cx, h - dp(2).toFloat()); path.lineTo(cx + r * 0.62f, cy + r * 0.78f); path.close()
        p.color = Color.parseColor("#990E0A14"); p.style = Paint.Style.STROKE; p.strokeWidth = dp(3).toFloat()
        c.drawPath(path, p)
        p.style = Paint.Style.FILL; p.color = S.PINK; c.drawPath(path, p)
        p.color = Color.WHITE; c.drawCircle(cx, cy, r * 0.4f, p)
        return b
    }

    // ================= 搜索 =================

    private fun buildSearchBar() {
        searchBar = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            background = S.round(this@RoutePlanActivity, S.CARD_SOLID, 16f, S.STROKE)
            elevation = S.dp(this@RoutePlanActivity, 8f)
        }
        val back = TextView(this).apply {
            text = "‹"; textSize = 30f; setTextColor(S.TEXT); gravity = Gravity.CENTER
            setPadding(dp(14), 0, dp(6), dp(4))
            setOnClickListener { goBack() }
        }
        input = EditText(this).apply {
            hint = "搜索目的地"; textSize = 17f; setTextColor(S.TEXT); setHintTextColor(S.SUB)
            background = null; isSingleLine = true
            imeOptions = EditorInfo.IME_ACTION_SEARCH
            setPadding(dp(4), 0, dp(4), 0)
        }
        clearBtn = TextView(this).apply {
            text = "✕"; textSize = 16f; setTextColor(S.SUB); gravity = Gravity.CENTER
            setPadding(dp(10), 0, dp(8), 0); visibility = View.GONE
            setOnClickListener { input.setText(""); toSearchMode() }
        }
        // v6.8 打完字点“搜索”（或按回车）才搜，不再每打一个字就搜
        val goBtn = TextView(this).apply {
            text = "搜索"; textSize = 15f; setTextColor(S.PINK); typeface = S.MEDIUM; gravity = Gravity.CENTER
            setPadding(dp(8), 0, dp(16), 0)
            setOnClickListener { searchOrPick(forceSearch = true) }
        }
        searchBar.addView(back, LinearLayout.LayoutParams(-2, -1))
        searchBar.addView(input, LinearLayout.LayoutParams(0, -1, 1f))
        searchBar.addView(clearBtn, LinearLayout.LayoutParams(-2, -1))
        searchBar.addView(goBtn, LinearLayout.LayoutParams(-2, -1))
        root.addView(searchBar, FrameLayout.LayoutParams(-1, dp(54), Gravity.TOP).apply {
            leftMargin = dp(12); rightMargin = dp(12); topMargin = dp(34)
        })

        input.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
            override fun onTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
            override fun afterTextChanged(e: Editable?) {
                clearBtn.visibility = if (e.isNullOrEmpty()) View.GONE else View.VISIBLE
                if (suppressSearch) return
                if (sheet.visibility == View.VISIBLE) toSearchMode()
                handler.removeCallbacks(searchRun)
                if (e.isNullOrBlank()) showHistory()          // v6.8 不再边打边搜：点“搜索”或按回车才搜
            }
        })
        input.setOnFocusChangeListener { _, has -> if (has && sheet.visibility == View.VISIBLE) toSearchMode() }
        input.setOnEditorActionListener { _, action, ev ->
            if (action == EditorInfo.IME_ACTION_SEARCH || (ev != null && ev.keyCode == KeyEvent.KEYCODE_ENTER)) { searchOrPick(); true } else false
        }
        input.setOnKeyListener { _, code, ev ->
            if (code == KeyEvent.KEYCODE_ENTER && ev.action == KeyEvent.ACTION_UP) { searchOrPick(); true } else false
        }
    }

    /** v6.8 回车 / 点搜索：这串字已经搜过、有结果 → 回车选第一个；否则现在搜（点“搜索”按钮总是重新搜） */
    private var tipsKw = ""
    private var lastGoAt = 0L
    private fun searchOrPick(forceSearch: Boolean = false) {
        val now = android.os.SystemClock.elapsedRealtime()
        if (now - lastGoAt < 300) return                  // 软键盘和实体键盘的回车可能各来一次
        lastGoAt = now
        val kw = input.text.toString().trim()
        if (kw.isEmpty()) return
        if (!forceSearch && tips.isNotEmpty() && tipsKw == kw) { pickFirst(); return }
        if (sheet.visibility == View.VISIBLE) toSearchMode()
        showMessage("搜索中…")
        doSearch(kw)
    }

    private val searchRun = Runnable { doSearch(input.text.toString().trim()) }

    private fun doSearch(kw: String) {
        if (kw.isEmpty()) return
        try {
            val q = InputtipsQuery(kw, "")
            q.setCityLimit(false)
            if (!myLat.isNaN()) q.setLocation(LatLonPoint(myLat, myLng))
            val it = Inputtips(this, q)
            it.setInputtipsListener(object : Inputtips.InputtipsListener {
                override fun onGetInputtips(list: MutableList<Tip>?, code: Int) {
                    if (input.text.toString().trim() != kw || sheet.visibility == View.VISIBLE) return
                    if (code != 1000) { showMessage("搜索失败（错误码 $code），检查一下网络和高德 key"); return }
                    tipsKw = kw
                    showTips((list ?: emptyList()).filter { t -> t.point != null })
                }
            })
            it.requestInputtipsAsyn()
        } catch (e: Exception) {
            showMessage("搜索出错：${e.message}")
        }
    }

    private fun buildPanel() {
        listBox = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(12), 0, dp(12), 0) }
        panel = ScrollView(this).apply {
            setBackgroundColor(S.BG)
            isFillViewport = true
            addView(listBox)
        }
        root.addView(panel, FrameLayout.LayoutParams(-1, -1))
    }

    private fun sectionLabel(t: String) = TextView(this).apply {
        text = t; textSize = 13f; setTextColor(S.SUB); typeface = S.MEDIUM
        setPadding(dp(6), dp(14), 0, dp(8))
    }

    private fun showMessage(t: String) {
        listBox.removeAllViews()
        listBox.addView(TextView(this).apply {
            text = t; textSize = 14f; setTextColor(S.SUB); gravity = Gravity.CENTER
            setTextIsSelectable(true)                       // v4.4.3 出错信息可以长按复制
            setPadding(dp(10), dp(30), dp(10), dp(30))
        })
    }

    /** 一行结果：左边小圆点，名称 + 地址，右边距离 */
    private fun row(name: String, sub: String, lat: Double, lng: Double, dotColor: Int, showDist: Boolean = true, onClick: () -> Unit): View {
        val r = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL
            background = S.round(this@RoutePlanActivity, S.CARD_SOLID, 14f)
            setPadding(dp(14), dp(12), dp(14), dp(12))
            setOnClickListener { onClick() }
        }
        val dot = View(this).apply { background = GradientDrawable().apply { shape = GradientDrawable.OVAL; setColor(dotColor) } }
        r.addView(dot, LinearLayout.LayoutParams(dp(8), dp(8)).apply { rightMargin = dp(12) })
        val col = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        col.addView(TextView(this).apply {
            text = name; textSize = 16f; setTextColor(S.TEXT); typeface = S.MEDIUM
            isSingleLine = true; ellipsize = TextUtils.TruncateAt.END
        })
        if (sub.isNotBlank()) col.addView(TextView(this).apply {
            text = sub; textSize = 12.5f; setTextColor(S.SUB); isSingleLine = true; ellipsize = TextUtils.TruncateAt.END
            setPadding(0, dp(2), 0, 0)
        })
        r.addView(col, LinearLayout.LayoutParams(0, -2, 1f))
        if (!myLat.isNaN() && showDist) {
            r.addView(TextView(this).apply {
                text = S.distText(distM(myLat, myLng, lat, lng).toInt()); textSize = 13f; setTextColor(S.CYAN)
                setPadding(dp(10), 0, 0, 0)
            })
        }
        return r
    }

    private fun addRow(v: View) {
        listBox.addView(v, LinearLayout.LayoutParams(-1, -2).apply { bottomMargin = dp(8) })
    }

    private fun showTips(list: List<Tip>) {
        tips = list
        listBox.removeAllViews()
        if (list.isEmpty()) { showMessage("没有找到，换个说法试试"); return }
        listBox.addView(sectionLabel("搜索结果"))
        for (t in list.take(12)) {
            val p = t.point
            val sub = listOf(t.district ?: "", t.address ?: "").filter { it.isNotBlank() }.joinToString(" ")
            addRow(row(t.name ?: "", sub, p.latitude, p.longitude, S.PINK) {
                chooseDest(t.name ?: "目的地", sub, p.latitude, p.longitude)
            })
        }
    }

    private fun pickFirst() {
        val t = tips.firstOrNull()
        if (t != null && input.text.isNotBlank()) {
            val sub = listOf(t.district ?: "", t.address ?: "").filter { it.isNotBlank() }.joinToString(" ")
            chooseDest(t.name ?: "目的地", sub, t.point.latitude, t.point.longitude)
        }
    }

    // ---- 最近去过 ----

    private fun loadHistory(): JSONArray = try {
        JSONArray(getSharedPreferences("motonav", Context.MODE_PRIVATE).getString("nav_history", "[]"))
    } catch (_: Exception) { JSONArray() }

    private fun saveHistory(name: String, addr: String, lat: Double, lng: Double) {
        val old = loadHistory()
        val arr = JSONArray()
        arr.put(JSONObject().put("n", name).put("a", addr).put("la", lat).put("lo", lng))
        for (i in 0 until old.length()) {
            val o = old.optJSONObject(i) ?: continue
            if (o.optString("n") == name && distM(o.optDouble("la"), o.optDouble("lo"), lat, lng) < 50) continue
            if (arr.length() >= 12) break
            arr.put(o)
        }
        getSharedPreferences("motonav", Context.MODE_PRIVATE).edit().putString("nav_history", arr.toString()).apply()
    }

    private fun showHistory() {
        tips = emptyList()
        listBox.removeAllViews()
        // v4.4 导入高德分享的路线（高德里规划好的摩托车路线）
        if (!addingVia)
            addRow(row("粘贴高德分享的路线", "高德里规划好路线 → 分享 → 复制链接，再点这里", 0.0, 0.0, S.AMBER, showDist = false) { pasteShare() })
        val h = loadHistory()
        if (h.length() == 0) {
            listBox.addView(TextView(this).apply {
                text = "输入目的地开始搜索\n\n全键盘直接打字，按回车选第一个结果"; textSize = 14f; setTextColor(S.SUB)
                gravity = Gravity.CENTER; setPadding(dp(10), dp(30), dp(10), dp(30))
            })
            return
        }
        listBox.addView(sectionLabel("最近去过"))
        for (i in 0 until h.length()) {
            val o = h.optJSONObject(i) ?: continue
            val n = o.optString("n"); val a = o.optString("a")
            val la = o.optDouble("la"); val lo = o.optDouble("lo")
            addRow(row(n, a, la, lo, S.PURPLE) { chooseDest(n, a, la, lo) })
        }
        listBox.addView(TextView(this).apply {
            text = "清除记录"; textSize = 13f; setTextColor(S.SUB); gravity = Gravity.CENTER
            setPadding(0, dp(10), 0, dp(10))
            setOnClickListener {
                getSharedPreferences("motonav", Context.MODE_PRIVATE).edit().putString("nav_history", "[]").apply()
                showHistory()
            }
        })
    }

    // ================= 选路线 =================

    private fun buildSheet() {
        sheet = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            background = GradientDrawable().apply {
                setColor(S.CARD_SOLID)
                val r = S.dp(this@RoutePlanActivity, 22f)
                cornerRadii = floatArrayOf(r, r, r, r, 0f, 0f, 0f, 0f)
            }
            elevation = S.dp(this@RoutePlanActivity, 12f)
            visibility = View.GONE
            isClickable = true
        }
        // 第一行：目的地 + 开关
        val top = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL }
        val col = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        sheetTitle = TextView(this).apply {
            textSize = 18f; setTextColor(S.TEXT); typeface = S.BOLD; isSingleLine = true; ellipsize = TextUtils.TruncateAt.END
        }
        sheetSub = TextView(this).apply {
            textSize = 12f; setTextColor(S.SUB); maxLines = 2; ellipsize = TextUtils.TruncateAt.END
        }
        col.addView(sheetTitle); col.addView(sheetSub)
        top.addView(col, LinearLayout.LayoutParams(0, -2, 1f))
        hwChip = chip("不走高速") {
            avoidHighway = !avoidHighway
            getSharedPreferences("motonav", Context.MODE_PRIVATE).edit().putBoolean("avoid_hw", avoidHighway).apply()
            styleChips(); calculate()
        }
        emuChip = chip("模拟") { emulate = !emulate; styleChips() }
        top.addView(hwChip, LinearLayout.LayoutParams(-2, dp(32)).apply { leftMargin = dp(8) })
        top.addView(emuChip, LinearLayout.LayoutParams(-2, dp(32)).apply { leftMargin = dp(6) })
        sheet.addView(top)
        favoriteButton = TextView(this).apply {
            textSize = 13f; setTextColor(S.CYAN); gravity = Gravity.CENTER_VERTICAL; minHeight = dp(40)
            setOnClickListener {
                if (RideTrackMath.valid(destLat,destLng)) FavoritePlacesUi.save(this@RoutePlanActivity,destName,destAddr,destLat,destLng) { updateFavoriteButton() }
            }
        }
        sheet.addView(favoriteButton,LinearLayout.LayoutParams(-1,-2))
        modeChip = chip("交通方式：驾车") {
            if (shareRoute == null) {
                routeMode = if (routeMode == LocalRouting.BICYCLE) LocalRouting.DRIVE else LocalRouting.BICYCLE
                intent.putExtra(EXTRA_MODE, routeMode)
                getSharedPreferences("motonav", MODE_PRIVATE).edit().putInt("direct_route_mode",routeMode).apply()
                if (bicycle && vias.isNotEmpty()) {
                    clearVias(); Toast.makeText(this,"自行车采用直达规划，已清除途经点",Toast.LENGTH_LONG).show()
                }
                refreshMode(); refreshVias(); calculate()
            }
        }
        sheet.addView(modeChip,LinearLayout.LayoutParams(-1,dp(38)))
        // v4.9 途经点一行：“途经：A → B”  [+ 途经点] [清除]
        viaRow = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL }
        viaText = TextView(this).apply {
            textSize = 12.5f; setTextColor(S.SUB); isSingleLine = true; ellipsize = TextUtils.TruncateAt.END
        }
        viaRow.addView(viaText, LinearLayout.LayoutParams(0, -2, 1f))
        val viaAdd = chip("+ 途经点") { startViaSearch() }.apply {
            background = S.round(this@RoutePlanActivity, S.CARD2, 16f, S.STROKE); setTextColor(S.CYAN)
        }
        viaClear = chip("清除") { vias.clear(); refreshVias(); calculate() }.apply {
            background = S.round(this@RoutePlanActivity, S.CARD2, 16f, S.STROKE); setTextColor(S.SUB)
        }
        viaRow.addView(viaAdd, LinearLayout.LayoutParams(-2, dp(30)).apply { leftMargin = dp(8) })
        viaRow.addView(viaClear, LinearLayout.LayoutParams(-2, dp(30)).apply { leftMargin = dp(6) })
        sheet.addView(viaRow, LinearLayout.LayoutParams(-1, -2).apply { topMargin = dp(8) })
        // 规划中 / 失败提示
        statusText = TextView(this).apply {
            textSize = 14f; setTextColor(S.SUB); gravity = Gravity.CENTER; visibility = View.GONE
            minHeight = dp(86)
        }
        sheet.addView(statusText, LinearLayout.LayoutParams(-1, -2).apply { topMargin = dp(10) })
        // 路线卡片
        cardsRow = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; minimumHeight = dp(86) }
        sheet.addView(cardsRow, LinearLayout.LayoutParams(-1, -2).apply { topMargin = dp(10) })
        val offlineLabel=TextView(this).apply { textSize=12f; setTextColor(S.SUB); maxLines=3 }
        sheet.addView(offlineLabel,LinearLayout.LayoutParams(-1,-2).apply { topMargin=dp(8) })
        handler.post(object : Runnable {
            override fun run() { offlineLabel.text=MapFeeder.offlineStatus()+"\n"+MapFeeder.screenMapStatus(); handler.postDelayed(this,1000) }
        })
        // 开始导航
        startBtn = TextView(this).apply {
            text = "开始导航"; textSize = 18f; setTextColor(Color.WHITE); typeface = S.BOLD
            letterSpacing = 0.08f; gravity = Gravity.CENTER
            background = S.gradient(this@RoutePlanActivity, 16f)
            setOnClickListener { startNav() }
        }
        sheet.addView(startBtn, LinearLayout.LayoutParams(-1, dp(52)).apply { topMargin = dp(12) })
        amapShareBtn = chip("用高德打开分享") { shareRoute?.let { openAmapShare(it.sourceUrl, it.nativeUri) } }.apply {
            visibility = View.GONE; setTextColor(S.AMBER)
        }
        sheet.addView(amapShareBtn, LinearLayout.LayoutParams(-1, dp(40)))
        root.addView(sheet, FrameLayout.LayoutParams(-1, -2, Gravity.BOTTOM))
        styleChips()
    }

    private fun chip(t: String, onClick: () -> Unit) = TextView(this).apply {
        text = t; textSize = 13f; gravity = Gravity.CENTER; typeface = S.MEDIUM
        setPadding(dp(12), 0, dp(12), 0)
        setOnClickListener { onClick() }
    }

    private fun refreshMode() {
        modeChip.text = if (shareRoute != null) "高德分享：${shareRoute?.kind}" else "交通方式：${if (bicycle) "自行车" else "驾车"} · 点此切换"
        modeChip.isEnabled = shareRoute == null
        modeChip.background = S.round(this,S.CARD2,12f,S.STROKE)
        modeChip.setTextColor(S.CYAN)
        hwChip.visibility = if (bicycle) View.GONE else View.VISIBLE
    }

    private fun availablePaths(): Map<Int, AMapNaviPath> {
        val n = navi ?: return emptyMap()
        return try {
            if (bicycle) n.naviPath?.let { mapOf(0 to it) } ?: emptyMap()
            else n.naviPaths ?: emptyMap()
        } catch (_: Exception) { emptyMap() }
    }

    private fun styleChips() {
        for ((c, on) in listOf(hwChip to avoidHighway, emuChip to emulate)) {
            c.background = S.round(this, if (on) Color.parseColor("#33FF4FB4") else S.CARD2, 16f, if (on) S.PINK else S.STROKE)
            c.setTextColor(if (on) S.PINK else S.SUB)
        }
        emuChip.text = if (emulate) "模拟 ✓" else "模拟"
    }

    private fun buildLocButton() {
        locBtn = TextView(this).apply {
            text = "◎"; textSize = 22f; setTextColor(S.CYAN); gravity = Gravity.CENTER
            background = GradientDrawable().apply { shape = GradientDrawable.OVAL; setColor(S.CARD_SOLID); setStroke(dp(1), S.STROKE) }
            elevation = S.dp(this@RoutePlanActivity, 6f)
            visibility = View.GONE
            setOnClickListener {
                if (!myLat.isNaN()) aMap?.animateCamera(CameraUpdateFactory.newLatLngZoom(LatLng(myLat, myLng), 16f))
            }
        }
        root.addView(locBtn, FrameLayout.LayoutParams(dp(46), dp(46), Gravity.BOTTOM or Gravity.END).apply {
            rightMargin = dp(14); bottomMargin = dp(260)
        })
        sheet.addOnLayoutChangeListener { _, _, t, _, b, _, _, _, _ ->
            (locBtn.layoutParams as FrameLayout.LayoutParams).bottomMargin = (b - t) + dp(12)
            locBtn.requestLayout()
        }
    }

    private lateinit var favoriteButton: TextView
    private fun updateFavoriteButton() {
        favoriteButton.text = if (FavoritePlaces.find(this,destLat,destLng)==null) "☆ 收藏到常用目的地" else "★ 已收藏 · 改名 / 删除"
    }

    private fun chooseDest(name: String, addr: String, lat: Double, lng: Double, share: AmapShare.Route? = null) {
        if (addingVia && share == null) { addVia(name, lat, lng); return }     // v4.9 正在选途经点
        if (share == null) clearShare()                 // v4.4 自己选的目的地：不再按分享的路线走
        clearVias()                                     // v4.9 新目的地：途经点清空
        shareRoute = share
        refreshMode()
        amapShareBtn.visibility = if (!share?.sourceUrl.isNullOrBlank()) View.VISIBLE else View.GONE
        destName = name; destAddr = addr; destLat = lat; destLng = lng
        updateFavoriteButton()
        saveHistory(name, if (share != null) "" else addr, lat, lng)
        hideKeyboard()
        suppressSearch = true; input.setText(name); suppressSearch = false
        input.clearFocus()
        panel.visibility = View.GONE
        sheet.visibility = View.VISIBLE
        locBtn.visibility = View.VISIBLE
        sheetTitle.text = name
        sheetSub.setTextColor(S.SUB)
        sheetSub.text = addr.ifBlank { if (!myLat.isNaN()) "直线 ${S.distText(distM(myLat, myLng, lat, lng).toInt())}" else "" }
        destMarker?.remove()
        destMarker = aMap?.addMarker(MarkerOptions().position(LatLng(lat, lng))
            .icon(BitmapDescriptorFactory.fromBitmap(pin())).anchor(0.5f, 1f))
        refreshVias()
        calculate()
    }

    // ================= v4.9 途经点 =================

    private fun startViaSearch() {
        if (bicycle) return
        if (vias.size >= 3) { Toast.makeText(this, "最多 3 个途经点", Toast.LENGTH_SHORT).show(); return }
        addingVia = true
        sheet.visibility = View.GONE
        locBtn.visibility = View.GONE
        panel.visibility = View.VISIBLE
        suppressSearch = true; input.setText(""); suppressSearch = false
        input.hint = "搜索途经点（比如加油站）"
        showHistory()
        input.requestFocus()
        (getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager).showSoftInput(input, 0)
    }

    /** 选好了一个途经点：回到路线页重新规划 */
    private fun addVia(name: String, lat: Double, lng: Double) {
        vias.add(Via(name, lat, lng))
        endViaSearch()
        calculate()
    }

    private fun endViaSearch() {
        addingVia = false
        input.hint = "搜索目的地"
        hideKeyboard()
        suppressSearch = true; input.setText(destName); suppressSearch = false
        input.clearFocus()
        panel.visibility = View.GONE
        sheet.visibility = View.VISIBLE
        locBtn.visibility = View.VISIBLE
        refreshVias()
    }

    private fun clearVias() {
        vias.clear()
        for (m in viaMarkers) { try { m.remove() } catch (_: Throwable) {} }
        viaMarkers.clear()
    }

    /** 途经点那一行的文字、地图上的小标记 */
    private fun refreshVias() {
        for (m in viaMarkers) { try { m.remove() } catch (_: Throwable) {} }
        viaMarkers.clear()
        viaRow.visibility = if (shareRoute != null || bicycle) View.GONE else View.VISIBLE      // 分享的路线自带途经点
        viaText.text = if (vias.isEmpty()) "途经：无" else "途经：" + vias.joinToString(" → ") { it.name }
        viaClear.visibility = if (vias.isEmpty()) View.GONE else View.VISIBLE
        for ((i, v) in vias.withIndex()) {
            aMap?.addMarker(MarkerOptions().position(LatLng(v.lat, v.lng))
                .icon(BitmapDescriptorFactory.fromBitmap(viaDot(i + 1))).anchor(0.5f, 0.5f))?.let { viaMarkers.add(it) }
        }
    }

    /** 途经点标记：紫色圆点里写序号 */
    private fun viaDot(n: Int): Bitmap {
        val sz = dp(26)
        val b = Bitmap.createBitmap(sz, sz, Bitmap.Config.ARGB_8888)
        val c = Canvas(b)
        val p = Paint(Paint.ANTI_ALIAS_FLAG)
        p.color = Color.WHITE; c.drawCircle(sz / 2f, sz / 2f, sz / 2f, p)
        p.color = S.PURPLE; c.drawCircle(sz / 2f, sz / 2f, sz / 2f - dp(2), p)
        p.color = Color.WHITE; p.textSize = sz * 0.55f; p.textAlign = Paint.Align.CENTER; p.typeface = S.BOLD
        c.drawText("$n", sz / 2f, sz / 2f - (p.descent() + p.ascent()) / 2, p)
        return b
    }

    private fun toSearchMode() {
        clearRoutes()
        clearShare()
        clearVias()                                     // v4.9
        destMarker?.remove(); destMarker = null
        sheet.visibility = View.GONE
        locBtn.visibility = View.GONE
        panel.visibility = View.VISIBLE
        if (input.text.isNullOrBlank()) showHistory()
    }

    private fun goBack() {
        if (addingVia) { endViaSearch(); return }       // v4.9 不加途经点了，回到路线页
        if (sheet.visibility == View.VISIBLE) {
            if (fromHome) { finish(); return }          // v6.3 首页选的目的地：返回回到首页
            toSearchMode()
            input.requestFocus()
            input.setSelection(input.text.length)
        } else finish()
    }

    private fun showStatus(t: String, retry: Boolean = false) {
        statusText.text = t
        statusText.visibility = View.VISIBLE
        cardsRow.visibility = View.GONE
        startBtn.alpha = 0.35f; startBtn.isEnabled = false
        statusText.setOnClickListener(if (retry) View.OnClickListener { calculate() } else null)
    }

    private fun openAmapShare(url: String, nativeUri: String = "") {
        try {
            startActivity(Intent(Intent.ACTION_VIEW, android.net.Uri.parse(nativeUri.ifBlank { url })).setPackage("com.autonavi.minimap"))
        } catch (_: Exception) {
            try { startActivity(Intent(Intent.ACTION_VIEW, android.net.Uri.parse(url))) }
            catch (_: Exception) { Toast.makeText(this, "无法打开分享，请先安装高德地图", Toast.LENGTH_LONG).show() }
        }
    }

    private fun calculate(retry: Int = 1) {
        // The SDK's implicit-start overload can accept a request but never
        // complete when cold GPS has no origin. Wait for the map's GCJ fix.
        if(shareRoute==null && (!myLat.isFinite() || !myLng.isFinite())) {
            clearRoutes()
            navi=NaviBridge.attach(this)
            NaviBridge.listenForRoute(routeOwner) { _, _ -> }
            waitingForOrigin=true;waitingOriginRetry=retry
            val waitingSeq=calcSeq
            logPlan("waiting-origin")
            showStatus("正在获取当前位置，定位后自动规划…")
            handler.postDelayed({
                if(waitingForOrigin && waitingSeq==calcSeq && NaviBridge.ownsRoute(routeOwner) && !isDestroyed && !isFinishing)
                    showStatus("暂未取得当前位置，请检查定位权限和信号；定位恢复后将自动规划，或点这里重试",true)
            },20_000)
            return
        }
        val n = NaviBridge.attach(this)
        if (n == null) { showStatus("导航初始化失败：${NaviBridge.lastInfo}"); return }
        navi = n
        clearRoutes()
        calculating = true
        val seq = ++calcSeq
        showStatus(if (bicycle) "正在规划自行车路线…" else "正在规划驾车路线…")
        logPlan("calculate retry=$retry explicit-origin=${myLat.isFinite() && myLng.isFinite()}")
        NaviBridge.listenForRoute(routeOwner) { ok, res -> if(seq==calcSeq)onRouteResult(ok,res) }
        val sr = shareRoute
        NaviBridge.directRouteMode = if (bicycle) LocalRouting.BICYCLE else LocalRouting.DRIVE
        // v4.4 按分享的路线走：沿路挑途经点，不躲避拥堵（免得绕开原路线），只要一条路线
        val wps: ArrayList<NaviLatLng> = if (sr != null) ArrayList(if (sr.endpointsOnly) emptyList() else AmapShare.waypoints(sr, myLat, myLng))
            else ArrayList(vias.map { NaviLatLng(it.lat, it.lng) })                  // v4.9 自己加的途经点
        NaviBridge.realWaypoints = !bicycle && sr == null && vias.isNotEmpty()                  // v7.9 到途经点放特效
        val strategy = try {
            if (sr != null) n.strategyConvert(false, avoidHighway, false, false, false)
            else n.strategyConvert(true, avoidHighway, false, false, true)
        } catch (_: Exception) { 10 }
        if (sr != null) showStatus(if (sr.isTravel) "正在规划${sr.kind}路线…" else "正在按高德分享的路线规划（${wps.size} 个途经点）…")
        var startError = ""
        val started = try {
            if (sr != null) ShareRouting.calculate(n, sr, myLat, myLng, wps, strategy)
            else LocalRouting.calculate(n, routeMode,
                if (!myLat.isNaN() && !myLng.isNaN()) NaviLatLng(myLat,myLng) else null,
                NaviLatLng(destLat,destLng),wps,strategy)
        } catch (e: Exception) { startError = e.message.orEmpty(); false }
        logPlan("request-started=$started")
        if (!started) {
            calculating = false
            if (retry > 0) {                     // 导航引擎可能还没初始化好：1.5 秒后自动再试一次
                handler.postDelayed({ if (seq == calcSeq && sheet.visibility == View.VISIBLE && NaviBridge.ownsRoute(routeOwner)) calculate(retry - 1) }, 1500)
                return
            }
            showStatus("${if (bicycle) "自行车" else "驾车"}路线规划没能开始\n${startError.ifBlank { "请检查定位、网络和高德 Key 权限" }}\n点这里重试", true); return
        }
        handler.postDelayed({
            if (seq == calcSeq && calculating && !isFinishing && !isDestroyed && NaviBridge.ownsRoute(routeOwner)) {
                calculating = false
                logPlan("timeout retry=$retry")
                if(retry>0 && sheet.visibility==View.VISIBLE) {
                    showStatus("规划响应较慢，正在重新规划…")
                    calculate(retry-1)
                } else showStatus("规划超时，点这里重试",true)
            }
        }, 20_000)
    }

    private fun onRouteResult(ok: Boolean, res: Any?) {
        if (!calculating) return                  // 高德新旧两种回调可能都来一次，只处理第一次
        logPlan("result ok=$ok")
        calculating = false
        val n = navi ?: return
        if (!ok) {
            val code = Refl.int(res, "getErrorCode") ?: (res as? Int)
            val detail = Refl.str(res, "getErrorDetail", "getErrorDescription") ?: ""
            showStatus("${if (bicycle) "自行车" else "驾车"}路线规划失败（${code ?: "?"}）$detail\n${if (bicycle) "请检查高德 Key 的骑行算路权限、额度和网络；不会改用驾车路线。\n" else ""}点这里重试", true)
            return
        }
        val paths = availablePaths()
        val fromRes = (Refl.call(res, "getRouteid").second as? IntArray)?.filter { paths.containsKey(it) }
        val ids = (if (!fromRes.isNullOrEmpty()) fromRes else paths.keys.sorted()).take(3)
        if (ids.isEmpty()) { showStatus("没有可用的路线，点这里重试", true); return }
        routeIds = ids.toIntArray()
        val m = aMap ?: return
        for (id in ids) {
            val p = paths[id] ?: continue
            try {
                val ov = RouteOverLay(m, p, this)
                try { ov.setArrowOnRoute(false) } catch (_: Throwable) {}
                ov.addToMap()
                overlays[id] = ov
            } catch (_: Throwable) {}
        }
        statusText.visibility = View.GONE
        cardsRow.visibility = View.VISIBLE
        startBtn.alpha = 1f; startBtn.isEnabled = true
        select(ids[0])
        sheet.post { zoomToRoutes(paths) }
    }

    /** v4.4 比较 SDK 算出的路线和分享的路线，结果写在目的地下面 */
    private fun checkShareMatch(p: AMapNaviPath?) {
        val sr = shareRoute ?: return
        if (sr.endpointsOnly) return
        val coords = try { p?.coordList?.toList() } catch (_: Exception) { null } ?: return
        val seq = calcSeq
        val routeId = selected
        val la = myLat; val lo = myLng
        Thread {
            val (ratio, bad) = AmapShare.match(sr, coords, la, lo)
            runOnUiThread {
                if (seq != calcSeq || shareRoute !== sr || selected != routeId) return@runOnUiThread
                val pct = (ratio * 100).toInt()
                if (pct >= 95) {
                    sheetSub.text = "${destAddr}\n✓ 和分享的路线吻合 $pct%"
                    sheetSub.setTextColor(S.GREEN)
                } else {
                    sheetSub.text = "${destAddr}\n⚠ 和分享的路线吻合 $pct%，约 ${S.distText(bad)} 不一样（虚线是原路线）"
                    sheetSub.setTextColor(S.AMBER)
                }
            }
        }.start()
    }

    private fun select(id: Int) {
        selected = id
        for ((k, o) in overlays) {
            try { o.setTransparency(if (k == id) 1f else 0.35f); o.setZindex(if (k == id) 10 else 1) } catch (_: Throwable) {}
        }
        if (!bicycle) try { navi?.selectRouteId(id) } catch (_: Exception) {}
        buildCards()
        checkShareMatch(availablePaths()[id])
        try {
            val p=availablePaths()[id]
            val coords=p?.coordList
            if (p!=null && coords!=null && coords.size>=2)
                MapFeeder.prepareRoute(this,"${p.pathid}:${p.allLength}",coords.map { doubleArrayOf(it.latitude,it.longitude) })
        } catch (_: Exception) { }
    }

    private fun buildCards() {
        cardsRow.removeAllViews()
        val paths = availablePaths()
        routeIds.forEachIndexed { i, id ->
            val p = paths[id] ?: return@forEachIndexed
            val sel = id == selected
            val card = LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
                minimumHeight = dp(86)
                gravity = Gravity.CENTER_VERTICAL
                setPadding(dp(10), dp(8), dp(8), dp(8))
                background = if (sel) GlowBorder.make(this@RoutePlanActivity, 14f) else S.round(this@RoutePlanActivity, S.CARD2, 14f)
                setOnClickListener { select(id) }
            }
            val label = (Refl.str(p, "getLabels") ?: "").split(",", "，", " ").firstOrNull { it.isNotBlank() }
                ?: if (i == 0) "推荐" else "方案${i + 1}"
            card.addView(TextView(this).apply {
                text = label; textSize = 11.5f; setTextColor(if (sel) S.PINK else S.SUB); typeface = S.MEDIUM
                isSingleLine = true; ellipsize = TextUtils.TruncateAt.END
            })
            card.addView(TextView(this).apply {
                text = S.durText(p.allTime); textSize = 19f; setTextColor(S.TEXT); typeface = S.NUM
                isSingleLine = true
            })
            val lights = Refl.int(p, "getTrafficLightCount", "getTrafficLightNumber")
            val toll = try { p.tollCost } catch (_: Exception) { 0 }
            val info = buildString {
                append(S.distText(p.allLength))
                if (lights != null && lights > 0) append(" · ${lights}灯")
                if (toll > 0) append(" · ¥$toll")
            }
            card.addView(TextView(this).apply {
                text = info; textSize = 11.5f; setTextColor(S.SUB); isSingleLine = true; ellipsize = TextUtils.TruncateAt.END
            })
            cardsRow.addView(card, LinearLayout.LayoutParams(0, -2, 1f).apply {
                if (i > 0) leftMargin = dp(8)
            })
        }
    }

    private fun zoomToRoutes(paths: Map<Int, AMapNaviPath>) {
        val m = aMap ?: return
        try {
            val b = LatLngBounds.builder()
            var any = false
            for (id in routeIds) {
                val cl = paths[id]?.coordList ?: continue
                val step = max(1, cl.size / 200)
                for (k in cl.indices step step) { b.include(LatLng(cl[k].latitude, cl[k].longitude)); any = true }
            }
            if (!myLat.isNaN()) b.include(LatLng(myLat, myLng))
            b.include(LatLng(destLat, destLng))
            if (!any) return
            m.animateCamera(CameraUpdateFactory.newLatLngBoundsRect(b.build(),
                dp(36), dp(36), insetTop + dp(84), sheet.height + dp(28)))
        } catch (_: Exception) {}
    }

    private fun clearRoutes() {
        calcSeq++
        waitingForOrigin=false
        calculating = false
        for (o in overlays.values) { try { o.removeFromMap(); o.destroy() } catch (_: Throwable) {} }
        overlays.clear()
        routeIds = IntArray(0); selected = -1
        cardsRow.removeAllViews()
    }

    private fun startNav() {
        if (selected < 0 || starting) return
        starting = true
        if (!bicycle) try { navi?.selectRouteId(selected) } catch (_: Exception) {}
        NaviBridge.releaseRoute(routeOwner)
        ShareGuide.start(shareRoute, this, avoidHighway, myLat, myLng)          // v4.5 偏航跟随原路线 + 原路线细线
        startActivity(Intent(this, NavActivity::class.java).putExtra("emu", emulate).putExtra("dest", destName))
        finish()
    }

    // ================= 其他 =================

    private fun hideKeyboard() {
        (getSystemService(Context.INPUT_METHOD_SERVICE) as InputMethodManager).hideSoftInputFromWindow(input.windowToken, 0)
    }

    private fun distM(lat1: Double, lng1: Double, lat2: Double, lng2: Double): Double {
        val k = Math.PI / 180
        val x = (lng2 - lng1) * k * cos((lat1 + lat2) / 2 * k)
        val y = (lat2 - lat1) * k
        return sqrt(x * x + y * y) * 6_371_000.0
    }

    override fun onResume() { super.onResume(); mapView.onResume() }
    override fun onPause() { super.onPause(); mapView.onPause() }
    override fun onSaveInstanceState(outState: Bundle) { outState.putInt("direct_route_mode", routeMode); super.onSaveInstanceState(outState); mapView.onSaveInstanceState(outState) }
    override fun onDestroy() {
        handler.removeCallbacksAndMessages(null)
        logPlan("destroy starting=$starting")
        NaviBridge.releaseRoute(routeOwner)
        sharePoly?.remove()
        for (o in overlays.values) { try { o.removeFromMap() } catch (_: Throwable) {} }
        overlays.clear()
        mapView.onDestroy()
        super.onDestroy()
    }
}

/** v6.3 “最近去过”：首页和搜索页共用（存在 motonav / nav_history） */
object NavHistory {
    class Item(val name: String, val addr: String, val lat: Double, val lng: Double)
    fun load(c: Context): List<Item> = try {
        val a = JSONArray(c.getSharedPreferences("motonav", Context.MODE_PRIVATE).getString("nav_history", "[]"))
        (0 until a.length()).mapNotNull { i ->
            a.optJSONObject(i)?.let { Item(it.optString("n"), it.optString("a"), it.optDouble("la"), it.optDouble("lo")) }
        }
    } catch (_: Exception) { emptyList() }
    fun clear(c: Context) {
        c.getSharedPreferences("motonav", Context.MODE_PRIVATE).edit().putString("nav_history", "[]").apply()
    }
}

/**
 * v4.4 接收别的 App（高德）分享过来的文字：转交给 RoutePlanActivity，放在 MotoNav 自己的任务里打开
 * （不然会叠在高德的界面里），然后马上关掉自己
 */
class ShareActivity : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val text = intent?.getStringExtra(Intent.EXTRA_TEXT) ?: intent?.dataString
        if (text != null) {
            startActivity(Intent(this, RoutePlanActivity::class.java)
                .putExtra(RoutePlanActivity.EXTRA_SHARE, text)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP))
        }
        finish()
    }
}
