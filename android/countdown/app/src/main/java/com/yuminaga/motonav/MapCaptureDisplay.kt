package com.yuminaga.motonav

import android.app.Presentation
import android.content.Context
import android.graphics.PixelFormat
import android.hardware.display.DisplayManager
import android.hardware.display.VirtualDisplay
import android.media.ImageReader
import android.os.Handler
import android.os.HandlerThread
import android.view.ViewGroup
import android.view.WindowManager
import android.widget.FrameLayout

/** 后台服务的私有绘制画面，只显示本 App 地图，不采集手机屏幕或其他应用。 */
class MapCaptureDisplay(context: Context) : AutoCloseable {
    private val thread=HandlerThread("map-display-surface").apply{start()}
    private val reader=ImageReader.newInstance(256,256,PixelFormat.RGBA_8888,2)
    private var display: VirtualDisplay?=null
    private var presentation: Presentation?=null
    val viewContext: Context get()=presentation!!.context
    val decor: ViewGroup get()=presentation!!.window!!.decorView as ViewGroup
    val showing: Boolean get()=presentation?.isShowing==true
    init {
        try {
            reader.setOnImageAvailableListener({ source ->
                try{source.acquireLatestImage()?.close()}catch(_:IllegalStateException){}
            },Handler(thread.looper))
            val manager=context.getSystemService(Context.DISPLAY_SERVICE) as DisplayManager
            display=manager.createVirtualDisplay("MotoNav-map",256,256,160,reader.surface,
                DisplayManager.VIRTUAL_DISPLAY_FLAG_OWN_CONTENT_ONLY or DisplayManager.VIRTUAL_DISPLAY_FLAG_PRESENTATION)
                ?: error("后台地图画面创建失败")
            presentation=Presentation(context,display!!.display).apply {
                window?.addFlags(WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE or
                    WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE or WindowManager.LayoutParams.FLAG_HARDWARE_ACCELERATED)
                setContentView(FrameLayout(this.context))
                show()
                window?.setLayout(256,256)
            }
        }catch(e:Throwable){close();throw e}
    }
    override fun close() {
        try{presentation?.dismiss()}catch(_:Exception){}
        presentation=null
        try{display?.release()}catch(_:Exception){}
        display=null
        reader.setOnImageAvailableListener(null,null)
        reader.close();thread.quitSafely()
    }
}
