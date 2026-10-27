package com.yuminaga.motonav

import android.content.Context
import android.content.pm.ApplicationInfo
import android.location.Location
import android.location.LocationManager
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import org.json.JSONObject

/** Local shell/DUMP regression source. Only coordinates are simulated; Android mock flags remain intact. */
object LiveGpsReplay {
    private val handler=Handler(Looper.getMainLooper())
    private var manager:LocationManager?=null
    private val owned=ArrayList<String>()
    private var point:DoubleArray?=null
    private var inputAt=0L
    private var expires=0L
    fun begin(c:Context) {
        check(c.applicationInfo.flags and ApplicationInfo.FLAG_DEBUGGABLE!=0)
        check(owned.isEmpty() && !CompleteGpsReplay.debug().optBoolean("active"))
        manager=c.getSystemService(Context.LOCATION_SERVICE) as LocationManager
        try {
            for(name in listOf("gps","network")) {
                @Suppress("DEPRECATION")
                manager!!.addTestProvider(name,false,name=="gps",false,false,true,true,true,3,1)
                owned.add(name);manager!!.setTestProviderEnabled(name,true)
            }
            expires=SystemClock.elapsedRealtime()+180000;handler.post(tick)
        }catch(e:RuntimeException){stop();throw e}
    }
    fun fix(lat:Double,lng:Double,speed:Double,bearing:Double) {
        require(listOf(lat,lng,speed,bearing).all{it.isFinite()} && lat in -90.0..90.0 && lng in -180.0..180.0 && speed in 0.0..35.0 && bearing in 0.0..360.0)
        check(owned.isNotEmpty())
        point=doubleArrayOf(lat,lng,speed,bearing);inputAt=SystemClock.elapsedRealtime()
    }
    fun renew(){check(owned.isNotEmpty());expires=SystemClock.elapsedRealtime()+180000}
    private val tick=object:Runnable{override fun run(){
        val now=SystemClock.elapsedRealtime()
        if(now>=expires){stop();return}
        try {
            point?.let{p->for(name in owned){
                val loc=Location(name).apply {
                    latitude=p[0];longitude=p[1];accuracy=4f;altitude=500.0
                    speed=if(now-inputAt<=3000)p[2].toFloat() else 0f;bearing=p[3].toFloat()
                    speedAccuracyMetersPerSecond=.5f;bearingAccuracyDegrees=3f
                    time=System.currentTimeMillis();elapsedRealtimeNanos=SystemClock.elapsedRealtimeNanos()
                }
                manager!!.setTestProviderLocation(name,loc)
            }}
            handler.postDelayed(this,200)
        }catch(_:RuntimeException){stop()}
    }}
    fun stop(){handler.removeCallbacks(tick);owned.forEach{runCatching{manager?.removeTestProvider(it)}};owned.clear();point=null;manager=null;inputAt=0;expires=0}
    fun debug()=JSONObject().put("active",owned.isNotEmpty()).put("inputAge",if(inputAt==0L)-1 else SystemClock.elapsedRealtime()-inputAt)
        .put("expiresIn",(expires-SystemClock.elapsedRealtime()).coerceAtLeast(0))
}
