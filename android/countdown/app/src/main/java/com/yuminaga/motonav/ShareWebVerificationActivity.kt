package com.yuminaga.motonav

import android.app.Activity
import android.content.Intent
import android.graphics.Bitmap
import android.net.Uri
import android.os.Bundle
import android.view.Gravity
import android.webkit.*
import android.widget.LinearLayout
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import org.json.JSONObject
import org.json.JSONTokener
import java.io.File
import java.util.UUID

/** Normal Amap browser verification. The user completes the real challenge; no challenge automation. */
class ShareWebVerificationActivity:AppCompatActivity(){
    private lateinit var web:WebView
    private lateinit var status:TextView
    private var captured=false
    private var inspecting=false
    private var pageGeneration=0
    override fun onCreate(state:Bundle?){
        super.onCreate(state);supportActionBar?.hide()
        val token=intent.getStringExtra(EXTRA_ROUTE).orEmpty()
        val route=runCatching{SharedRouteStore.load(this,token)}.getOrNull()
        val url=route?.verificationUrl.orEmpty()
        if(!allowed(url)){finish();return}
        val s=NavStyle
        fun dp(v:Int)=s.dpi(this,v)
        WindowCompat.setDecorFitsSystemWindows(window,false)
        val root=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL;setBackgroundColor(s.BG)}
        ViewCompat.setOnApplyWindowInsetsListener(root){view,ins->val bar=ins.getInsets(WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.displayCutout());view.setPadding(bar.left,bar.top,bar.right,bar.bottom);ins}
        fun button(label:String,click:()->Unit)=TextView(this).apply{text=label;textSize=14f;setTextColor(s.CYAN);gravity=Gravity.CENTER;setOnClickListener{click()}}
        val row=LinearLayout(this)
        row.addView(button("返回"){setResult(Activity.RESULT_CANCELED);finish()},LinearLayout.LayoutParams(dp(66),dp(48)))
        row.addView(TextView(this).apply{text="高德网页验证";textSize=17f;setTextColor(s.TEXT);gravity=Gravity.CENTER_VERTICAL},LinearLayout.LayoutParams(0,dp(48),1f))
        row.addView(button("验证后重试"){CookieManager.getInstance().flush();web.reload()},LinearLayout.LayoutParams(dp(114),dp(48)))
        root.addView(row)
        status=TextView(this).apply{text="请在下方完成高德网页验证。取得路线数据后会自动返回。";textSize=12f;setTextColor(s.SUB);setPadding(dp(12),dp(8),dp(12),dp(8))};root.addView(status)
        web=WebView(this)
        CookieManager.getInstance().setAcceptCookie(true)
        CookieManager.getInstance().setAcceptThirdPartyCookies(web,true)
        web.settings.apply{javaScriptEnabled=true;domStorageEnabled=true;userAgentString=AmapShare.UA;allowFileAccess=false;allowContentAccess=false;mixedContentMode=WebSettings.MIXED_CONTENT_NEVER_ALLOW}
        web.webViewClient=object:WebViewClient(){
            override fun shouldOverrideUrlLoading(view:WebView,request:WebResourceRequest):Boolean{
                if(request.isForMainFrame && !allowed(request.url.toString())){status.text="验证页面尝试离开高德网站，已停止跳转";return true}
                return false
            }
            override fun onPageStarted(view:WebView,url:String?,favicon:Bitmap?){pageGeneration++;inspecting=false;status.text="正在加载高德网页…"}
            override fun onPageFinished(view:WebView,url:String?){
                CookieManager.getInstance().flush()
                if(url!=null && allowed(url)){status.text="如页面要求验证，请手动完成；完成后可点「验证后重试」。";inspectResponse()}
            }
            override fun onReceivedError(view:WebView,request:WebResourceRequest,error:WebResourceError){if(request.isForMainFrame)status.text="验证页加载失败：${error.description}。可点右上角重试。"}
        }
        root.addView(web,LinearLayout.LayoutParams(-1,0,1f));setContentView(root)
        if(state==null || web.restoreState(state)==null)web.loadUrl(url.replaceFirst("http://","https://"))
    }
    private fun inspectResponse(){
        if(captured || inspecting)return
        inspecting=true;val generation=pageGeneration
        web.evaluateJavascript("(function(){return (document.body ? document.body.innerText : '').slice(0,8000001);})()"){encoded->
            inspecting=false
            if(isFinishing || isDestroyed || captured || generation!=pageGeneration)return@evaluateJavascript
            val body=runCatching{JSONTokener(encoded).nextValue() as? String}.getOrNull()?:return@evaluateJavascript
            if(body.length>8_000_000){status.text="返回的路线内容过大，请分段分享";return@evaluateJavascript}
            val json=runCatching{JSONObject(body)}.getOrNull()?:return@evaluateJavascript
            if(json.optInt("code")==1 && (json.optJSONArray("path_list")?.length() ?: 0)>0){
                val token=UUID.randomUUID().toString().replace("-","")
                runCatching{responseFile(this,token).writeText(body)}.onSuccess{
                    captured=true;CookieManager.getInstance().flush()
                    setResult(Activity.RESULT_OK,Intent().putExtra(EXTRA_RESPONSE,token));finish()
                }.onFailure{status.text="路线数据保存失败：${it.message}"}
            }else status.text="高德尚未返回完整路线数据，验证后可重试。"
        }
    }
    override fun onResume(){super.onResume();if(::web.isInitialized)web.onResume()}
    override fun onPause(){if(::web.isInitialized){CookieManager.getInstance().flush();web.onPause()};super.onPause()}
    override fun onSaveInstanceState(state:Bundle){if(::web.isInitialized)web.saveState(state);super.onSaveInstanceState(state)}
    override fun onDestroy(){if(::web.isInitialized){web.stopLoading();web.destroy()};super.onDestroy()}
    companion object{
        const val EXTRA_ROUTE="verification_route_token"
        const val EXTRA_RESPONSE="verification_response_token"
        fun allowed(url:String):Boolean=runCatching{val u=Uri.parse(url);val host=u.host.orEmpty().lowercase();u.scheme in listOf("http","https") && (host=="amap.com" || host.endsWith(".amap.com"))}.getOrDefault(false)
        fun responseFile(c:android.content.Context,token:String):File{
            require(SharedRouteStore.validToken(token))
            return File(File(c.cacheDir,"verified-share-responses").apply{mkdirs()},"$token.json")
        }
    }
}
