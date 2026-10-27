package com.yuminaga.motonav

import android.content.Context
import android.content.Intent
import android.os.Bundle
import java.util.UUID

/** Only user destination selections carry a command ID; returning to navigation does not. */
object SingleEngineDestination {
    const val EXTRA_ID = "single_destination_request_id"
    const val EXTRA_SHARE_TOKEN = "single_shared_route_token"
    private const val STATE_HANDLED = "single_destination_handled"
    private val ledger = DestinationRequestLedger()
    data class Request(val id: String, val lat: Double, val lng: Double, val name: String, val address: String, val shareToken:String="") {
        fun state() = Bundle().apply {
            putString(EXTRA_ID,id); putString(EXTRA_SHARE_TOKEN,shareToken); putDouble(RoutePlanActivity.EXTRA_LAT,lat); putDouble(RoutePlanActivity.EXTRA_LNG,lng)
            putString(RoutePlanActivity.EXTRA_NAME,name); putString(RoutePlanActivity.EXTRA_ADDR,address)
        }
    }
    fun intent(c: Context, name: String, address: String, lat: Double, lng: Double, id: String = UUID.randomUUID().toString(), shareToken:String="") =
        Intent(c,SingleEngineActivity::class.java).putExtras(Request(id,lat,lng,name,address,shareToken).state())
    fun decode(b: Bundle?): Request? {
        if (b == null) return null
        val lat=b.getDouble(RoutePlanActivity.EXTRA_LAT,Double.NaN); val lng=b.getDouble(RoutePlanActivity.EXTRA_LNG,Double.NaN)
        if (!CarSignalAlignment.coordinate(lat,lng)) return null
        val name=b.getString(RoutePlanActivity.EXTRA_NAME).orEmpty().ifBlank { "目的地" }
        val address=b.getString(RoutePlanActivity.EXTRA_ADDR).orEmpty()
        // Compatibility with an old task's launch Intent from before command IDs existed.
        val id=b.getString(EXTRA_ID)?.takeIf { it.isNotBlank() } ?: "legacy:$lat:$lng:$name:$address"
        val token=b.getString(EXTRA_SHARE_TOKEN).orEmpty()
        if(token.isNotEmpty() && !SharedRouteStore.validToken(token))return null
        return Request(id,lat,lng,name,address,token)
    }
    fun clear(i: Intent) {
        listOf(EXTRA_ID,EXTRA_SHARE_TOKEN,RoutePlanActivity.EXTRA_LAT,RoutePlanActivity.EXTRA_LNG,RoutePlanActivity.EXTRA_NAME,RoutePlanActivity.EXTRA_ADDR).forEach { i.removeExtra(it) }
    }
    fun handled(id:String)=ledger.contains(id)
    fun claim(request: Request) = ledger.claim(request.id)
    fun save(state: Bundle) { state.putStringArray(STATE_HANDLED,ledger.snapshot()) }
    fun restore(state: Bundle?) { ledger.restore(state?.getStringArray(STATE_HANDLED)) }
}
