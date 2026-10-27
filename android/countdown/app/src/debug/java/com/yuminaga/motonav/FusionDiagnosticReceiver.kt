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
    companion object { private var routeCheckCandidates:List<WebMotorRoute> = emptyList() }
    private fun routeJson(r:WebMotorRoute):JSONObject=JSONObject()
        .put("key",r.key).put("distance",r.distance).put("seconds",r.seconds)
        .put("points",org.json.JSONArray(r.points.map{org.json.JSONArray(it.toList())}))
        .put("roads",org.json.JSONArray(r.steps.map{it.road}))
        .put("steps",org.json.JSONArray(r.steps.map{JSONObject().put("road",it.road).put("start",it.startIndex).put("end",it.endIndex).put("instruction",it.instruction)}))
    override fun onReceive(context: Context, intent: Intent) {
        when (intent.getStringExtra("op")) {
            "shareParse" -> {
                val text=intent.getStringExtra("text")?.takeIf{it.length<=20000}?:return
                val label=intent.getStringExtra("label")?.takeIf{it.matches(Regex("[a-z0-9-]{1,32}"))}?:return
                Thread{
                    val out=runCatching{val r=AmapShare.load(text);val token=SharedRouteStore.save(context,r)
                        JSONObject().put("token",token).put("kind",r.kind).put("points",r.pts.size).put("restored",r.restored)
                            .put("endpointsOnly",r.endpointsOnly).put("distance",r.distance).put("sourceUrl",r.sourceUrl).put("verificationAvailable",r.verificationUrl.isNotBlank())}
                        .getOrElse{JSONObject().put("error",it.message)}
                    java.io.File(context.filesDir,"share-parse-$label.json").writeText(out.toString())
                }.start()
            }
            "sharePreviewCached" -> {
                val token=intent.getStringExtra("token")?.takeIf{SharedRouteStore.validToken(it)}?:return
                context.startActivity(Intent(context,RoutePlanActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    .putExtra(RoutePlanActivity.EXTRA_IMPORT_TOKEN,token).putExtra(SingleEngineDestination.EXTRA_ID,java.util.UUID.randomUUID().toString()))
            }
            "routeCheckCandidates" -> {
                val raw=intent.getStringExtra("url")?:return
                val request=runCatching{WebMotorRoutes.request(raw)}.getOrNull()?:return
                if(NavigationUi.active())return
                Thread{
                    val result=runCatching{WebMotorRoutes.load(request)}
                    android.os.Handler(android.os.Looper.getMainLooper()).post{
                        result.onSuccess{routes->
                            routeCheckCandidates=routes
                            java.io.File(context.filesDir,"routecheck-web-candidates.json").writeText(JSONObject()
                                .put("at",android.os.SystemClock.elapsedRealtime()).put("routes",org.json.JSONArray(routes.map{routeJson(it)})).toString())
                        }.onFailure{e->java.io.File(context.filesDir,"routecheck-web-candidates.json").writeText(JSONObject().put("error",e.message).toString())}
                    }
                }.start()
            }
            "routeCheckStart" -> {
                if(NavigationUi.active())return
                val k=intent.getStringExtra("index")?.toIntOrNull()?:return
                val route=routeCheckCandidates.getOrNull(k)?:return
                WebMotorGuide.start(context,route,false)
                context.startActivity(Intent(context,MotorWebNavActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
            }
            "routeCheckDump" -> WebMotorGuide.route?.let{q->java.io.File(context.filesDir,"routecheck-active-web.json").writeText(routeJson(q).toString())}
            "cruiseBegin" -> {
                if(SingleEngineSession.owns)SingleEngineSession.disarm()
                Log.i("MotoNavFusionCheck","cruise-start="+NaviBridge.startCruise(context))
            }
            "cruiseStop" -> NaviBridge.stopCruise()
            "probeNative" -> {
                java.io.File(context.filesDir,"single-engine-probe-state.json").delete()
                java.io.File(context.filesDir,"single-engine-probe").writeText((android.os.SystemClock.elapsedRealtime()+120000).toString())
            }
            "liveReplayBegin" -> LiveGpsReplay.begin(context)
            "liveReplayRenew" -> LiveGpsReplay.renew()
            "liveReplayStop" -> LiveGpsReplay.stop()
            "liveReplayFix" -> {
                val p=listOf("lat","lng","speed","bearing").map{intent.getStringExtra(it)?.toDoubleOrNull()?:return}
                LiveGpsReplay.fix(p[0],p[1],p[2],p[3])
            }
            "completeReplayBegin" -> CompleteGpsReplay.begin(context)
            "completeReplayStop" -> CompleteGpsReplay.stop()
            "singleDestinationRecreate" -> Log.i("MotoNavLifecycle","destination-recreate="+SingleEngineActivity.recreateForCheck())
            "singleDestinationPlan" -> {
                val lat=intent.getStringExtra("lat")?.toDoubleOrNull()?:return
                val lng=intent.getStringExtra("lng")?.toDoubleOrNull()?:return
                if(!CarSignalAlignment.coordinate(lat,lng))return
                val id=intent.getStringExtra("requestId")?.takeIf{it.isNotBlank()&&it.length<=128}?:return
                context.startActivity(SingleEngineDestination.intent(context,"生命周期验证目的地","",lat,lng,id)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_SINGLE_TOP))
            }
            "singleOpen" -> context.startActivity(Intent(context,SingleEngineActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
            "singleStop" -> SingleEngineSession.disarm()
            "singleSimulationSpeed" -> Log.i("MotoNavFusionCheck","simulation-speed-request="+SingleEngineSession.setSimulationSpeed(intent.getStringExtra("kmh")?.toIntOrNull()?:return))
            "singleChoose" -> SingleEngineSession.selectChoice(intent.getStringExtra("index")?.toIntOrNull()?:return)
            "singleStartChoice" -> Log.i("MotoNavFusionCheck","start-choice="+SingleEngineSession.startChoice(intent.getStringExtra("simulate")=="true",intent.getStringExtra("acceptSharedChange")=="true"))
            "singlePlan" -> {
                val v=listOf("fromLat","fromLng","toLat","toLng").map{intent.getStringExtra(it)?.toDoubleOrNull()?:return}
                val vias=(0..2).mapNotNull{k->val la=intent.getStringExtra("via${k}Lat")?.toDoubleOrNull();val lo=intent.getStringExtra("via${k}Lng")?.toDoubleOrNull();if(la!=null&&lo!=null)doubleArrayOf(la,lo) else null}
                SingleEngineSession.plan(context,v[0],v[1],v[2],v[3],vias,choose=intent.getStringExtra("choose")=="true")
            }
            "settings" -> if(NavigationUi.active()) {
                val target=if(SingleEngineSession.owns)MainActivity::class.java else if(WebMotorGuide.active)MotorWebNavActivity::class.java else NavActivity::class.java
                context.startActivity(Intent(context,target)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP)
                    .putExtra(NavigationUi.EXTRA_SETTINGS_PAGE,intent.getStringExtra("page")?.toIntOrNull()?.coerceIn(0,4) ?: 1))
            }
            "returnNavigation" -> NavigationUi.returnToNavigation(context)
            "endNavigation" -> { if(SingleEngineSession.owns)SingleEngineSession.disarm() else if(WebMotorGuide.active)WebMotorGuide.stop() else NaviBridge.exitNavi() }
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
            "reconnect" -> {
                BleLink.stopAuto()
                android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({BleLink.startAuto(context)},3000)
            }
            "connect" -> BleLink.connect(context)
            "junctionLayout" -> if(BleLink.verified)BleLink.sendEvent("#TLP,1")
            "status" -> { val state=JSONObject().put("statusRequest",intent.getStringExtra("statusRequest"))
                .put("version", APP_VERSION)
                .put("completeGpsReplay",CompleteGpsReplay.debug()).put("liveGpsReplay",LiveGpsReplay.debug())
                .put("singleEngine",SingleEngineSession.debug()).put("destinationUi",SingleEngineActivity.destinationDiagnostics())
                .put("sdkCruising",NaviBridge.cruising).put("preparedRoute",NaviBridge.preparedRouteDiagnostics())
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
                .put("webSignals",WebMotorSignals.debug()).put("carSync",CarNavSync.debug()).toString()
                java.io.File(context.filesDir,"fusion-diagnostic-status.json").writeText(state)
                Log.i("MotoNavFusionCheck",state)
            }
            "preview", "previewBicycle" -> {
                val lat=intent.getStringExtra("lat")?.toDoubleOrNull() ?: return
                val lng=intent.getStringExtra("lng")?.toDoubleOrNull() ?: return
                if(!lat.isFinite() || !lng.isFinite() || lat !in -90.0..90.0 || lng !in -180.0..180.0)return
                context.startActivity(Intent(context, RoutePlanActivity::class.java)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
                    .putExtra(RoutePlanActivity.EXTRA_FROM_HOME, true)
                    .putExtra(RoutePlanActivity.EXTRA_MODE, if(intent.getStringExtra("op")=="previewBicycle")LocalRouting.BICYCLE else LocalRouting.DRIVE)
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
