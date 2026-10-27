package com.example.motonav

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.text.Editable
import android.text.TextWatcher
import android.view.Gravity
import android.view.View
import android.view.inputmethod.EditorInfo
import android.widget.*
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.widget.TextViewCompat
import com.amap.api.maps.CameraUpdateFactory
import com.amap.api.maps.MapView
import com.amap.api.maps.model.*
import com.amap.api.services.core.LatLonPoint
import com.amap.api.services.help.Inputtips
import com.amap.api.services.help.InputtipsQuery
import com.amap.api.services.help.Tip
import java.util.concurrent.Executors
import java.util.concurrent.Future

/** Native endpoint search and route comparison; the route provider remains Amap's motorcycle web service. */
class MotorWebPlannerActivity : AppCompatActivity() {
    private val S = NavStyle
    private fun dp(v: Int) = S.dpi(this, v)
    private val main = Handler(Looper.getMainLooper())
    private val worker = Executors.newFixedThreadPool(2)
    private var task: Future<*>? = null
    private var generation = 0
    private var searchGeneration = 0
    private lateinit var map: MapView
    private lateinit var fields: LinearLayout
    private lateinit var compact: TextView
    private lateinit var startText: TextView
    private lateinit var endText: TextView
    private lateinit var status: TextView
    private lateinit var plan: TextView
    private lateinit var alternatives: LinearLayout
    private lateinit var actions: LinearLayout
    private var start: MotorPoint? = null
    private var end: MotorPoint? = null
    private var currentStart = true
    private var fix: MotorPoint? = null
    private var fixAt = 0L
    private var centered = false
    private var routes = emptyList<WebMotorRoute>()
    private var loadedRequest: MotorRequest? = null
    private var selected = -1
    private val lines = ArrayList<Polyline>()
    private val markers = ArrayList<Marker>()
    private var pendingStart: Pair<WebMotorRoute, Boolean>? = null
    private var starting = false
    private var browserOpened = false

    override fun onCreate(state: Bundle?) {
        super.onCreate(state)
        supportActionBar?.hide()
        NaviBridge.initPrivacy(this)
        WindowCompat.setDecorFitsSystemWindows(window, false)
        window.statusBarColor = S.BG
        window.navigationBarColor = S.BG
        val root = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setBackgroundColor(S.BG) }
        ViewCompat.setOnApplyWindowInsetsListener(root) { view, ins ->
            val bars = ins.getInsets(WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.displayCutout())
            view.setPadding(bars.left, bars.top, bars.right, bars.bottom); ins
        }
        val toolbar = LinearLayout(this).apply { gravity = Gravity.CENTER_VERTICAL }
        toolbar.addView(button("返回") { finish() }, LinearLayout.LayoutParams(dp(62), dp(44)))
        toolbar.addView(label("摩托车路线规划", 17f, S.TEXT).apply { isSingleLine = true; ellipsize = android.text.TextUtils.TruncateAt.END }, LinearLayout.LayoutParams(0, -2, 1f))
        toolbar.addView(button("账号") { openBrowser() }, LinearLayout.LayoutParams(dp(68), dp(44)))
        root.addView(toolbar)
        fields = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(12), 0, dp(12), 0) }
        startText = endpointRow(fields, "起点") { searchPlace(true) }
        endText = endpointRow(fields, "终点") { searchPlace(false) }
        val controls = LinearLayout(this)
        controls.addView(button("交换起终点") { swap() }, LinearLayout.LayoutParams(0, dp(40), 1f))
        plan = button("规划摩托车路线") { calculate() }.apply { background = S.round(this@MotorWebPlannerActivity, S.PINK, 12f); setTextColor(Color.WHITE) }
        controls.addView(plan, LinearLayout.LayoutParams(0, dp(40), 2f))
        fields.addView(controls)
        root.addView(fields)
        compact = label("", 13f, S.TEXT).apply {
            setPadding(dp(14), dp(7), dp(14), dp(7)); visibility = View.GONE
            setOnClickListener { showFields() }
        }
        root.addView(compact)
        status = label("选择起终点后规划，在地图上对比路线", 12f, S.SUB).apply {
            setPadding(dp(14), dp(7), dp(14), dp(7)); setTextIsSelectable(true)
        }
        root.addView(status)
        map = MapView(this); map.onCreate(state)
        map.map.uiSettings.apply { isZoomControlsEnabled = false; isTiltGesturesEnabled = false; isScaleControlsEnabled = true }
        map.map.setOnPolylineClickListener { line -> lines.indexOf(line).takeIf { it >= 0 }?.let { selectRoute(it) } }
        map.map.setOnMapLoadedListener { if (routes.isNotEmpty()) fitRoute() }
        root.addView(map, LinearLayout.LayoutParams(-1, 0, 1f))
        alternatives = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; setPadding(dp(8), dp(6), dp(8), dp(4)) }
        root.addView(HorizontalScrollView(this).apply { isHorizontalScrollBarEnabled = false; addView(alternatives) })
        actions = LinearLayout(this).apply { setPadding(dp(8), 0, dp(8), dp(8)); visibility = View.GONE }
        actions.addView(button("模拟导航") { begin(true) }, LinearLayout.LayoutParams(0, dp(44), 1f))
        actions.addView(button("开始导航") { begin(false) }.apply {
            background = S.round(this@MotorWebPlannerActivity, S.PINK, 12f); setTextColor(Color.WHITE)
        }, LinearLayout.LayoutParams(0, dp(44), 2f))
        root.addView(actions)
        setContentView(root)
        val saved = state?.getString("motor_request") ?: intent.getStringExtra(EXTRA_REQUEST_URL)
        if (!saved.isNullOrBlank()) runCatching { WebMotorRoutes.request(saved) }.onSuccess {
            start = it.start; end = it.end; currentStart = state?.getBoolean("current_start", false) ?: false
        }
        if (state != null) {
            fun restore(prefix: String): MotorPoint? = if (state.containsKey(prefix + "_lat"))
                MotorPoint(state.getDouble(prefix + "_lat"), state.getDouble(prefix + "_lng"), state.getString(prefix + "_name").orEmpty(),state.getString(prefix + "_id").orEmpty()) else null
            start = restore("start") ?: start; end = restore("end") ?: end
            currentStart = state.getBoolean("current_start", true)
        }
        refreshFields()
        setupLocation()
        if (!Keys.hasAmap(this)) status.text = "请先在首页「地图 Key」配置高德 Android Key，以使用地点搜索和地图"
    }

    private fun label(value: String, size: Float = 14f, color: Int = S.TEXT) = TextView(this).apply {
        text = value; textSize = size; setTextColor(color)
    }
    private fun button(value: String, click: () -> Unit) = label(value, 14f, S.CYAN).apply {
        gravity = Gravity.CENTER; isSingleLine = true
        TextViewCompat.setAutoSizeTextTypeUniformWithConfiguration(this, 10, 14, 1, android.util.TypedValue.COMPLEX_UNIT_SP)
        setPadding(dp(6), dp(4), dp(6), dp(4)); setOnClickListener { click() }
    }
    private fun endpointRow(parent: LinearLayout, title: String, click: () -> Unit): TextView {
        val row = LinearLayout(this).apply { gravity = Gravity.CENTER_VERTICAL }
        row.addView(label(title, 13f, S.SUB).apply { gravity = Gravity.CENTER_VERTICAL }, LinearLayout.LayoutParams(dp(42), dp(43)))
        val text = label("", 15f).apply { gravity = Gravity.CENTER_VERTICAL; maxLines = 1; setOnClickListener { click() } }
        row.addView(text, LinearLayout.LayoutParams(0, dp(43), 1f))
        row.addView(button("选择", click), LinearLayout.LayoutParams(dp(58), dp(43)))
        parent.addView(row)
        return text
    }

    private fun refreshFields() {
        startText.text = if (currentStart) "我的位置" else start?.name?.ifBlank { "已选起点" } ?: "输入出发地点"
        endText.text = end?.name?.ifBlank { "已选目的地" } ?: "输入目的地"
        compact.text = "${startText.text} → ${endText.text}   · 修改"
    }
    private fun showFields() { fields.visibility = View.VISIBLE; compact.visibility = View.GONE }
    private fun invalidatePlan() {
        generation++; task?.cancel(true); task = null
        routes = emptyList(); loadedRequest = null; selected = -1
        lines.forEach { it.remove() }; lines.clear(); markers.forEach { it.remove() }; markers.clear()
        alternatives.removeAllViews(); actions.visibility = View.GONE
        starting = false; pendingStart = null; plan.isEnabled = true
        status.text = "起终点已更新，请重新规划"; showFields(); refreshFields()
    }
    private fun swap() {
        val from = if (currentStart) freshFix() else start
        val to = end
        if (from == null || to == null) { status.text = "请先选好起终点，当前位置还未获取时可手动选择起点"; return }
        start = to; end = from; currentStart = false; invalidatePlan()
    }
    private fun freshFix(): MotorPoint? = fix?.takeIf { android.os.SystemClock.elapsedRealtime() - fixAt <= 60000 }
    private fun setupLocation() {
        if (checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) return
        map.map.myLocationStyle = MyLocationStyle().myLocationType(MyLocationStyle.LOCATION_TYPE_LOCATION_ROTATE_NO_CENTER)
            .interval(2500).strokeColor(Color.TRANSPARENT).radiusFillColor(0x2243E1F0)
        map.map.setOnMyLocationChangeListener { loc ->
            if (loc != null && MotorTrack.valid(loc.latitude, loc.longitude) && loc.accuracy in 0f..300f &&
                System.currentTimeMillis() - loc.time in -2000..60000) {
                fix = MotorPoint(loc.latitude, loc.longitude, "我的位置"); fixAt = android.os.SystemClock.elapsedRealtime()
                if (!centered && routes.isEmpty()) { centered = true; map.map.moveCamera(CameraUpdateFactory.newLatLngZoom(LatLng(loc.latitude,loc.longitude),14f)) }
            }
        }
        map.map.isMyLocationEnabled = true
    }

    private fun searchPlace(origin: Boolean) {
        val box = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(18), dp(8), dp(18), 0) }
        val input = EditText(this).apply { hint = if(origin) "输入起点名称" else "输入目的地名称"; isSingleLine = true; imeOptions = EditorInfo.IME_ACTION_SEARCH }
        val row = LinearLayout(this)
        row.addView(input, LinearLayout.LayoutParams(0, dp(48), 1f))
        val search = button("搜索") {}
        row.addView(search, LinearLayout.LayoutParams(dp(60), dp(48))); box.addView(row)
        val note = label("输入名称后点搜索，再从结果中选择具体地点", 12f, S.SUB); box.addView(note)
        val list = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        box.addView(ScrollView(this).apply { addView(list) }, LinearLayout.LayoutParams(-1, dp(225)))
        val dialog = AlertDialog.Builder(this).setTitle(if(origin) "选择起点" else "选择目的地").setView(box).setNegativeButton("取消", null).create()
        fun choose(point: MotorPoint) {
            if (origin) { start = point; currentStart = false } else end = point
            invalidatePlan(); dialog.dismiss()
        }
        if (origin) {
            val current = button("使用我的位置") {
                currentStart = true; start = null; invalidatePlan(); dialog.dismiss()
                if (checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED)
                    requestPermissions(arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION), 71)
                else setupLocation()
            }
            box.addView(current, 1, LinearLayout.LayoutParams(-1, dp(42)))
        }
        input.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, st: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, st: Int, before: Int, count: Int) { searchGeneration++; list.removeAllViews() }
            override fun afterTextChanged(s: Editable?) {}
        })
        fun searchNow() {
            val kw = input.text.toString().trim(); if (kw.isEmpty()) return
            val seq = ++searchGeneration
            list.removeAllViews(); note.text = "正在搜索…"
            runCatching {
                val query = InputtipsQuery(kw, "").apply { setCityLimit(false); freshFix()?.let { setLocation(LatLonPoint(it.lat,it.lng)) } }
                Inputtips(this, query).apply {
                    setInputtipsListener(object : Inputtips.InputtipsListener {
                        override fun onGetInputtips(result: MutableList<Tip>?, code: Int) {
                            if (isDestroyed || !dialog.isShowing || seq != searchGeneration || input.text.toString().trim() != kw) return
                            list.removeAllViews()
                            if (code != 1000) { note.text = "搜索失败（$code），请检查网络和高德 Key"; return }
                            val points = result.orEmpty().filter { it.point != null && MotorTrack.valid(it.point.latitude,it.point.longitude) && (it.point.latitude != 0.0 || it.point.longitude != 0.0) }
                            note.text = if (points.isEmpty()) "没有找到具体地点，请补充城市或更完整的名称" else "请选择具体地点"
                            points.forEach { tip ->
                                list.addView(label("${tip.name}\n${tip.district.orEmpty()} ${tip.address.orEmpty()}", 14f).apply {
                                    setPadding(dp(4), dp(10), dp(4), dp(10))
                                    setOnClickListener { choose(MotorPoint(tip.point.latitude,tip.point.longitude,tip.name.orEmpty(),tip.poiID.orEmpty())) }
                                })
                            }
                        }
                    }); requestInputtipsAsyn()
                }
            }.onFailure { note.text = "地点搜索失败：${it.message}" }
        }
        search.setOnClickListener { searchNow() }
        input.setOnEditorActionListener { _, action, event ->
            if (action == EditorInfo.IME_ACTION_SEARCH || (event?.keyCode == android.view.KeyEvent.KEYCODE_ENTER && event.action == android.view.KeyEvent.ACTION_UP)) { searchNow(); true } else false
        }
        dialog.setOnDismissListener { searchGeneration++ }
        dialog.show()
    }

    private fun calculate() {
        val from = if (currentStart) freshFix() else start
        val to = end
        if (from == null) {
            status.text = if (currentStart) "正在等待当前位置；也可以点起点手动选择出发地点" else "请先选择起点"
            if (currentStart && checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED)
                requestPermissions(arrayOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION), 71)
            return
        }
        if (to == null) { searchPlace(false); return }
        if (MotorTrack.distance(from.lat,from.lng,to.lat,to.lng) < 20) { status.text = "起终点过近，请重新选择"; return }
        val request = MotorRequest(from,to)
        val seq = ++generation
        starting = false; pendingStart = null
        task?.cancel(true); routes = emptyList(); loadedRequest = null; selected = -1
        lines.forEach { it.remove() }; lines.clear(); markers.forEach { it.remove() }; markers.clear()
        alternatives.removeAllViews(); actions.visibility = View.GONE
        status.text = "正在规划摩托车路线…"; plan.isEnabled = false
        task = worker.submit {
            val result = runCatching { WebMotorRoutes.load(request) }
            main.post {
                if (isFinishing || isDestroyed || generation != seq) return@post
                plan.isEnabled = true
                result.onSuccess {
                    routes = it; loadedRequest = request
                    fields.visibility = View.GONE; compact.visibility = View.VISIBLE; refreshFields()
                    drawRoutes(); selectRoute(0); actions.visibility = View.VISIBLE
                    status.text = "${it.size} 条摩托车路线 · 点卡片或地图路线切换"
                    android.util.Log.i("MotoNavMotorPlan", "loaded alternatives=${it.size} points=${it.joinToString { r -> r.points.size.toString() }}")
                }.onFailure {
                    status.text = "${it.message ?: "路线规划失败"}。可重试，或点右上角「账号」检查登录与验证。"
                }
            }
        }
    }

    private fun drawRoutes() {
        routes.forEach { route ->
            val coordinates = route.points.map { LatLng(it[0],it[1]) }
            lines.add(map.map.addPolyline(PolylineOptions().addAll(coordinates).width(dp(5).toFloat()).color(0xff7d8596.toInt())))
        }
        loadedRequest?.let { req ->
            markers.add(map.map.addMarker(MarkerOptions().position(LatLng(req.start.lat,req.start.lng)).title("起点 · ${req.start.name}")))
            markers.add(map.map.addMarker(MarkerOptions().position(LatLng(req.end.lat,req.end.lng)).title("终点 · ${req.end.name}")))
        }
    }
    private fun selectRoute(index: Int) {
        if (index !in routes.indices) return
        selected = index
        android.util.Log.i("MotoNavMotorPlan", "selected=${index+1} distance=${routes[index].distance} seconds=${routes[index].seconds}")
        lines.forEachIndexed { i, line -> line.color = if(i == index) S.PINK else 0xff7d8596.toInt(); line.zIndex = if(i == index) 10f else 0f; line.width = dp(if(i == index) 7 else 4).toFloat() }
        alternatives.removeAllViews()
        routes.forEachIndexed { i, route ->
            val minutes = (route.seconds + 59) / 60
            val time = if(minutes >= 60) "${minutes/60}小时${minutes%60}分" else "${minutes}分钟"
            alternatives.addView(label("${if(i == index) "● " else ""}方案 ${i+1}  $time\n${"%.1f".format(route.distance/1000.0)} 公里 · ${route.lights} 个红绿灯", 13f, if(i == index) S.CYAN else S.TEXT).apply {
                setPadding(dp(12),dp(9),dp(12),dp(9)); background = S.round(this@MotorWebPlannerActivity, if(i == index) 0xff263043.toInt() else S.CARD_SOLID, 12f)
                setOnClickListener { selectRoute(i) }
            },LinearLayout.LayoutParams(-2,-2).apply { rightMargin = dp(6) })
        }
        fitRoute()
    }
    private fun fitRoute() {
        val route = routes.getOrNull(selected) ?: return
        map.post {
            if (isDestroyed || map.width <= dp(80) || map.height <= dp(80)) return@post
            runCatching {
                val bounds = LatLngBounds.builder(); route.points.forEach { bounds.include(LatLng(it[0],it[1])) }
                map.map.animateCamera(CameraUpdateFactory.newLatLngBounds(bounds.build(),dp(28)))
            }
        }
    }
    private fun openBrowser() {
        browserOpened = true
        startActivity(Intent(this, MotorWebActivity::class.java).apply {
            val from = if(currentStart) freshFix() else start
            val request = loadedRequest ?: if(from != null && end != null) MotorRequest(from,end!!) else null
            request?.pageUrl()?.let { putExtra("initial_motor_url", it) }
        })
    }
    private fun begin(emulate: Boolean) {
        val chosen = routes.getOrNull(selected) ?: return
        if (starting) return
        pendingStart = chosen to emulate
        val needed = mutableListOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION)
        if (Build.VERSION.SDK_INT >= 31) needed.addAll(listOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT))
        val missing = needed.filter { checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }
        if (missing.isNotEmpty()) { requestPermissions(missing.toTypedArray(),72); return }
        if (!emulate && !Keys.hasAmap(this)) { status.text = "真实导航需要先在首页配置高德 Android Key"; return }
        starting = true; status.text = "正在启动导航…"
        NavService.start(this)
        waitForService(chosen,emulate,0)
    }
    private fun waitForService(route: WebMotorRoute, emulate: Boolean, attempt: Int) {
        if (isFinishing || isDestroyed || !starting || loadedRequest != route.request) return
        if (!NavService.running) {
            if (attempt >= 20) { starting = false; status.text = "后台服务未启动，请检查定位和蓝牙权限"; return }
            main.postDelayed({ waitForService(route,emulate,attempt+1) },200); return
        }
        runCatching { WebMotorGuide.start(this,route,emulate) }
            .onSuccess { starting = false; pendingStart = null; status.text = "${routes.size} 条摩托车路线 · 已选方案 ${selected+1}"; android.util.Log.i("MotoNavMotorPlan","started simulation=$emulate selected=${selected+1} distance=${route.distance}"); startActivity(Intent(this,MotorWebNavActivity::class.java)) }
            .onFailure { starting = false; status.text = "导航启动失败：${it.message}"; WebMotorGuide.stop() }
    }
    override fun onRequestPermissionsResult(code: Int, permissions: Array<out String>, results: IntArray) {
        super.onRequestPermissionsResult(code,permissions,results)
        if (code == 71) setupLocation()
        if (code == 72 && results.isNotEmpty() && results.all { it == PackageManager.PERMISSION_GRANTED }) pendingStart?.let { begin(it.second) }
    }
    override fun onSaveInstanceState(state: Bundle) {
        val from = if(currentStart) freshFix() else start
        if (from != null && end != null) state.putString("motor_request",MotorRequest(from,end!!).pageUrl())
        fun store(prefix: String, point: MotorPoint?) { point?.let {
            state.putDouble(prefix + "_lat",it.lat); state.putDouble(prefix + "_lng",it.lng)
            state.putString(prefix + "_name",it.name); state.putString(prefix + "_id",it.id)
        } }
        store("start",start); store("end",end)
        state.putBoolean("current_start",currentStart); map.onSaveInstanceState(state); super.onSaveInstanceState(state)
    }
    override fun onResume() {
        super.onResume(); map.onResume()
        if (browserOpened) { browserOpened = false; status.text = "已返回规划页，可重新规划以使用当前登录状态" }
        if (WebMotorGuide.active && routes.isEmpty()) {
            status.text = "已有摩托车导航运行中 · 点这里继续"
            status.setOnClickListener { startActivity(Intent(this,MotorWebNavActivity::class.java)) }
        } else status.setOnClickListener(null)
    }
    override fun onPause() { map.onPause(); super.onPause() }
    override fun onDestroy() {
        generation++; searchGeneration++; starting = false; task?.cancel(true); worker.shutdownNow()
        main.removeCallbacksAndMessages(null); map.onDestroy(); super.onDestroy()
    }
    companion object { const val EXTRA_REQUEST_URL = "native_motor_request_url" }
}
