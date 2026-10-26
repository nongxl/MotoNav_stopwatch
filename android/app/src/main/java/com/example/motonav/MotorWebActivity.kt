package com.example.motonav

import android.Manifest
import android.annotation.SuppressLint
import android.content.Intent
import android.content.pm.PackageManager
import android.graphics.Color
import android.net.Uri
import android.net.http.SslError
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.webkit.CookieManager
import android.webkit.GeolocationPermissions
import android.webkit.SslErrorHandler
import android.webkit.WebChromeClient
import android.webkit.WebResourceError
import android.webkit.WebResourceRequest
import android.webkit.WebSettings
import android.webkit.WebView
import android.webkit.WebViewClient
import android.widget.EditText
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import androidx.activity.OnBackPressedCallback
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import org.json.JSONTokener
import java.util.concurrent.Executors

/** Only embeds the public HTTPS page; there is no JavaScript-to-native privileged bridge. */
class MotorWebActivity : AppCompatActivity() {
    private lateinit var web: WebView
    private lateinit var status: TextView
    private lateinit var readButton: TextView
    private val handler = Handler(Looper.getMainLooper())
    private val worker = Executors.newSingleThreadExecutor()
    private var sequence = 0
    private var pending: Pair<WebMotorRoute, Boolean>? = null
    private var choices: List<WebMotorRoute> = emptyList()
    private var currentUrl = WebMotorRoutes.PAGE

    @SuppressLint("SetJavaScriptEnabled")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        supportActionBar?.hide()
        val root = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setBackgroundColor(Color.rgb(14,10,20)) }
        WindowCompat.setDecorFitsSystemWindows(window, false)
        ViewCompat.setOnApplyWindowInsetsListener(root) { view, insets ->
            val bars = insets.getInsets(WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.displayCutout())
            val ime = insets.getInsets(WindowInsetsCompat.Type.ime())
            view.setPadding(bars.left,bars.top,bars.right,maxOf(bars.bottom,ime.bottom)); insets
        }
        val toolbar = LinearLayout(this)
        fun tool(text: String, click: () -> Unit) { toolbar.addView(button(text, click), LinearLayout.LayoutParams(0, 52.dp, 1f)) }
        tool("返回首页") { finish() }
        tool("粘贴链接") { linkDialog() }
        tool("刷新") { web.reload() }
        root.addView(toolbar)
        status = TextView(this).apply {
            text = "网页摩托车实验：选摩托车和起终点，再点下方读取。也可粘贴完整路线链接。"
            setTextColor(Color.LTGRAY); textSize = 12f; setPadding(12.dp, 6.dp, 12.dp, 6.dp)
        }
        root.addView(status)
        CookieManager.getInstance().setAcceptCookie(true)
        web = WebView(this)
        web.settings.apply {
            javaScriptEnabled = true; domStorageEnabled = true
            userAgentString = WebMotorRoutes.UA
            useWideViewPort = true; loadWithOverviewMode = true
            builtInZoomControls = true; displayZoomControls = false
            allowFileAccess = false; allowContentAccess = false
            mixedContentMode = WebSettings.MIXED_CONTENT_NEVER_ALLOW
            javaScriptCanOpenWindowsAutomatically = false; setSupportMultipleWindows(false)
        }
        web.webViewClient = object : WebViewClient() {
            override fun shouldOverrideUrlLoading(view: WebView, request: WebResourceRequest): Boolean {
                val u = request.url
                val allowed = u.scheme == "https" && (u.host == "amap.com" || u.host.orEmpty().endsWith(".amap.com"))
                if (!allowed && request.isForMainFrame) status.text = "网页尝试打开外部页面或高德 App。请返回摩托车规划页；本实验从下方读取路线。"
                return !allowed
            }
            override fun onReceivedSslError(view: WebView, ssl: SslErrorHandler, error: SslError) {
                ssl.cancel(); status.text = "网页证书校验失败，已停止加载。请检查时间和网络后刷新。"
            }
            override fun doUpdateVisitedHistory(view: WebView, url: String, isReload: Boolean) {
                currentUrl = url
                getSharedPreferences("webmotor", 0).edit().putString("last_page", url).apply()
            }
            override fun onPageFinished(view: WebView, url: String) {
                currentUrl = url
                if (choices.isEmpty()) status.text = "请选择摩托车和起终点，然后点「读取摩托路线」。可双指缩放网页。"
            }
            override fun onReceivedError(view: WebView, request: WebResourceRequest, error: WebResourceError) {
                if (request.isForMainFrame) status.text = "网页加载失败：${error.description}。请检查网络后刷新，或粘贴路线链接读取。"
            }
        }
        web.webChromeClient = object : WebChromeClient() {
            override fun onGeolocationPermissionsShowPrompt(origin: String, callback: GeolocationPermissions.Callback) {
                val u = Uri.parse(origin)
                val valid = u.scheme == "https" && (u.host == "amap.com" || u.host.orEmpty().endsWith(".amap.com"))
                if (!valid || checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
                    callback.invoke(origin, false, false)
                    status.text = "网页定位需要位置权限。可回首页授权，或在网页手动设置起点。"
                    return
                }
                AlertDialog.Builder(this@MotorWebActivity).setMessage("允许高德网页使用当前位置作为起点？")
                    .setPositiveButton("允许") { _, _ -> callback.invoke(origin, true, false) }
                    .setNegativeButton("取消") { _, _ -> callback.invoke(origin, false, false) }
                    .setOnCancelListener { callback.invoke(origin, false, false) }.show()
            }
        }
        root.addView(web, LinearLayout.LayoutParams(-1, 0, 1f))
        readButton = button("读取摩托路线") { readPage() }
        root.addView(readButton, LinearLayout.LayoutParams(-1, 54.dp))
        root.addView(button("继续当前网页导航") {
            if (WebMotorGuide.route != null) startActivity(Intent(this, MotorWebNavActivity::class.java))
            else Toast.makeText(this, "还没有开始网页导航", Toast.LENGTH_SHORT).show()
        }, LinearLayout.LayoutParams(-1, 44.dp))
        setContentView(root)
        currentUrl = intent.getStringExtra("initial_motor_url") ?: getSharedPreferences("webmotor",0).getString("last_page", WebMotorRoutes.PAGE)
            ?.takeIf { WebMotorRoutes.isPage(it) } ?: WebMotorRoutes.PAGE
        if (savedInstanceState == null || web.restoreState(savedInstanceState) == null) web.loadUrl(currentUrl)
        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() { if (web.canGoBack()) web.goBack() else finish() }
        })
    }

    private val Int.dp: Int get() = (this * resources.displayMetrics.density).toInt()
    private fun button(text: String, click: () -> Unit) = TextView(this).apply {
        this.text = text; textSize = 15f; gravity = android.view.Gravity.CENTER
        setTextColor(Color.rgb(67,225,240)); setBackgroundColor(Color.rgb(26,20,36))
        setOnClickListener { click() }
    }

    private fun linkDialog() {
        val input = EditText(this).apply { hint = "粘贴高德网页版摩托车完整链接"; setText(currentUrl); selectAll() }
        AlertDialog.Builder(this).setTitle("高德网页摩托车路线").setView(input)
            .setPositiveButton("打开并读取") { _, _ ->
                try {
                    val raw = input.text.toString().trim().replace("\\&", "&")
                    val request = WebMotorRoutes.request(raw)
                    currentUrl = raw; web.loadUrl(raw); load(request)
                } catch (e: Exception) { status.text = e.message }
            }.setNegativeButton("取消", null).show()
    }

    private fun readPage() {
        // Read location only. No injected hooks, cookies export or JavaScript interface.
        web.evaluateJavascript("window.location.href") { value ->
            try {
                val url = JSONTokener(value).nextValue() as? String ?: error("无法读取网页地址")
                currentUrl = url
                load(WebMotorRoutes.request(url))
            } catch (e: Exception) { status.text = e.message }
        }
    }

    private fun load(request: MotorRequest) {
        val id = ++sequence
        status.text = "正在读取高德网页摩托车路线…"
        readButton.isEnabled = false
        worker.execute {
            val result = runCatching { WebMotorRoutes.load(request) }
            handler.post {
                if (isFinishing || isDestroyed || id != sequence) return@post
                readButton.isEnabled = true
                if (runCatching { WebMotorRoutes.request(currentUrl) }.getOrNull() != request) {
                    status.text = "网页起终点已变化，请重新读取。"; return@post
                }
                result.onSuccess { routes -> choices = routes; choose(routes) }
                    .onFailure { status.text = it.message ?: "网页摩托车算路失败" }
            }
        }
    }

    private fun choose(routes: List<WebMotorRoute>) {
        status.text = "已读取 ${routes.size} 条摩托车路线。请在实验版中选择方案；网页高亮选择不会自动同步。"
        var selected = 0
        val labels = routes.mapIndexed { i, r -> "方案 ${i+1} · ${"%.1f".format(r.distance/1000.0)} km · ${r.seconds/60} 分钟 · ${r.lights} 个红绿灯" }.toTypedArray()
        AlertDialog.Builder(this).setTitle("摩托车路线 · ${routes[0].request.end.name}")
            .setSingleChoiceItems(labels,0) { _, which -> selected = which }
            .setPositiveButton("真实导航") { _, _ -> prepare(routes[selected], false) }
            .setNeutralButton("模拟导航") { _, _ -> prepare(routes[selected], true) }
            .setNegativeButton("取消", null).show()
    }

    private fun prepare(route: WebMotorRoute, simulation: Boolean) {
        pending = route to simulation
        val permissions = mutableListOf(Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.ACCESS_COARSE_LOCATION)
        if (Build.VERSION.SDK_INT >= 31) permissions.addAll(listOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT))
        val missing = permissions.filter { checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }
        if (missing.isNotEmpty()) { requestPermissions(missing.toTypedArray(), 60); return }
        if (!simulation && !Keys.hasAmap(this)) { status.text = "真实导航需要高德 Android Key 用于 GCJ-02 定位，请回首页配置。网页规划和模拟可单独测试。"; return }
        NavService.start(this)
        waitForService(route, simulation, 0)
    }

    private fun waitForService(route: WebMotorRoute, simulation: Boolean, attempts: Int) {
        if (isFinishing || isDestroyed) return
        if (!NavService.running) {
            if (attempts >= 20) { status.text = "后台服务没有启动，请回首页检查位置和附近设备权限。"; return }
            handler.postDelayed({ waitForService(route, simulation, attempts + 1) }, 200); return
        }
        runCatching { WebMotorGuide.start(this, route, simulation) }
            .onSuccess { startActivity(Intent(this, MotorWebNavActivity::class.java)) }
            .onFailure { status.text = "启动失败：${it.message}"; WebMotorGuide.stop() }
    }

    override fun onRequestPermissionsResult(code: Int, permissions: Array<out String>, results: IntArray) {
        super.onRequestPermissionsResult(code, permissions, results)
        if (code == 60) {
            if (results.isNotEmpty() && results.all { it == PackageManager.PERMISSION_GRANTED }) pending?.let { prepare(it.first, it.second) }
            else status.text = "权限未给全，可回首页授权后再试。"
        }
    }
    override fun onSaveInstanceState(outState: Bundle) { web.saveState(outState); super.onSaveInstanceState(outState) }
    override fun onResume() { super.onResume(); if (::web.isInitialized) web.onResume() }
    override fun onPause() { if (::web.isInitialized) web.onPause(); CookieManager.getInstance().flush(); super.onPause() }
    override fun onDestroy() {
        sequence++; handler.removeCallbacksAndMessages(null); worker.shutdownNow()
        web.stopLoading(); web.destroy(); super.onDestroy()
    }
}
