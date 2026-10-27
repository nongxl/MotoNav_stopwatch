package com.yuminaga.motonav

import android.content.Context
import android.net.Uri
import android.os.Handler
import android.os.Looper
import android.util.AtomicFile
import android.util.Xml
import org.json.JSONObject
import java.io.BufferedWriter
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import java.util.TimeZone
import java.util.UUID
import java.util.concurrent.Executors

data class RideSummary(val id: String, val title: String, val mode: String, val start: Long, val end: Long,
    val meters: Double, val points: Int, val maxKmh: Double, val active: Boolean, val interrupted: Boolean = false) {
    fun json() = JSONObject().put("version",1).put("id",id).put("title",title).put("mode",mode).put("start",start)
        .put("end",end).put("meters",meters).put("points",points).put("maxKmh",maxKmh).put("active",active).put("interrupted",interrupted)
    companion object {
        fun from(o: JSONObject) = RideSummary(o.getString("id"),o.getString("title"),o.getString("mode"),o.getLong("start"),
            o.getLong("end"),o.getDouble("meters"),o.getInt("points"),o.getDouble("maxKmh"),o.optBoolean("active"),o.optBoolean("interrupted"))
    }
}
data class RidePoint(val lat: Double, val lng: Double, val time: Long, val segment: Boolean)

/** Private, append-only tracks. All disk work and lifecycle events share one FIFO worker. */
object RideRecorder {
    private lateinit var context: Context
    private lateinit var directory: File
    private val worker = Executors.newSingleThreadExecutor { r -> Thread(r,"MotoNav-rides") }
    private val main = Handler(Looper.getMainLooper())
    @Volatile var enabled = true; private set
    @Volatile var live: RideSummary? = null; private set
    @Volatile private var requestedLabel: String? = null
    @Volatile private var error: String? = null
    private var request: Pair<String,String>? = null // Worker-confined from here on.
    private var gate = RideTrackMath.Gate()
    private var writer: BufferedWriter? = null
    private var session: RideSummary? = null
    private var lastMetadataAt = 0L
    private var failed = false

    fun init(c: Context) {
        context = c.applicationContext
        // noBackupFilesDir also excludes tracks from Android cloud/device backup.
        directory = File(context.noBackupFilesDir,"ride_tracks")
        enabled = context.getSharedPreferences("motonav",0).getBoolean("record_rides_v1",true)
        work { check(directory.isDirectory || directory.mkdirs()); recover() }
    }
    fun start(mode: String, title: String, simulation: Boolean = false) = work {
        finishSession()
        request = if (simulation) null else mode to title.take(120)
        requestedLabel = request?.first
        gate = RideTrackMath.Gate(); failed = false; error = null
    }
    fun stop() = work { request = null; requestedLabel = null; finishSession() }
    fun setEnabled(value: Boolean) {
        enabled = value
        work {
            check(context.getSharedPreferences("motonav",0).edit().putBoolean("record_rides_v1",value).commit())
            if (!value) finishSession()
            gate = RideTrackMath.Gate(); failed = false; error = null
        }
    }
    fun status(): String {
        error?.let { return "轨迹保存失败，请检查手机存储后重新开启记录" }
        if (!enabled) return "轨迹记录已关闭"
        live?.let { return "轨迹记录中 · ${distanceText(it.meters)} · ${it.points} 个点" }
        return if (requestedLabel != null) "轨迹记录已开启 · 等待有效定位" else "轨迹自动记录已开启"
    }
    fun distanceText(m: Double) = if (m < 1000) "${m.toInt()} 米" else String.format(Locale.CHINA,"%.2f 公里",m/1000)
    fun durationText(ms: Long): String { val m = ms.coerceAtLeast(0)/60000; return if (m < 60) "${m} 分钟" else "${m/60} 小时 ${m%60} 分钟" }

    fun onFix(lat: Double, lng: Double, stamp: Long, wall: Long, accuracy: Double, kmh: Double) = work {
        val req = request ?: return@work
        if (!enabled || failed) return@work
        val accepted = gate.offer(lat,lng,stamp,wall,accuracy)
        if (accepted == 0) return@work
        val time = RideTrackMath.timestamp(stamp,wall)
        if (session == null) {
            val id = UUID.randomUUID().toString()
            val s = RideSummary(id,req.second.ifBlank { req.first },req.first,time,time,0.0,0,0.0,true)
            save(s) // Metadata first: recovery can find even a just-opened track.
            writer = pointsFile(id).bufferedWriter(Charsets.UTF_8)
            session = s
        }
        val s = session!!
        val speed = kmh.takeIf { it.isFinite() && it in 0.0..300.0 } ?: 0.0
        val next = s.copy(end=time,meters=s.meters+gate.addedMeters,points=s.points+1,maxKmh=maxOf(s.maxKmh,speed))
        val line = JSONObject().put("lat",lat).put("lng",lng).put("time",time).put("segment",accepted==2)
            .put("meters",next.meters).put("maxKmh",next.maxKmh)
        writer!!.apply { write(line.toString()); newLine(); flush() }
        session = next; live = next
        if (time-lastMetadataAt >= 10000) { save(next); lastMetadataAt=time }
    }

    fun history(callback: (List<RideSummary>, String?) -> Unit) {
        worker.execute {
            val result = runCatching { summaries().sortedByDescending { it.start } }
            main.post { callback(result.getOrDefault(emptyList()),result.exceptionOrNull()?.message) }
        }
    }
    fun preview(id: String, callback: (List<RidePoint>, String?) -> Unit) {
        worker.execute {
            val result = runCatching {
                val file = pointsFile(id)
                // Keep preview bounded; exports always contain every accepted point.
                val count = file.useLines { it.count() }
                val stride = maxOf(1,(count+2499)/2500)
                val p = ArrayList<RidePoint>()
                var last: RidePoint? = null
                var index = 0
                file.forEachLine(Charsets.UTF_8) { line ->
                    runCatching { point(JSONObject(line)) }.getOrNull()?.let { point ->
                        if (point.segment) {
                            last?.let { if (p.lastOrNull() != it) p.add(it) }
                            p.add(point)
                        } else if (index % stride == 0) {
                            p.add(point)
                        }
                        index++; last=point
                    }
                }
                last?.let { if (p.lastOrNull() != it) p.add(it) }
                p
            }
            main.post { callback(result.getOrDefault(emptyList()),result.exceptionOrNull()?.message) }
        }
    }
    fun delete(id: String, callback: (String?) -> Unit) = worker.execute {
        val r = runCatching {
            check(session?.id != id) { "这条轨迹仍在记录，请先结束导航或巡航" }
            val p = pointsFile(id); val m = metaFile(id)
            check(!p.exists() || p.delete()) { "删除轨迹失败" }
            check(!m.exists() || m.delete()) { "删除记录失败" }
        }
        main.post { callback(r.exceptionOrNull()?.message) }
    }
    fun export(id: String, uri: Uri, callback: (String?) -> Unit) = worker.execute {
        val r = runCatching {
            writer?.flush()
            val summary = readSummary(metaFile(id))
            val utc = SimpleDateFormat("yyyy-MM-dd'T'HH:mm:ss.SSS'Z'",Locale.US).apply { timeZone=TimeZone.getTimeZone("UTC") }
            val output = context.contentResolver.openOutputStream(uri,"wt") ?: error("无法写入所选文件")
            output.use { out ->
                val xml = Xml.newSerializer()
                xml.setOutput(out,"UTF-8"); xml.startDocument("UTF-8",true)
                xml.startTag(null,"gpx").attribute(null,"version","1.1").attribute(null,"creator","MotoNav")
                    .attribute(null,"xmlns","http://www.topografix.com/GPX/1/1")
                xml.startTag(null,"trk").startTag(null,"name").text(summary.title).endTag(null,"name")
                var segmentOpen = false
                pointsFile(id).forEachLine(Charsets.UTF_8) { line ->
                    val p = runCatching { point(JSONObject(line)) }.getOrNull() ?: return@forEachLine
                    if (p.segment || !segmentOpen) {
                        if (segmentOpen) xml.endTag(null,"trkseg")
                        xml.startTag(null,"trkseg"); segmentOpen=true
                    }
                    val wgs = MapGeo.gcjToWgs(p.lat,p.lng)
                    xml.startTag(null,"trkpt").attribute(null,"lat",String.format(Locale.US,"%.7f",wgs[0]))
                        .attribute(null,"lon",String.format(Locale.US,"%.7f",wgs[1]))
                        .startTag(null,"time").text(utc.format(Date(p.time))).endTag(null,"time").endTag(null,"trkpt")
                }
                if (segmentOpen) xml.endTag(null,"trkseg")
                xml.endTag(null,"trk").endTag(null,"gpx"); xml.endDocument(); xml.flush()
            }
        }
        main.post { callback(r.exceptionOrNull()?.message) }
    }
    private fun point(o: JSONObject): RidePoint {
        val p = RidePoint(o.getDouble("lat"),o.getDouble("lng"),o.getLong("time"),o.optBoolean("segment"))
        require(RideTrackMath.valid(p.lat,p.lng) && p.time>0)
        return p
    }
    private fun safeId(id: String): String { require(UUID.fromString(id).toString()==id); return id }
    private fun metaFile(id: String) = File(directory,"${safeId(id)}.json")
    private fun pointsFile(id: String) = File(directory,"${safeId(id)}.jsonl")
    private fun readSummary(f: File): RideSummary = RideSummary.from(JSONObject(AtomicFile(f).openRead().bufferedReader(Charsets.UTF_8).use { it.readText() }))
    private fun summaries(): List<RideSummary> = directory.listFiles { f -> f.extension=="json" }.orEmpty().mapNotNull { f ->
        runCatching { readSummary(f) }.getOrNull()?.let { if (it.id==session?.id) session else it }
    }
    private fun save(s: RideSummary) {
        val file = AtomicFile(metaFile(s.id)); val out = file.startWrite()
        try { out.write(s.json().toString().toByteArray(Charsets.UTF_8)); file.finishWrite(out) }
        catch (e: Exception) { file.failWrite(out); throw e }
    }
    private fun finishSession() {
        writer?.close(); writer=null
        session?.let { s ->
            save(s.copy(active=false))
        }
        session=null; live=null; lastMetadataAt=0
    }
    private fun recover() {
        for (s in summaries().filter { it.active }) {
            var count=0; var end=s.start; var meters=0.0; var speed=0.0
            val file = pointsFile(s.id)
            if (file.exists()) file.forEachLine(Charsets.UTF_8) { line ->
                runCatching { JSONObject(line) }.getOrNull()?.let { o ->
                    if (runCatching { point(o) }.isSuccess) {
                        count++; end=o.getLong("time"); meters=o.optDouble("meters",meters); speed=o.optDouble("maxKmh",speed)
                    }
                }
            }
            if (count==0) { file.delete(); metaFile(s.id).delete() }
            else save(s.copy(points=count,end=end,meters=meters,maxKmh=speed,active=false,interrupted=true))
        }
    }
    private fun work(action: () -> Unit) = worker.execute {
        try { action() } catch (_: Exception) {
            error = "storage"; failed=true
            runCatching { writer?.close() }; writer=null
            // Leave active metadata for recovery on the next app start.
            session=null; live=null
        }
    }
}
