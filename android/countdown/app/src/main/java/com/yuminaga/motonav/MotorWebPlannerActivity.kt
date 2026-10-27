package com.yuminaga.motonav

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
    private lateinit var favorite: TextView
    private lateinit var plan: TextView
    private lateinit var viaSummary: TextView
    private var vias:List<MotorPoint> = emptyList()
    private lateinit var alternatives: LinearLayout
    private lateinit var actions: LinearLayout
    private lateinit var title: TextView
    private lateinit var backButton: TextView
    private lateinit var accountButton: TextView
    private lateinit var summary: TextView
    private lateinit var routeScroll: ScrollView
    private lateinit var speedRow: LinearLayout
    private val speedButtons=linkedMapOf<Int,TextView>()
    private var plannedSpeed=60
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
            view.setPadding(bars.left+dp(8), bars.top+dp(8), bars.right+dp(8), bars.bottom+dp(8)); ins
        }
        val head=card()
        val toolbar = LinearLayout(this).apply { gravity = Gravity.CENTER_VERTICAL }
        backButton=button("返回") { finish() }
        toolbar.addView(backButton, LinearLayout.LayoutParams(dp(44), dp(40)).apply{rightMargin=dp(8)})
        title=label("摩托车路线规划",22f).apply{maxLines=2;typeface=S.MEDIUM}
        toolbar.addView(title,LinearLayout.LayoutParams(0,-2,1f))
        accountButton=button("账号") { openBrowser() }
        toolbar.addView(accountButton,LinearLayout.LayoutParams(dp(44),dp(40)).apply{leftMargin=dp(8)})
        head.addView(toolbar)
        fields = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(0,dp(6),0,0) }
        startText = endpointRow(fields, "起点") { searchPlace(true) }
        viaSummary=button(RouteWaypointEditor.summary(vias)){
            RouteWaypointEditor.show(this,vias,if(currentStart)freshFix() else start){points->vias=points;invalidatePlan()}
        }.apply{contentDescription="编辑摩托车途经点"}
        fields.addView(viaSummary,LinearLayout.LayoutParams(-1,dp(38)).apply{topMargin=dp(3);bottomMargin=dp(3)})
        endText = endpointRow(fields, "终点") { searchPlace(false) }
        val controls = LinearLayout(this)
        controls.addView(button("交换起终点") { swap() }, LinearLayout.LayoutParams(0, dp(42), 1f).apply{rightMargin=dp(8)})
        plan = button("规划摩托车路线") { calculate() }.apply { background = S.round(this@MotorWebPlannerActivity, S.PINK, 12f); setTextColor(Color.WHITE) }
        controls.addView(plan, LinearLayout.LayoutParams(0, dp(42), 2f))
        fields.addView(controls,LinearLayout.LayoutParams(-1,-2).apply{topMargin=dp(6)})
        head.addView(fields)
        compact = label("", 12f, S.CYAN).apply {
            setPadding(0,dp(7),0,0);maxLines=1;ellipsize=android.text.TextUtils.TruncateAt.END;visibility=View.GONE
            setOnClickListener { showFields() }
        }
        head.addView(compact)
        status = label("选择起终点后规划，在地图上对比路线", 11f, S.SUB).apply {
            setPadding(0,dp(7),0,0);maxLines=3;setTextIsSelectable(true)
        }
        head.addView(status);root.addView(head)
        map = MapView(this); map.onCreate(state)
        map.map.uiSettings.apply { isZoomControlsEnabled = false; isTiltGesturesEnabled = false; isScaleControlsEnabled = true }
        map.map.setOnPolylineClickListener { line -> lines.indexOf(line).takeIf { it >= 0 }?.let { selectRoute(it) } }
        map.map.setOnMapLoadedListener { if (routes.isNotEmpty()) fitRoute() }
        root.addView(map, LinearLayout.LayoutParams(-1, 0, 1f).apply{topMargin=dp(8);bottomMargin=dp(8)})
        val foot=card()
        summary=label("",17f).apply{typeface=S.MEDIUM;setPadding(0,0,0,dp(10));visibility=View.GONE};foot.addView(summary)
        alternatives = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        routeScroll=ScrollView(this).apply{isFillViewport=false;isVerticalScrollBarEnabled=true;addView(alternatives);visibility=View.GONE}
        val heightDp=resources.displayMetrics.heightPixels/resources.displayMetrics.density
        foot.addView(routeScroll,LinearLayout.LayoutParams(-1,dp(when{heightDp>=800->326;heightDp>=700->214;else->132})))
        plannedSpeed=SimulationSpeedPolicy.normalize(state?.getInt("planned_speed") ?: getSharedPreferences("motonav",0).getInt("motor_simulation_speed",60))
        speedRow=LinearLayout(this).apply{gravity=Gravity.CENTER_VERTICAL;visibility=View.GONE}
        speedRow.addView(label("模拟速度",11f,S.SUB),LinearLayout.LayoutParams(dp(65),-2))
        listOf(60,100,120).forEach{kmh->
            val item=button("$kmh km/h"){
                plannedSpeed=kmh;updateSpeedButtons()
                getSharedPreferences("motonav",0).edit().putInt("motor_simulation_speed",kmh).apply()
            }
            speedButtons[kmh]=item
            speedRow.addView(item,LinearLayout.LayoutParams(0,dp(36),1f).apply{leftMargin=dp(5)})
        }
        foot.addView(speedRow,LinearLayout.LayoutParams(-1,-2).apply{topMargin=dp(6);bottomMargin=dp(6)})
        actions = LinearLayout(this).apply{visibility=View.GONE}
        actions.addView(button("取消") { showFields() },LinearLayout.LayoutParams(dp(52),dp(46)).apply{rightMargin=dp(7)})
        actions.addView(button("模拟导航") { begin(true) }, LinearLayout.LayoutParams(0, dp(46), 1f).apply{rightMargin=dp(7)})
        actions.addView(button("开始导航") { begin(false) }, LinearLayout.LayoutParams(0, dp(46), 1f))
        foot.addView(actions,LinearLayout.LayoutParams(-1,-2).apply{topMargin=dp(2);bottomMargin=dp(4)})
        val savedPlaces=LinearLayout(this)
        savedPlaces.addView(button("常用地点") { showFavorites() },LinearLayout.LayoutParams(0,dp(36),1f))
        savedPlaces.addView(button("历史记录") { showHistory() },LinearLayout.LayoutParams(0,dp(36),1f).apply{leftMargin=dp(4);rightMargin=dp(4)})
        favorite = button("收藏目的地") {
            end?.let { p -> FavoritePlacesUi.save(this,p.name,"",p.lat,p.lng) { refreshFields() } }
        }
        savedPlaces.addView(favorite,LinearLayout.LayoutParams(0,dp(36),1f))
        foot.addView(savedPlaces,LinearLayout.LayoutParams(-1,-2).apply{topMargin=dp(6)})
        root.addView(foot)
        updateSpeedButtons()
        setContentView(root)
        val saved = state?.getString("motor_request") ?: intent.getStringExtra(EXTRA_REQUEST_URL)
        if (!saved.isNullOrBlank()) runCatching { WebMotorRoutes.request(saved) }.onSuccess {
            start = it.start; end = it.end; vias=it.vias; currentStart = state?.getBoolean("current_start", false) ?: false
        }
        if (state != null) {
            fun restore(prefix: String): MotorPoint? = if (state.containsKey(prefix + "_lat"))
                MotorPoint(state.getDouble(prefix + "_lat"), state.getDouble(prefix + "_lng"), state.getString(prefix + "_name").orEmpty(),state.getString(prefix + "_id").orEmpty()) else null
            start = restore("start") ?: start; end = restore("end") ?: end
            currentStart = state.getBoolean("current_start", true)
            if(state.containsKey("via_count"))vias=(0 until state.getInt("via_count").coerceIn(0,16)).mapNotNull{restore("via_$it")}
        }
        refreshFields()
        setupLocation()
        if (!Keys.hasAmap(this)) status.text = "请先在首页「地图 Key」配置高德 Android Key，以使用地点搜索和地图"
    }

    private fun label(value: String, size: Float = 14f, color: Int = S.TEXT) = TextView(this).apply {
        text = value; textSize = size; setTextColor(color);typeface=S.MEDIUM;includeFontPadding=false
    }
    private fun button(value: String, click: () -> Unit) = label(value, 14f, S.CYAN).apply {
        gravity = Gravity.CENTER; isSingleLine = true
        TextViewCompat.setAutoSizeTextTypeUniformWithConfiguration(this, 10, 14, 1, android.util.TypedValue.COMPLEX_UNIT_SP)
        setPadding(dp(6), dp(4), dp(6), dp(4));background=S.round(this@MotorWebPlannerActivity,S.CARD2,12f);setOnClickListener { click() }
    }
    private fun card()=LinearLayout(this).apply{
        orientation=LinearLayout.VERTICAL;background=S.round(this@MotorWebPlannerActivity,S.CARD,20f,S.STROKE)
        setPadding(dp(12),dp(10),dp(12),dp(10))
    }
    private fun updateSpeedButtons(){
        speedButtons.forEach{(kmh,item)->
            val active=kmh==plannedSpeed
            item.setTextColor(if(active)S.PINK else S.CYAN)
            item.background=S.round(this,S.CARD2,12f,if(active)S.PINK else S.STROKE)
        }
    }
    private fun updateLayout(){
        val choosing=routes.isNotEmpty()&&fields.visibility!=View.VISIBLE
        title.text=if(choosing)"前往 ${end?.name?.ifBlank{"目的地"} ?: "目的地"}" else "摩托车路线规划"
        backButton.visibility=if(choosing)View.GONE else View.VISIBLE
        accountButton.visibility=if(choosing)View.GONE else View.VISIBLE
        compact.visibility=if(choosing)View.VISIBLE else View.GONE
        summary.text="${routes.size} 条路线 · 请选择"
        listOf(summary,routeScroll,speedRow,actions).forEach{it.visibility=if(choosing)View.VISIBLE else View.GONE}
        plan.text=if(routes.isEmpty())"规划摩托车路线" else "重新规划路线"
        map.post{if(routes.isNotEmpty())fitRoute()}
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
        compact.text = "起点：${startText.text}${if(vias.isEmpty())"" else " · 途经 ${vias.size} 点"}  · 修改"
        viaSummary.text=RouteWaypointEditor.summary(vias)
        favorite.isEnabled = end != null
        favorite.alpha = if (end != null) 1f else .4f
        favorite.text = if (end?.let { FavoritePlaces.find(this,it.lat,it.lng) } != null) "已收藏 · 管理" else "收藏目的地"
        updateLayout()
    }
    private fun showFields() { fields.visibility = View.VISIBLE;updateLayout() }
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
        start = to; end = from; vias=vias.reversed();currentStart = false; invalidatePlan()
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

    private fun showFavorites() {
        FavoritePlacesUi.manage(this, { refreshFields() }) { p ->
            AlertDialog.Builder(this).setTitle(p.name).setItems(arrayOf("设为目的地", "设为起点")) { _, which ->
                val point = MotorPoint(p.lat,p.lng,p.name)
                if (which == 0) end = point else { start = point; currentStart = false }
                invalidatePlan()
            }.setNegativeButton("取消",null).show()
        }
    }
    private fun showHistory() {
        val items = MotorPlanHistory.load(this)
        if (items.isEmpty()) { Toast.makeText(this,"暂无历史记录，成功规划后会自动保存",Toast.LENGTH_SHORT).show(); return }
        AlertDialog.Builder(this).setTitle("摩托车规划历史（${items.size}/20）")
            .setItems(items.map { "${if(it.currentStart) "我的位置" else it.request.start.name} → ${it.request.end.name}"+
                if(it.request.vias.isEmpty())"" else "\n途经：${it.request.vias.joinToString(" → "){p->p.name}}" }.toTypedArray()) { _, index ->
                val item = items[index]
                currentStart = item.currentStart; start = if (currentStart) null else item.request.start; end = item.request.end;vias=item.request.vias
                invalidatePlan()
                status.text = "已填入历史起终点和途经点，点「规划摩托车路线」获取最新路线"
            }.setNeutralButton("清空记录") { _,_ ->
                AlertDialog.Builder(this).setTitle("清空摩托车规划历史？").setMessage("常用地点会保留。")
                    .setPositiveButton("清空") { _,_ ->
                        runCatching { MotorPlanHistory.clear(this) }.onFailure { Toast.makeText(this,it.message,Toast.LENGTH_LONG).show() }
                    }.setNegativeButton("取消",null).show()
            }.setNegativeButton("关闭",null).show()
    }
    private fun pointRow(parent: LinearLayout, point: MotorPoint, address: String = "", choose: (MotorPoint) -> Unit) {
        val row = LinearLayout(this).apply { gravity = Gravity.CENTER_VERTICAL }
        row.addView(label(point.name + if(address.isBlank()) "" else "\n$address",14f).apply {
            setPadding(dp(4),dp(10),dp(4),dp(10)); setOnClickListener { choose(point) }
        },LinearLayout.LayoutParams(0,-2,1f))
        row.addView(button("收藏") {
            FavoritePlacesUi.save(this,point.name,address,point.lat,point.lng) { refreshFields() }
        },LinearLayout.LayoutParams(dp(56),dp(48)))
        parent.addView(row)
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
        fun showSaved() {
            list.removeAllViews()
            val favorites = FavoritePlaces.load(this)
            if (favorites.isNotEmpty()) {
                list.addView(label("常用地点",12f,S.CYAN))
                favorites.forEach { p -> pointRow(list,MotorPoint(p.lat,p.lng,p.name),p.addr,::choose) }
            }
            val recent = MotorPlanHistory.load(this).map { if (origin && !it.currentStart) it.request.start else it.request.end }
                .distinctBy { "${it.lat},${it.lng}" }.take(8)
            if (recent.isNotEmpty()) {
                list.addView(label("最近规划地点",12f,S.CYAN))
                recent.forEach { p -> pointRow(list,p,choose=::choose) }
            }
            note.text = if (favorites.isEmpty() && recent.isEmpty()) "输入名称后点搜索，再选择具体地点；结果旁可直接收藏" else "可选择常用／历史地点，也可以输入名称搜索"
        }
        showSaved()
        input.addTextChangedListener(object : TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, st: Int, count: Int, after: Int) {}
            override fun onTextChanged(s: CharSequence?, st: Int, before: Int, count: Int) { searchGeneration++; list.removeAllViews(); if (s.isNullOrBlank()) showSaved() }
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
                                pointRow(list,MotorPoint(tip.point.latitude,tip.point.longitude,tip.name.orEmpty(),tip.poiID.orEmpty()),
                                    "${tip.district.orEmpty()} ${tip.address.orEmpty()}",::choose)
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
        val request = MotorRequest(from,to,vias.toList())
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
                    if (it.isEmpty()) { status.text = "未返回可用路线，请重新规划"; return@onSuccess }
                    routes = it; loadedRequest = request
                    runCatching { MotorPlanHistory.add(this,request,currentStart) }.onFailure {
                        Toast.makeText(this,it.message ?: "历史记录保存失败",Toast.LENGTH_LONG).show()
                    }
                    fields.visibility = View.GONE; compact.visibility = View.VISIBLE; refreshFields()
                    drawRoutes();selectRoute(0);updateLayout()
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
            req.vias.forEachIndexed{i,point->markers.add(map.map.addMarker(MarkerOptions().position(LatLng(point.lat,point.lng))
                .title("途经${i+1} · ${point.name}").icon(BitmapDescriptorFactory.defaultMarker(BitmapDescriptorFactory.HUE_ORANGE))))}
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
            val active=i==index
            val item=LinearLayout(this).apply{
                orientation=LinearLayout.VERTICAL;setPadding(dp(12),dp(10),dp(12),dp(10))
                background=S.round(this@MotorWebPlannerActivity,if(active)S.CARD2 else S.CARD,12f,if(active)S.CYAN else S.STROKE)
                setOnClickListener{selectRoute(i)}
            }
            item.addView(label("${if(active)"✓ " else ""}方案 ${i+1}　${S.durText(route.seconds)} · ${S.distText(route.distance)}",17f,if(active)S.CYAN else S.TEXT))
            val roads=route.steps.map{it.road.trim()}.filter{it.isNotBlank()}.distinct().take(6).joinToString(" → ")
            item.addView(label(roads.ifBlank{"按地图所示路线行驶"},12f,S.SUB).apply{maxLines=2;setPadding(0,dp(5),0,0)})
            item.addView(label("${route.lights} 个红绿灯",11f,S.SUB).apply{setPadding(0,dp(5),0,0)})
            alternatives.addView(item,LinearLayout.LayoutParams(-1,-2).apply{bottomMargin=dp(7)})
        }
        status.text="已选择方案 ${index+1}，可开始导航"
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
            val request = loadedRequest ?: if(from != null && end != null) MotorRequest(from,end!!,vias.toList()) else null
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
        runCatching {
            if(emulate)WebMotorGuide.simulationKmh=plannedSpeed
            WebMotorGuide.start(this,route,emulate)
        }
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
        if (from != null && end != null) state.putString("motor_request",MotorRequest(from,end!!,vias.toList()).pageUrl())
        fun store(prefix: String, point: MotorPoint?) { point?.let {
            state.putDouble(prefix + "_lat",it.lat); state.putDouble(prefix + "_lng",it.lng)
            state.putString(prefix + "_name",it.name); state.putString(prefix + "_id",it.id)
        } }
        store("start",start); store("end",end)
        state.putInt("via_count",vias.size);vias.forEachIndexed{i,point->store("via_$i",point)}
        state.putInt("planned_speed",plannedSpeed)
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
