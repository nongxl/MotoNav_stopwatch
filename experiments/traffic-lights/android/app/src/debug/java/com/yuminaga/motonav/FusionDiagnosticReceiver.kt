package com.yuminaga.motonav

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.util.Log
import org.json.JSONObject

/** Local debug regression checks. Shell/system DUMP permission is mandatory.
 * No system location or traffic-light injection. Navigation uses the selected SDK path.
 */
class FusionDiagnosticReceiver : BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        when (intent.getStringExtra("op")) {
            "settings" -> if(NavigationUi.active()) {
                val target=if(WebMotorGuide.active)MotorWebNavActivity::class.java else NavActivity::class.java
                context.startActivity(Intent(context,target)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP)
                    .putExtra(NavigationUi.EXTRA_SETTINGS_PAGE,intent.getStringExtra("page")?.toIntOrNull()?.coerceIn(0,4) ?: 1))
            }
            "returnNavigation" -> NavigationUi.returnToNavigation(context)
            "endNavigation" -> { if(WebMotorGuide.active)WebMotorGuide.stop() else NaviBridge.exitNavi() }
            "motorPlan" -> {
                val raw=intent.getStringExtra("url") ?: return
                val request=runCatching { WebMotorRoutes.request(raw) }.getOrNull() ?: return
                context.startActivity(Intent(context,MotorWebPlannerActivity::class.java)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    .putExtra(MotorWebPlannerActivity.EXTRA_REQUEST_URL,request.pageUrl()))
            }
            "motorRouteDump" -> WebMotorGuide.route?.let{route->
                val points=org.json.JSONArray()
                route.points.forEach{points.put(org.json.JSONArray(it.toList()))}
                java.io.File(context.filesDir,"web-selected-route.json").writeText(JSONObject()
                    .put("key",route.key).put("points",points).put("distance",route.distance)
                    .put("roads",org.json.JSONArray(route.steps.map{it.road})).toString())
            }
            "motorNavigate" -> {
                // Shell-only regression entry: use a genuine web result, never a driving fallback.
                val raw=intent.getStringExtra("url") ?: return
                val request=runCatching{WebMotorRoutes.request(raw)}.getOrNull() ?: return
                val index=intent.getStringExtra("index")?.toIntOrNull() ?: 0
                if(index !in 0..9 || NavigationUi.active() || !NavService.running)return
                Thread {
                    val result=runCatching{WebMotorRoutes.load(request).getOrNull(index) ?: error("No selected route")}
                    android.os.Handler(android.os.Looper.getMainLooper()).post {
                        result.onSuccess{route ->
                                if(NavigationUi.active() || !NavService.running) {
                                    Log.i("MotoNavMotorPlan","motorNavigate skipped: active guidance or service not ready")
                                } else {
                                    WebMotorGuide.start(context,route,false)
                                    context.startActivity(Intent(context,MotorWebNavActivity::class.java)
                                        .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
                                    Log.i("MotoNavMotorPlan",JSONObject().put("event","web-navigation-started")
                                        .put("routeKey",route.key).put("distance",route.distance)
                                        .put("points",route.points.size).put("steps",route.steps.size).toString())
                                }
                        }.onFailure{Log.w("MotoNavMotorPlan","motorNavigate failed",it)}
                    }
                }.start()
            }
            "motorGuideStatus" -> Log.i("MotoNavMotorPlan",JSONObject()
                .put("active",WebMotorGuide.active).put("simulation",WebMotorGuide.simulation)
                .put("remaining",WebMotorGuide.remaining).put("instruction",WebMotorGuide.instruction)
                .put("routeKey",WebMotorGuide.route?.key).put("road",WebMotorGuide.currentRoad)
                .put("webSignals",WebMotorSignals.debug()).put("carSync",CarNavSync.debug())
                .put("routeDistance",WebMotorGuide.route?.distance ?: -1).toString())
            "connect" -> BleLink.connect(context)
            "junctionLayout" -> if(BleLink.verified)BleLink.sendEvent("#TLP,1")
            "status" -> Log.i("MotoNavFusionCheck", JSONObject()
                .put("version", APP_VERSION)
                .put("navigationEpoch",NaviBridge.navigationEpoch())
                .put("webNavigation",JSONObject().put("active",WebMotorGuide.active).put("simulation",WebMotorGuide.simulation)
                    .put("remaining",WebMotorGuide.remaining).put("turnMetres",WebMotorGuide.turnMeters)
                    .put("direction",WebMotorGuide.direction).put("road",WebMotorGuide.currentRoad)
                    .put("nextRoad",WebMotorGuide.nextRoad).put("progress",WebMotorGuide.routeProgress))
                .put("returnNavigationAvailable",NavigationUi.active())
                .put("bleConnected", BleLink.isConnected).put("bleVerified", BleLink.verified)
                .put("map", MapFeeder.infoText()).put("raster", AmapSdkRaster.state)
                .put("lightContext",NaviBridge.trafficLightOverlayDebug())
                .put("navigationInfo",NaviBridge.lastInfo).put("navigationExtra",NaviBridge.extraInfo)
                .put("lights", TrafficLightOverlay.summary())
                .put("lightWantedDirection",TrafficLightOverlay.wantedDirection)
                .put("junctionDirections",org.json.JSONArray(TrafficLightOverlay.snapshots().map{it.dir}))
                .put("upstreamDirections",org.json.JSONArray(TrafficLightOverlay.upstreamDirections))
                .put("webSignals",WebMotorSignals.debug()).put("carSync",CarNavSync.debug()).toString())
            "preview" -> {
                val lat=intent.getStringExtra("lat")?.toDoubleOrNull() ?: return
                val lng=intent.getStringExtra("lng")?.toDoubleOrNull() ?: return
                if(!lat.isFinite() || !lng.isFinite() || lat !in -90.0..90.0 || lng !in -180.0..180.0)return
                context.startActivity(Intent(context, RoutePlanActivity::class.java)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    .putExtra(RoutePlanActivity.EXTRA_FROM_HOME, true)
                    .putExtra(RoutePlanActivity.EXTRA_MODE, LocalRouting.DRIVE)
                    .putExtra(RoutePlanActivity.EXTRA_NAME,"融合版回归验证")
                    .putExtra(RoutePlanActivity.EXTRA_LAT,lat).putExtra(RoutePlanActivity.EXTRA_LNG,lng))
            }
            "simulate" -> context.startActivity(Intent(context, NavActivity::class.java)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                .putExtra("emu",true).putExtra("dest","融合版回归验证"))
            "navigate" -> context.startActivity(Intent(context, NavActivity::class.java)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                .putExtra("emu",false).putExtra("dest","融合版实际导航验证"))
            "speed" -> {
                val speed=intent.getStringExtra("kmh")?.toIntOrNull() ?: return
                if(speed !in listOf(40,60,100,150) || !NaviBridge.trafficLightOverlayDebug().optBoolean("simulation"))return
                com.amap.api.navi.AMapNavi.getInstance(context).setEmulatorNaviSpeed(speed)
                Log.i("MotoNavFusionCheck","SDK simulation speed=$speed; real GPS unchanged")
            }
        }
    }
}
