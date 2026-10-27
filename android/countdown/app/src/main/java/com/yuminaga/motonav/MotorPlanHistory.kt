package com.yuminaga.motonav

import android.content.Context
import org.json.JSONArray
import org.json.JSONObject

/** Successful plans only. Route geometry and login cookies are never saved here. */
object MotorPlanHistory {
    data class Item(val request: MotorRequest, val currentStart: Boolean)
    private fun prefs(c: Context) = c.getSharedPreferences("webmotor", Context.MODE_PRIVATE)
    fun load(c: Context): List<Item> = runCatching {
        val a = JSONArray(prefs(c).getString("plan_history_v1", "[]"))
        (0 until a.length()).mapNotNull { i -> runCatching {
            val o = a.getJSONObject(i)
            val request = WebMotorRoutes.request(o.getString("url"))
            require(MotorTrack.valid(request.start.lat, request.start.lng) && MotorTrack.valid(request.end.lat, request.end.lng))
            Item(request, o.optBoolean("currentStart"))
        }.getOrNull() }.take(20)
    }.getOrDefault(emptyList())
    private fun same(a: Item, b: Item): Boolean = a.currentStart == b.currentStart &&
        (a.currentStart || MotorTrack.distance(a.request.start.lat,a.request.start.lng,b.request.start.lat,b.request.start.lng) < 15) &&
        MotorTrack.distance(a.request.end.lat,a.request.end.lng,b.request.end.lat,b.request.end.lng) < 15 &&
        a.request.vias.size == b.request.vias.size && a.request.vias.indices.all{
            val p=a.request.vias[it];val q=b.request.vias[it]
            MotorTrack.distance(p.lat,p.lng,q.lat,q.lng)<15
        }
    @Synchronized fun add(c: Context, request: MotorRequest, currentStart: Boolean) {
        val item = Item(request,currentStart)
        val a = JSONArray()
        (listOf(item) + load(c).filterNot { same(it,item) }).take(20).forEach {
            a.put(JSONObject().put("url",it.request.pageUrl()).put("currentStart",it.currentStart))
        }
        check(prefs(c).edit().putString("plan_history_v1",a.toString()).commit()) { "历史记录保存失败" }
    }
    fun clear(c: Context) {
        check(prefs(c).edit().remove("plan_history_v1").commit()) { "历史记录清除失败" }
    }
}
