package com.yuminaga.motonav

import org.json.JSONObject

/** Metadata from the same native calculation as the selectable paths. */
data class NativeRouteChoice(val index:Int,val metres:Int,val seconds:Int,val lights:Int,val tolls:Int,val roads:String,val method:String) {
    companion object {
        fun parse(raw:String,lat:Double,lng:Double):List<NativeRouteChoice> {
            val root=JSONObject(raw)
            val data=root.optJSONObject("data")?:root
            if(data.optInt("resultCode",10000)!=10000)return emptyList()
            if(CarSignalAlignment.metres(lat,lng,data.optDouble("endPOILatitude",Double.NaN),data.optDouble("endPOILongitude",Double.NaN))>100)return emptyList()
            // NaN distances cannot pass endpoint validation.
            if(!CarSignalAlignment.coordinate(data.optDouble("endPOILatitude",Double.NaN),data.optDouble("endPOILongitude",Double.NaN)))return emptyList()
            val routes=data.optJSONArray("protocolRouteInfos")?:return emptyList()
            if(routes.length() !in 1..10)return emptyList()
            return List(routes.length()){k->
                val r=routes.getJSONObject(k)
                val distance=r.getInt("distance");val seconds=r.getInt("time")
                require(distance in 1..5_000_000 && seconds in 1..1_000_000)
                val roads=r.optJSONArray("streetNames")
                val names=if(roads==null)emptyList() else List(roads.length()){roads.optString(it)}
                NativeRouteChoice(k,distance,seconds,r.optInt("trafficLights").coerceAtLeast(0),r.optInt("tolls").coerceAtLeast(0),
                    names.filter{it.isNotBlank()&&it !in listOf("我的位置","内部道路","MotoNav 起点","MotoNav 终点","目的地")}.distinct().joinToString(" → "),r.optString("method"))
            }
        }
    }
}
