package com.yuminaga.motonav

import android.app.Activity
import android.content.Context
import android.widget.EditText
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import org.json.JSONArray
import org.json.JSONObject
import java.util.UUID

data class FavoritePlace(val id: String, val name: String, val addr: String, val lat: Double, val lng: Double)

object FavoritePlaces {
    private fun prefs(c: Context) = c.getSharedPreferences("motonav", Context.MODE_PRIVATE)
    fun load(c: Context): List<FavoritePlace> = try {
        val a = JSONArray(prefs(c).getString("favorite_places_v1", "[]"))
        (0 until a.length()).mapNotNull { i ->
            runCatching {
                val o = a.getJSONObject(i)
                FavoritePlace(o.getString("id"), o.getString("name"), o.optString("addr"), o.getDouble("lat"), o.getDouble("lng"))
                    .takeIf { it.name.isNotBlank() && RideTrackMath.valid(it.lat,it.lng) }
            }.getOrNull()
        }.take(50)
    } catch (_: Exception) { emptyList() }
    fun find(c: Context, lat: Double, lng: Double) = load(c).firstOrNull { RideTrackMath.distance(lat,lng,it.lat,it.lng) < 15 }
    private fun write(c: Context, items: List<FavoritePlace>) {
        val a = JSONArray()
        items.forEach { a.put(JSONObject().put("id",it.id).put("name",it.name).put("addr",it.addr).put("lat",it.lat).put("lng",it.lng)) }
        // Small, explicit user action: commit lets the UI report a storage failure.
        check(prefs(c).edit().putString("favorite_places_v1",a.toString()).commit()) { "手机存储写入失败" }
    }
    @Synchronized fun save(c: Context, name: String, addr: String, lat: Double, lng: Double) {
        require(name.isNotBlank() && RideTrackMath.valid(lat,lng)) { "名称或坐标无效" }
        val old = load(c)
        val existing = find(c,lat,lng)
        require(existing != null || old.size < 50) { "最多保存 50 个常用目的地，请先删除不用的地点" }
        write(c,listOf(FavoritePlace(existing?.id ?: UUID.randomUUID().toString(),name.trim().take(60),addr.take(300),lat,lng)) + old.filter { it.id != existing?.id })
    }
    @Synchronized fun remove(c: Context, id: String) = write(c,load(c).filter { it.id != id })
}

object FavoritePlacesUi {
    fun save(a: Activity, name: String, addr: String, lat: Double, lng: Double, changed: () -> Unit = {}) {
        val old = FavoritePlaces.find(a,lat,lng)
        val input = EditText(a).apply { setSingleLine(); setText(old?.name ?: name); selectAll(); setPadding(32,24,32,24) }
        val d = AlertDialog.Builder(a).setTitle(if (old == null) "收藏到常用目的地" else "修改常用目的地")
            .setView(input).setPositiveButton("保存",null).setNegativeButton("取消",null)
            .apply { if (old != null) setNeutralButton("删除") { _,_ ->
                AlertDialog.Builder(a).setTitle("删除 ${old.name}？").setPositiveButton("删除") { _,_ ->
                    runCatching { FavoritePlaces.remove(a,old.id) }.onSuccess { changed() }.onFailure { toast(a,it.message) }
                }.setNegativeButton("取消",null).show()
            } }.create()
        d.setOnShowListener { d.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener {
            runCatching { FavoritePlaces.save(a,input.text.toString(),addr,lat,lng) }
                .onSuccess { changed(); d.dismiss(); toast(a,"已保存到常用目的地") }
                .onFailure { input.error = it.message ?: "保存失败" }
        } }
        d.show()
    }
    fun manage(a: Activity, changed: () -> Unit, navigate: (FavoritePlace) -> Unit) {
        val items = FavoritePlaces.load(a)
        if (items.isEmpty()) { toast(a,"在路线规划页点「收藏到常用目的地」即可添加"); return }
        AlertDialog.Builder(a).setTitle("常用目的地（${items.size}/50）")
            .setItems(items.map { it.name }.toTypedArray()) { _,i ->
                val p = items[i]
                AlertDialog.Builder(a).setTitle(p.name).setItems(arrayOf("规划路线","改名 / 删除")) { _,action ->
                    if (action == 0) navigate(p) else save(a,p.name,p.addr,p.lat,p.lng,changed)
                }.setNegativeButton("取消",null).show()
            }.setNegativeButton("关闭",null).show()
    }
    private fun toast(c: Context, text: String?) = Toast.makeText(c,text ?: "操作失败",Toast.LENGTH_LONG).show()
}
