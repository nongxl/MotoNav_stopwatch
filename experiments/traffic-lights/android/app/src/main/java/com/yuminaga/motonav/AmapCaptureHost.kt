package com.yuminaga.motonav

import android.app.Activity
import android.app.Application
import android.content.Context
import android.graphics.Bitmap
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.TextView
import com.amap.api.maps.AMap
import com.amap.api.maps.CameraUpdateFactory
import com.amap.api.maps.TextureMapView
import com.amap.api.maps.model.CameraPosition
import com.amap.api.maps.model.LatLng
import com.amap.api.maps.model.LatLngBounds
import java.lang.ref.WeakReference
import kotlin.math.abs

/** 单独的 SDK 渲染器，不操作规划/导航地图的相机、路线与图层。 */
class AmapCaptureHost private constructor(private val context: Context,private val decor: ViewGroup,private val background: Boolean=false) : AmapSdkRaster.Host {
    private val handler=Handler(Looper.getMainLooper())
    private var size=1440
    private val box=FrameLayout(context)
    private val view=TextureMapView(context)
    private val map: AMap
    private var loaded=false
    private var alive=true
    private var serial=0
    private var pending: ((AmapSdkRaster.Shot?,String?) -> Unit)?=null
    private var started=0L
    private var withoutBuildings: IntArray?=null
    private var satelliteCapture=false
    private var captureStyle=MapStylePolicy.VICE_CITY
    init {
        box.clipChildren=true
        box.importantForAccessibility=View.IMPORTANT_FOR_ACCESSIBILITY_NO_HIDE_DESCENDANTS
        view.onCreate(null)
        box.addView(view,FrameLayout.LayoutParams(size,size))
        // 保持真实渲染尺寸，正常界面只占背后一像素；诊断预览可展开同一个视图。
        decor.addView(box,0,FrameLayout.LayoutParams(1,1,Gravity.TOP or Gravity.END))
        map=view.map
        map.uiSettings.setAllGesturesEnabled(false)
        map.uiSettings.isZoomControlsEnabled=false
        map.uiSettings.isCompassEnabled=false
        map.uiSettings.isScaleControlsEnabled=false
        map.isMyLocationEnabled=false; map.isTrafficEnabled=false
        map.mapType=AMap.MAP_TYPE_NORMAL
        map.showBuildings(false); map.showIndoorMap(false); map.showMapText(captureStyle==MapStylePolicy.ORIGINAL)
        map.setOnMapLoadedListener { if (alive) { loaded=true; AmapSdkRaster.state="高德无字底图已就绪" } }
        val close=TextView(context).apply {
            text="×"; textSize=24f; gravity=Gravity.CENTER
            setTextColor(0xffffffff.toInt()); setBackgroundColor(0x99000000.toInt())
            setOnClickListener { setPreview(false) }
        }
        box.addView(close,FrameLayout.LayoutParams(dp(32),dp(32),Gravity.TOP or Gravity.END))
        setPreview(preview)
        view.onResume()
        AmapSdkRaster.state="高德地图初始化中"
        AmapSdkRaster.attach(this)
    }
    private fun dp(n: Int)=(n*context.resources.displayMetrics.density+0.5f).toInt()
    private fun setPreview(show: Boolean) {
        if(background) {
            box.layoutParams=FrameLayout.LayoutParams(256,256,Gravity.TOP or Gravity.END)
            view.pivotX=0f;view.pivotY=0f;view.scaleX=256f/size;view.scaleY=256f/size
            return
        }
        preview=show
        val width=if (show) dp(210) else 1
        box.layoutParams=FrameLayout.LayoutParams(width,width,Gravity.TOP or Gravity.END).apply { if (show) { topMargin=dp(80); rightMargin=dp(10) } }
        view.pivotX=0f; view.pivotY=0f; view.scaleX=width.toFloat()/size; view.scaleY=width.toFloat()/size
        if (show) box.bringToFront() else { decor.removeView(box); decor.addView(box,0) }
    }
    override fun capture(f: MapGeo.Frame,x0: Int,y0: Int,n: Int,satellite: Boolean,style: Int,done: (AmapSdkRaster.Shot?,String?) -> Unit) {
        if (!alive || !loaded) { done(null,"高德地图初始化中，稍后自动重试"); return }
        if (pending!=null) { done(null,"高德正在准备上一块底图"); return }
        pending=done; satelliteCapture=satellite;captureStyle=style; box.keepScreenOn=true
        map.mapType=if(satellite)AMap.MAP_TYPE_SATELLITE else AMap.MAP_TYPE_NORMAL
        val id=++serial
        started=SystemClock.elapsedRealtime()
        AmapSdkRaster.state="正在加载高德无字底图"
        val wanted=if(n==600)2880 else 1440
        val resized=size!=wanted
        if(resized) {
            size=wanted;view.layoutParams=FrameLayout.LayoutParams(size,size)
            setPreview(preview)
        }
        map.showBuildings(style==MapStylePolicy.ORIGINAL);withoutBuildings=null
        val margin=if(n==600)20.0 else 160.0
        val bounds=LatLngBounds(LatLng(f.lat(y0-n*(if(satelliteCapture)3.5 else 2.0)-margin),f.lng(x0-margin)),
            LatLng(f.lat(y0+margin),f.lng(x0+n*(if(satelliteCapture)3.5 else 2.0)+margin)))
        var cameraDone=false
        map.showMapText(captureStyle==MapStylePolicy.ORIGINAL)
        map.setOnCameraChangeListener(object: AMap.OnCameraChangeListener {
            override fun onCameraChange(p: CameraPosition?) {}
            override fun onCameraChangeFinish(p: CameraPosition?) {
                if (id==serial && !cameraDone) {
                    // 相机更新异步完成，必须沿用本次回调的位置，不能用更新前的相机覆盖缩放。
                    val tilt=0.05f
                    if(n==600 && p!=null && p.tilt<tilt-0.025f) {
                        map.moveCamera(CameraUpdateFactory.newCameraPosition(CameraPosition.Builder(p).tilt(tilt).build()))
                        return
                    }
                    cameraDone=true;handler.postDelayed({ request(id,f,x0,y0,n) },700)
                }
            }
        })
        handler.postDelayed({
            if(!alive || id!=serial)return@postDelayed
            map.moveCamera(CameraUpdateFactory.newLatLngBounds(bounds,size,size,0))
            handler.postDelayed({ if (id==serial && !cameraDone) { cameraDone=true; request(id,f,x0,y0,n) } },1800)
        },if(resized)300 else 0)
    }
    private fun request(id: Int,f: MapGeo.Frame,x0: Int,y0: Int,n: Int) {
        if (!alive || id!=serial || pending==null) return
        try {
            map.showMapText(captureStyle==MapStylePolicy.ORIGINAL)
            map.getMapScreenShot(object: AMap.OnMapScreenShotListener {
                override fun onMapScreenShot(bitmap: Bitmap?) {}
                override fun onMapScreenShot(bitmap: Bitmap?,status: Int) {
                    handler.post {
                        try {
                            if (!alive || id!=serial || pending==null) return@post
                            if (status!=1 || bitmap==null) {
                                if (SystemClock.elapsedRealtime()-started<38000) {
                                    AmapSdkRaster.state="高德底图尚未加载完整，保留当前地图等待重试"
                                    handler.postDelayed({ request(id,f,x0,y0,n) },1200)
                                } else finish(null,"高德底图加载未完成，保留当前地图")
                                return@post
                            }
                            require(bitmap.width==size && bitmap.height==size) { "高德截图尺寸异常" }
                            val camera=map.cameraPosition
                            if(n==600)require(camera.zoom>=17f){"楼块缩放级别不足，保留普通底图"}
                            require(abs(camera.tilt)<0.1 && minOf(abs(camera.bearing),abs(camera.bearing-360))<0.1) { "高德截图方向不正确" }
                            val north=f.lat(y0.toDouble()); val south=f.lat(y0-n*(if(satelliteCapture)3.5 else 2.0))
                            val west=f.lng(x0.toDouble()); val east=f.lng(x0+n*(if(satelliteCapture)3.5 else 2.0))
                            val projection=map.projection
                            val tl=projection.toScreenLocation(LatLng(north,west)); val tr=projection.toScreenLocation(LatLng(north,east))
                            val bl=projection.toScreenLocation(LatLng(south,west)); val br=projection.toScreenLocation(LatLng(south,east))
                            require(abs(tl.y-tr.y)<=2 && abs(tl.x-bl.x)<=2 && abs(bl.y-br.y)<=2 && abs(tr.x-br.x)<=2) { "高德截图坐标未对齐" }
                            require(tr.x-tl.x>=n-2 && bl.y-tl.y>=n-2) { "高德截图精度不足，保留当前地图" }
                            val pixels=IntArray(size*size)
                            bitmap.getPixels(pixels,0,size,0,0,size,size)
                            val reference=if(n==600)withoutBuildings else null
                            if(n==600) {
                                if(reference==null) {
                                    withoutBuildings=pixels
                                    map.showBuildings(true)
                                    handler.postDelayed({ request(id,f,x0,y0,n) },700)
                                    return@post
                                }
                                withoutBuildings=null
                            }
                            finish(AmapSdkRaster.Shot(pixels,size,size,tl.x.toDouble(),tl.y.toDouble(),tr.x.toDouble(),bl.y.toDouble(),reference),null)
                        } catch (_: OutOfMemoryError) { finish(null,"楼块取图内存不足，先显示普通底图") }
                        catch (e: Exception) { finish(null,e.message ?: "高德取图失败") }
                        finally { if (bitmap!=null && !bitmap.isRecycled) bitmap.recycle() }
                    }
                }
            })
        } catch (e: Exception) { finish(null,e.message ?: "高德截图请求失败") }
    }
    private fun finish(shot: AmapSdkRaster.Shot?,reason: String?) {
        val done=pending ?: return
        pending=null; serial++; box.keepScreenOn=false
        withoutBuildings=null
        map.setOnCameraChangeListener(null)
        AmapSdkRaster.state=reason ?: "高德完整底图已取到，正在准备圆屏数据"
        done(shot,reason)
    }
    override fun cancelCapture() { finish(null,"高德取图已暂停，保留当前地图") }
    private fun destroy() {
        if (!alive) return
        alive=false; AmapSdkRaster.detach(this)
        handler.removeCallbacksAndMessages(null)
        view.onPause(); view.onDestroy(); decor.removeView(box)
    }
    companion object {
        private var current: AmapCaptureHost?=null
        private var activityRef=WeakReference<Activity>(null)
        private var preview=false
        private var serviceContext: Context?=null
        private var backgroundDisplay: MapCaptureDisplay?=null
        fun serviceStarted(context: Context) { serviceContext=context }
        fun serviceStopped(context: Context) {
            if(serviceContext!==context)return
            serviceContext=null
            if(backgroundDisplay!=null)destroyCurrent()
        }
        private fun destroyCurrent() {
            current?.destroy();current=null
            backgroundDisplay?.close();backgroundDisplay=null
        }
        /** 只在确实需要新地图且页面不在前台时创建，不空转占用后台资源。 */
        fun ensureBackground(): AmapSdkRaster.Host? {
            if (!MapStylePolicy.usesAmapCapture(MapFeeder.theme)) { destroyCurrent(); return null }
            if(current!=null && backgroundDisplay?.showing==false)destroyCurrent()
            current?.let{return it}
            if(activityRef.get()!=null){refresh();return current}
            val context=serviceContext ?: return null
            if(!MapProviderPolicy.usesAmap(MapFeeder.source,MapFeeder.theme) || !MapFeeder.enabled || !MapStylePolicy.usesAmapCapture(MapFeeder.theme) || !Keys.hasAmap(context))return null
            try {
                val surface=MapCaptureDisplay(context);backgroundDisplay=surface
                current=AmapCaptureHost(surface.viewContext,surface.decor,true)
            }catch(_:OutOfMemoryError){destroyCurrent();AmapSdkRaster.state="后台地图内存不足，保留已有地图"}
            catch(e:Exception){destroyCurrent();AmapSdkRaster.state="后台地图初始化失败：${e.message ?: "未知错误"}"}
            return current
        }
        fun togglePreview() { current?.let { it.setPreview(!preview) } }
        fun refresh() {
            val activity=activityRef.get()
            if(activity==null) {
                val context=serviceContext ?: return
                if(!MapProviderPolicy.usesAmap(MapFeeder.source,MapFeeder.theme) || !MapFeeder.enabled || !MapStylePolicy.usesAmapCapture(MapFeeder.theme) || !Keys.hasAmap(context))destroyCurrent()
                return
            }
            val needed=MapProviderPolicy.usesAmap(MapFeeder.source,MapFeeder.theme) && MapFeeder.enabled && MapStylePolicy.usesAmapCapture(MapFeeder.theme) && Keys.hasAmap(activity)
            if (needed && current==null) try { current=AmapCaptureHost(activity,activity.window.decorView as ViewGroup) }
            catch (e: Exception) { AmapSdkRaster.state="高德渲染器初始化失败：${e.message ?: "未知错误"}" }
            if (!needed) destroyCurrent()
        }
        fun install(app: Application) {
            AmapSdkRaster.setBackgroundProvider { ensureBackground() }
            app.registerActivityLifecycleCallbacks(object: Application.ActivityLifecycleCallbacks {
                override fun onActivityResumed(activity: Activity) {
                    if (activity !is MainActivity && activity !is RoutePlanActivity && activity !is NavActivity) return
                    if(backgroundDisplay!=null)destroyCurrent()
                    activityRef=WeakReference(activity); MapFeeder.load(activity); refresh()
                }
                override fun onActivityPaused(activity: Activity) {
                    if (activityRef.get()!==activity) return
                    destroyCurrent();activityRef.clear()
                }
                override fun onActivityCreated(a: Activity,b: Bundle?) {}
                override fun onActivityStarted(a: Activity) {}
                override fun onActivityStopped(a: Activity) {}
                override fun onActivitySaveInstanceState(a: Activity,b: Bundle) {}
                override fun onActivityDestroyed(a: Activity) { if (activityRef.get()===a) onActivityPaused(a) }
            })
        }
    }
}
