package com.example.motonav

import android.graphics.Bitmap
import android.os.Handler
import android.os.Looper
import java.io.IOException
import java.util.concurrent.CancellationException
import java.util.concurrent.CountDownLatch
import java.util.concurrent.TimeUnit
import java.util.concurrent.atomic.AtomicBoolean
import java.util.concurrent.atomic.AtomicReference
import java.util.concurrent.locks.ReentrantLock
import kotlin.math.roundToInt

/** 普通底图保持原采样，楼块另用近景取图拼接，不改变底图道路精度。 */
object AmapSdkRaster {
    interface Host {
        fun capture(f: MapGeo.Frame,x0: Int,y0: Int,size: Int,satellite: Boolean,style: Int,done: (Shot?,String?) -> Unit)
        fun cancelCapture()
    }
    class Shot @JvmOverloads constructor(val pixels: IntArray,val width: Int,val height: Int,
               val left: Double,val top: Double,val right: Double,val bottom: Double,val withoutBuildings: IntArray?=null)
    private val main=Handler(Looper.getMainLooper())
    private val lease=ReentrantLock(true)
    private var host: Host?=null
    private var backgroundProvider: (() -> Host?)?=null
    private val buildingTiles=object: LinkedHashMap<String,ByteArray>(8,0.75f,true) {
        override fun removeEldestEntry(eldest: MutableMap.MutableEntry<String,ByteArray>?)=size>8
    }
    @Volatile var state="等待地图取图器就绪"
        internal set
    fun attach(value: Host) { check(Looper.myLooper()==Looper.getMainLooper()); host=value }
    fun setBackgroundProvider(provider: (() -> Host?)?) {
        check(Looper.myLooper()==Looper.getMainLooper());backgroundProvider=provider
    }
    fun detach(value: Host) {
        check(Looper.myLooper()==Looper.getMainLooper())
        if(host===value){host=null;value.cancelCapture();state="地图取图器切换中，已离线的地图仍可继续使用"}
    }
    fun build(f: MapGeo.Frame,cx: Double,cy: Double,half: Double,makePreview: Boolean=true,
              shouldCancel: () -> Boolean = { false },includeBuildings: Boolean=false,onBaseReady: ((ByteArray)->Unit)?=null,satellite: Boolean=false,style: Int=MapStylePolicy.VICE_CITY): RasterResult {
        check(Looper.myLooper()!=Looper.getMainLooper())
        fun cancelled(){if(Thread.currentThread().isInterrupted || shouldCancel())throw CancellationException()}
        var locked=false
        val active=AtomicReference<AtomicBoolean?>()
        val owner=AtomicReference<Host?>()
        fun take(x: Int,y: Int,n: Int,timeout: Int): IntArray {
            cancelled()
            val ended=AtomicBoolean(false);active.set(ended)
            val latch=CountDownLatch(1);val shot=AtomicReference<Shot?>()
            val error=AtomicReference("高德取图超时，保留当前地图等待重试")
            main.post {
                if(!ended.get()) {
                    val h=host ?: backgroundProvider?.invoke();owner.set(h)
                    if(h==null){error.set("地图取图器未就绪，请确认后台服务已开启");ended.set(true);latch.countDown()}
                    else try {
                        h.capture(f,x,y,n,satellite,style){image,reason->
                            if(ended.compareAndSet(false,true)){shot.set(image);if(reason!=null)error.set(reason);latch.countDown()}
                        }
                    }catch(e: Exception){error.set(e.message ?: "高德取图失败");ended.set(true);latch.countDown()}
                }
            }
            var waited=0
            try {
                while(!latch.await(150,TimeUnit.MILLISECONDS)){cancelled();waited+=150;if(waited>=timeout)throw IOException(error.get())}
                cancelled()
                val s=shot.get() ?: throw IOException(error.get())
                s.withoutBuildings?.let{BuildingDifference.mark(it,s.pixels)}
                return AmapSampleGrid.resample(s.pixels,s.width,s.height,s.left,s.top,s.right,s.bottom,
                    f.lat(y.toDouble()),f.lat(y-n*(if(satellite)3.5 else 2.0)),n)
            }finally{ended.set(true)}
        }
        try {
            while(!locked){cancelled();locked=lease.tryLock(150,TimeUnit.MILLISECONDS)}
            cancelled()
            val n=(2*half/(if(satellite)3.5 else 2.0)).roundToInt();require(n==(if(satellite)400 else 1200)){"高德底图采样范围不正确"}
            val x0=(cx-half).roundToInt();val y0=(cy+half).roundToInt()
            val raw=take(x0,y0,n,45000)
            var different=false
            for(i in raw.indices step 7)if(raw[i]!=raw[0]){different=true;break}
            if(!different)throw IOException("高德返回纯色图片，保留当前底图")
            if(satellite)return SatelliteRaster.encode(f,cx,cy,half,raw,shouldCancel)
            // Each style starts from the captured SDK pixels, never another style's output.
            if(style==MapStylePolicy.ORIGINAL || style==MapStylePolicy.MINIMAL) {
                val mask=if(style==MapStylePolicy.MINIMAL)MinimalRoadRaster.indices(raw,n)else null
                val bytes=if(style==MapStylePolicy.ORIGINAL)OriginalRaster.encode(f,x0,y0,n,raw,shouldCancel)
                    else MapGeo.encodeRaster(f.id,x0,y0,n,n,2000,mask!!)
                if(!RasterPacket.valid(bytes,f.id,x0,y0))throw IOException("独立风格地图包不完整")
                onBaseReady?.invoke(bytes)
                val pv=300;val p=IntArray(pv*pv){i->val k=(i/pv*n/pv)*n+(i%pv*n/pv);if(mask==null)raw[k]else if(mask[k].toInt()==1)0xff808080.toInt()else 0xff000000.toInt()}
                val preview=if(makePreview)Bitmap.createBitmap(p,pv,pv,Bitmap.Config.ARGB_8888)else null
                return RasterResult(bytes,1,0,preview,null)
            }
            val bg=AmapRasterColors.ROAD_REFERENCE_LUM;val colors=HashMap<Int,Int>()
            val indices=ByteArray(n*n){i->colors.getOrPut(raw[i]){MapGeo.classifyPixel(raw[i],bg)}.toByte()}
            val roads=MapGeo.dilateRoads(MapGeo.cleanRaster(indices,n,n),n,n)
            val baseBytes=MapGeo.encodeRaster(f.id,x0,y0,n,n,2000,roads)
            if(!RasterPacket.valid(baseBytes,f.id,x0,y0))throw IOException("高德普通底图编码不完整")
            onBaseReady?.invoke(baseBytes)
            var marked: IntArray?=null
            var complete=false
            if(includeBuildings) {
                try {
                    val roofs=raw.clone()
                    for(ty in 0..1)for(tx in 0..1) {
                        cancelled()
                        val x=x0+tx*1200;val y=y0-ty*1200
                        val key="${f.refLat}_${f.refLng}_${x}_${y}"
                        val mask=buildingTiles[key] ?: run {
                            val tile=take(x,y,600,9000)
                            ByteArray(tile.size){i->if(AmapBuildings.marked(tile[i]))1 else 0}.also{buildingTiles[key]=it}
                        }
                        for(j in 0 until 600)for(i in 0 until 600)
                            if(mask[j*600+i].toInt()!=0)roofs[(ty*600+j)*n+tx*600+i]=0xffff00ff.toInt()
                    }
                    marked=roofs;complete=true
                }catch(e: CancellationException){throw e}
                catch(_: IOException){state="楼块尚未就绪，先显示普通底图"}
                catch(_: IllegalArgumentException){state="楼块图不完整，先显示普通底图"}
                catch(_: OutOfMemoryError){state="楼块处理内存不足，先显示普通底图"}
            }
            cancelled()
            var buildings: AmapBuildings.Result?=null
            if(complete)try{buildings=AmapBuildings.apply(marked!!,roads,n,n)}
            catch(_:IllegalArgumentException){complete=false;state="楼块图层异常，先显示普通底图"}
            catch(_:OutOfMemoryError){complete=false;state="楼块处理内存不足，先显示普通底图"}
            val display=buildings?.pixels ?: roads
            if(buildings!=null)state="高德楼块：${buildings.buildings} 片轮廓（该地图块）"
            val candidate=if(complete)MapGeo.encodeRaster(f.id,x0,y0,n,n,2000 or AmapBuildings.FLAG,display)else baseBytes
            val bytes=if(RasterPacket.valid(candidate,f.id,x0,y0))candidate else baseBytes.also{state="楼块数据过大，先显示普通底图"}
            if(!makePreview)return RasterResult(bytes,1,0,null,null)
            val pv=300;val pr=IntArray(pv*pv);val pc=IntArray(pv*pv)
            val pal=intArrayOf(0xffdcdcdc.toInt(),0xff303030.toInt(),0xff5bbf5b.toInt(),0xff4a90d9.toInt(),0xffa0a0a0.toInt(),0xff787878.toInt(),0xffa0a0a0.toInt(),0xff787878.toInt())
            val preview=if(bytes===candidate)display else roads
            for(y in 0 until pv)for(x in 0 until pv){val k=(y*n/pv)*n+x*n/pv;pr[y*pv+x]=(marked ?: raw)[k];pc[y*pv+x]=pal[preview[k].toInt() and 15]}
            return RasterResult(bytes,1,0,Bitmap.createBitmap(pr,pv,pv,Bitmap.Config.ARGB_8888),Bitmap.createBitmap(pc,pv,pv,Bitmap.Config.ARGB_8888))
        }catch(_:InterruptedException){Thread.currentThread().interrupt();throw CancellationException()}
        finally {
            active.get()?.set(true)
            // 取消先入主线程队列，再释放锁，防止旧请求取消下一次取图。
            main.post{owner.get()?.let{if(host===it)it.cancelCapture()}}
            if(locked)lease.unlock()
        }
    }
}
