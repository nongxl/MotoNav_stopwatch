package com.example.motonav

import com.amap.api.maps.model.LatLng
import com.amap.api.navi.AMapNavi
import com.amap.api.navi.enums.TravelStrategy
import com.amap.api.navi.model.NaviLatLng
import com.amap.api.navi.model.NaviPoi

/** 初次规划和偏航回原路线共用，始终保持分享的交通方式。 */
object ShareRouting {
    fun calculate(navi: AMapNavi, route: AmapShare.Route, lat: Double, lng: Double,
                  waypoints: List<NaviLatLng>, driveStrategy: Int): Boolean {
        navi.setIsNaviTravelView(route.isTravel)
        navi.setMultipleRouteNaviMode(false)
        val from = NaviLatLng(if (lat.isNaN()) route.startLat else lat,
            if (lng.isNaN()) route.startLng else lng)
        val to = NaviLatLng(route.endLat, route.endLng)
        fun poi(point: NaviLatLng, name: String) = NaviPoi(name, LatLng(point.latitude, point.longitude), "")
        return when (route.kind) {
            "自行车" -> navi.calculateRideRoute(poi(from, "起点"),
                waypoints.map { poi(it, "沿分享路线") }, poi(to, route.endName), TravelStrategy.SINGLE)
            "驾车", "摩托车" -> navi.calculateDriveRoute(arrayListOf(from), arrayListOf(to), waypoints, driveStrategy)
            else -> false
        }
    }
}
