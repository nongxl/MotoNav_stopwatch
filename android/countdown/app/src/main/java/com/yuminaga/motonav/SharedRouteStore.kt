package com.yuminaga.motonav

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.util.UUID

/** Private, bounded handoff. Geometry is never placed in an Android Intent. */
object SharedRouteStore {
    fun validToken(token:String)=token.matches(Regex("[a-f0-9]{32}"))
    private fun directory(c:Context)=File(c.cacheDir,"shared-routes").apply { mkdirs() }
    fun encode(r:AmapShare.Route):String {
        require(r.kind in listOf("驾车","摩托车","自行车")) { "不支持的分享交通方式" }
        require(r.pts.size in 2..50000 && r.pts.all { it.size>=2 && CarSignalAlignment.coordinate(it[0],it[1]) }) { "分享路线坐标无效或过长" }
        fun points(p:List<DoubleArray>)=JSONArray(p.map { JSONArray(it.take(2)) })
        val o=JSONObject().put("startName",r.startName).put("startLat",r.startLat).put("startLng",r.startLng)
            .put("endName",r.endName).put("endLat",r.endLat).put("endLng",r.endLng)
            .put("points",points(r.pts)).put("roads",JSONArray(r.roads.map { JSONObject().put("name",it.name).put("length",it.length).put("points",points(it.pts)) }))
            .put("distance",r.distance).put("seconds",r.timeS).put("lights",r.lights).put("restored",r.restored)
            .put("kind",r.kind).put("endpointsOnly",r.endpointsOnly).put("sourceUrl",r.sourceUrl).put("nativeUri",r.nativeUri).put("importNote",r.importNote).put("verificationUrl",r.verificationUrl).put("verificationPathIndex",r.verificationPathIndex)
        return o.toString().also { require(it.length<=8_000_000) { "分享路线过长，请分段导入" } }
    }
    fun decode(text:String):AmapShare.Route {
        require(text.length<=8_000_000)
        val o=JSONObject(text)
        fun points(a:JSONArray)=List(a.length()) { i->val p=a.getJSONArray(i);doubleArrayOf(p.getDouble(0),p.getDouble(1)) }
        val roads=o.getJSONArray("roads")
        val r=AmapShare.Route(o.getString("startName"),o.getDouble("startLat"),o.getDouble("startLng"),o.getString("endName"),o.getDouble("endLat"),o.getDouble("endLng"),
            points(o.getJSONArray("points")),List(roads.length()){i->val p=roads.getJSONObject(i);AmapShare.Road(p.getString("name"),p.getInt("length"),points(p.getJSONArray("points")))},
            o.getInt("distance"),o.getInt("seconds"),o.getInt("lights"),o.getBoolean("restored"),o.getString("kind"),o.getBoolean("endpointsOnly"))
        require(CarSignalAlignment.coordinate(r.startLat,r.startLng) && CarSignalAlignment.coordinate(r.endLat,r.endLng))
        require(r.kind in listOf("驾车","摩托车","自行车") && r.pts.size in 2..50000 && r.pts.all { CarSignalAlignment.coordinate(it[0],it[1]) })
        return r.apply {sourceUrl=o.optString("sourceUrl");nativeUri=o.optString("nativeUri");importNote=o.optString("importNote");verificationUrl=o.optString("verificationUrl");verificationPathIndex=o.optInt("verificationPathIndex")}
    }
    @Synchronized fun save(c:Context,r:AmapShare.Route):String {
        val text=encode(r);val dir=directory(c);val token=UUID.randomUUID().toString().replace("-","")
        val file=File(dir,"$token.json");val temp=File(dir,"$token.tmp");temp.writeText(text);check(temp.renameTo(file))
        dir.listFiles()?.filter { it.extension=="json" }?.sortedByDescending { it.lastModified() }?.drop(20)?.forEach { it.delete() }
        return token
    }
    fun load(c:Context,token:String):AmapShare.Route {
        require(validToken(token)) { "分享路线标记无效，请重新导入" }
        val f=File(directory(c),"$token.json")
        require(f.exists() && f.length() in 1..16_000_000) { "分享缓存已失效，请重新分享路线" }
        return decode(f.readText())
    }
}
