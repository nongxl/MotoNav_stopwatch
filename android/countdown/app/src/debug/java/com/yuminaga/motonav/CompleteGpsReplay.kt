package com.yuminaga.motonav

import android.content.Context
import android.content.pm.ApplicationInfo
import android.location.Location
import android.location.LocationManager
import android.os.*
import org.json.JSONArray
import org.json.JSONObject
import java.io.File

/** Shell/DUMP-only, bounded GPS source. Android's mock flag stays intact. */
object CompleteGpsReplay {
    private val handler=Handler(Looper.getMainLooper())
    private var manager:LocationManager?=null
    private val owned=ArrayList<String>()
    private var points:List<DoubleArray> = emptyList()
    private var interval=500L
    private var started=0L
    private var index=0
    fun begin(c:Context){
        check(c.applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE!=0)
        check(owned.isEmpty())
        val f=File(c.filesDir,"single-replay.json");require(f.length() in 1..64000)
        val o=JSONObject(f.readText());val a=o.getJSONArray("points");val ms=o.optLong("intervalMs",500)
        require(ms in 500..2000 && a.length() in 1..360 && ms*a.length()<=180000)
        val data=List(a.length()){k->val p=a.getJSONArray(k);DoubleArray(4){p.getDouble(it)}}
        require(data.all{it.all{v->v.isFinite()}&&it[0] in -90.0..90.0&&it[1] in -180.0..180.0&&it[2] in 0.0..35.0&&it[3] in 0.0..360.0})
        manager=c.getSystemService(Context.LOCATION_SERVICE) as LocationManager
        try{
            for(name in listOf("gps","network")){
                @Suppress("DEPRECATION")
                manager!!.addTestProvider(name,false,name=="gps",false,false,true,true,true,3,1)
                owned.add(name);manager!!.setTestProviderEnabled(name,true)
            }
            points=data;interval=ms;started=SystemClock.elapsedRealtime();index=0;handler.post(tick)
        }catch(e:RuntimeException){stop();throw e}
    }
    private val tick=object:Runnable{override fun run(){
        if(index>=points.size||SystemClock.elapsedRealtime()-started>180000){stop();return}
        val p=points[index++]
        try{
            for(name in owned){
                val fix=Location(name).apply{
                    latitude=p[0];longitude=p[1];accuracy=4f;altitude=500.0
                    speed=p[2].toFloat();bearing=p[3].toFloat()
                    speedAccuracyMetersPerSecond=.5f;bearingAccuracyDegrees=3f
                    time=System.currentTimeMillis();elapsedRealtimeNanos=SystemClock.elapsedRealtimeNanos()
                }
                manager!!.setTestProviderLocation(name,fix)
            }
            handler.postDelayed(this,(started+index*interval-SystemClock.elapsedRealtime()).coerceAtLeast(1))
        }catch(_:RuntimeException){stop()}
    }}
    fun stop(){handler.removeCallbacks(tick);owned.forEach{runCatching{manager?.removeTestProvider(it)}};owned.clear();points=emptyList()}
    fun debug()=JSONObject().put("active",owned.isNotEmpty()).put("index",index).put("points",points.size)
}
