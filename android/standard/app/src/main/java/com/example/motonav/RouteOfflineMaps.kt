package com.example.motonav

import android.content.Context
import java.io.File
import java.io.IOException
import java.util.concurrent.CancellationException
import java.util.concurrent.Executors
import kotlin.math.roundToInt

/** 保存可直接发给圆屏的普通底图；完成后不依赖网络，也不再重算抗锯齿。 */
class RouteOfflineMaps(private val ctx: Context, val frame: MapGeo.Frame, val plan: RouteMapPlan,
                       private val foregroundBusy: () -> Boolean,
                       private val onReady: (RouteMapPlan.Node) -> Unit = {}, private val buildings: Boolean=false, private val style: Int=MapStylePolicy.VICE_CITY) {
    private val worker = Executors.newSingleThreadExecutor { r -> Thread(r, "route-offline").apply { priority=Thread.MIN_PRIORITY; isDaemon=true } }
    private val dir = File(ctx.filesDir, "route_maps/${MapStylePolicy.cacheKey(style,buildings)}/${frame.refLat}_${frame.refLng}").apply { mkdirs() }
    private val ready = BooleanArray(plan.nodes.size)
    @Volatile private var closed = false
    @Volatile private var target = 0
    @Volatile var status = "沿线离线地图：准备中"
        private set
    private fun file(node: RouteMapPlan.Node) = File(dir, "${node.x.roundToInt()}_${node.y.roundToInt()}.bin")
    private fun baseFile(node: RouteMapPlan.Node)=File(ctx.filesDir,"route_maps/amap-sdk-v3/${frame.refLat}_${frame.refLng}/${node.x.roundToInt()}_${node.y.roundToInt()}.bin")
    fun readBase(node: RouteMapPlan.Node): ByteArray? {
        if(style!=MapStylePolicy.VICE_CITY)return null
        val f=baseFile(node)
        return try {
            if(!f.isFile || f.length() !in 17..480000)null
            else f.readBytes().takeIf{!RasterPacket.isBuilding(it) && RasterPacket.valid(it,frame.id,(node.x-1200).roundToInt(),(node.y+1200).roundToInt())}
        }catch(_:IOException){null}
    }
    private fun storeBase(node: RouteMapPlan.Node,bytes: ByteArray) {
        if(closed)return
        if(readBase(node)!=null)return
        val plain=RasterPacket.forLegacy(bytes)
        if(!RasterPacket.valid(plain,frame.id,(node.x-1200).roundToInt(),(node.y+1200).roundToInt()))throw IOException("普通地图包不完整")
        val target=baseFile(node);target.parentFile?.mkdirs()
        if((target.parentFile?.usableSpace ?: 0L)<32L*1024*1024)throw IOException("手机存储空间不足")
        if(File(ctx.filesDir,"route_maps").walkTopDown().filter{it.isFile}.sumOf{it.length()}>512L*1024*1024)throw IOException("离线地图缓存已达 512 MB")
        val temp=File.createTempFile("base-",".tmp",target.parentFile)
        try{temp.writeBytes(plain);if(!temp.renameTo(target))throw IOException("普通底图保存失败")}
        finally{temp.delete()}
    }
    fun update(index: Int) { target=index.coerceIn(0,plan.nodes.lastIndex) }
    fun read(node: RouteMapPlan.Node): ByteArray? = synchronized(dir.path.intern()) {
        val f=file(node)
        try {
            if (!f.isFile || f.length() !in 17..480000) return@synchronized null
            val b=f.readBytes()
            if(buildings!=RasterPacket.isBuilding(b))return@synchronized null
            if (!RasterPacket.valid(b, frame.id, (node.x-1200).roundToInt(), (node.y+1200).roundToInt())) {
                f.delete(); null
            } else b
        } catch (_: IOException) { null }
    }
    fun store(node: RouteMapPlan.Node, bytes: ByteArray) = synchronized(dir.path.intern()) {
        if (closed) return@synchronized
        if(buildings && readBase(node)==null)storeBase(node,bytes)
        if(buildings!=RasterPacket.isBuilding(bytes))throw IOException("楼块尚未准备完整，普通地图继续显示")
        if (!RasterPacket.valid(bytes, frame.id, (node.x-1200).roundToInt(), (node.y+1200).roundToInt())) throw IOException("离线底图校验失败")
        if (dir.usableSpace < 32L*1024*1024) throw IOException("手机存储空间不足")
        // 活跃路线不淘汰；空间上限达到时明确提示，不能悄悄把已离线的前方地图删除。
        val root=File(ctx.filesDir,"route_maps")
        if (root.walkTopDown().filter { it.isFile }.sumOf { it.length() } > 512L*1024*1024) throw IOException("离线地图缓存已达 512 MB")
        val tmp=File.createTempFile("map-", ".tmp", dir)
        try { tmp.writeBytes(bytes); if (!tmp.renameTo(file(node))) throw IOException("离线地图保存失败") }
        finally { tmp.delete() }
    }
    fun start() { worker.execute {
        try {
            for (i in ready.indices) {
                if (closed) return@execute
                ready[i]=read(plan.nodes[i])!=null
                if (ready[i] || (buildings && readBase(plan.nodes[i])!=null)) onReady(plan.nodes[i])
            }
            var retryAt=0L
            while (!closed) {
                if (foregroundBusy() || System.currentTimeMillis()<retryAt) { Thread.sleep(250); continue }
                val wanted=target
                val index=(wanted..ready.lastIndex).firstOrNull { !ready[it] }
                    ?: (0 until wanted).firstOrNull { !ready[it] }
                val count=ready.count { it }
                if (index==null) { status="沿线底图已全部离线：$count/${ready.size} 块（2 米/像素）"; return@execute }
                status="沿线离线地图：$count/${ready.size} 块，正在下载第 ${index+1} 块"
                val node=plan.nodes[index]
                try {
                    if (read(node)!=null) { ready[index]=true; onReady(node); continue }
                    val r=AmapSdkRaster.build(frame,node.x,node.y,1200.0,makePreview=false,
                        shouldCancel={ closed || foregroundBusy() },includeBuildings=buildings,style=style,
                        onBaseReady=if(buildings)({base->storeBase(node,base);onReady(node)})else null)
                    if (closed) return@execute
                    store(node,r.bytes); ready[index]=true; onReady(node)
                    retryAt=0L
                } catch (_: CancellationException) { Thread.sleep(250) }
                catch (e: IOException) { status="沿线离线地图：$count/${ready.size} 块，${e.message}（稍后重试）"; retryAt=System.currentTimeMillis()+15000 }
                catch (e: Exception) { status="沿线离线地图：$count/${ready.size} 块，下载未完成（稍后重试）"; retryAt=System.currentTimeMillis()+5000 }
            }
        } catch (_: InterruptedException) { }
        catch (_: OutOfMemoryError) { status="沿线离线地图：手机内存不足，保留已下载底图" }
    } }
    fun close() { closed=true; worker.shutdownNow() }
}
