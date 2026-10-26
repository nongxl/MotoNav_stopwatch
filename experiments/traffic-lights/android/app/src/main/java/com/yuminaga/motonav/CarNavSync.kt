package com.yuminaga.motonav

import android.content.*
import android.os.*
import android.util.Log
import com.amap.api.navi.AMapNavi
import org.json.JSONObject

/** Real-route handoff. Never mixes a real-position car light into SDK simulation. */
object CarNavSync {
    private val handler=Handler(Looper.getMainLooper())
    private var context:Context?=null
    private var registered=false
    var requested=false; private set
    var simulation=false; private set
    private var epoch=-1L
    private var requestedAt=0L
    private var expectedLat=Double.NaN
    private var expectedLng=Double.NaN
    private var guideAt=0L
    private var carType=-1
    private var carLat=Double.NaN
    private var carLng=Double.NaN
    private var endLat=Double.NaN
    private var endLng=Double.NaN
    private var carIcon=0
    private var carTurnMetres=-1
    private var guides=0
    private var status="未启动联动"
    private var logAt=0L
    private var carRoad=""
    private var carNextRoad=""
    private var carRemaining=-1
    private var calculating=false
    private var startSent=false
    private var webRouteKey=""
    private var routeAnchors:List<DoubleArray> = emptyList()
    private var routeSource="sdk"
    private fun current()=requested && if(webRouteKey.isNotEmpty())
        WebMotorGuide.active && !WebMotorGuide.simulation && WebMotorGuide.route?.key==webRouteKey
        else epoch==NaviBridge.navigationEpoch()
    private fun now()=SystemClock.elapsedRealtime()

    fun start(c:Context) {
        if(registered)return
        context=c.applicationContext
        val r=object:BroadcastReceiver(){override fun onReceive(c:Context,i:Intent){receive(i)}}
        if(Build.VERSION.SDK_INT>=33)context!!.registerReceiver(r,IntentFilter(CarEngineTap.ACTION),"com.yuminaga.motonav.permission.CAR_GUIDANCE",handler,Context.RECEIVER_NOT_EXPORTED)
        else context!!.registerReceiver(r,IntentFilter(CarEngineTap.ACTION),"com.yuminaga.motonav.permission.CAR_GUIDANCE",handler)
        registered=true
    }
    private fun send(i:Intent) {
        val c=context?:return
        i.action="AUTONAVI_STANDARD_BROADCAST_RECV"
        i.setPackage(c.packageName)
        i.putExtra("SOURCE_APP","MotoNav fusion")
        c.sendBroadcast(i)
    }
    private fun muteProvider() {
        // Confirmed in this provider's NaviMuteAction: 10047, int 1 = mute.
        // The navigation and cruise settings are distinct; neither changes system audio.
        send(Intent().putExtra("KEY_TYPE",10047).putExtra("EXTRA_MUTE",1))
        send(Intent().putExtra("KEY_TYPE",10047).putExtra("EXTRA_CASUAL_MUTE",1))
        Log.i("MotoNavCarSync","provider mute requested: navigation=1 cruise=1")
    }
    /** Uses the current selected SDK path, not a stale destination preference. */
    fun wants(c:Context)=TrafficLightOverlay.enabled && c.packageName=="com.autonavi.amapauto" &&
        NaviBridge.directRouteMode!=LocalRouting.BICYCLE && ShareGuide.route?.isTravel!=true
    fun request(c:Context,n:AMapNavi,emulate:Boolean):Boolean {
        stop()
        start(c)
        if(!wants(c))return false
        FusionCarUi.prepare(c)
        muteProvider()
        if(emulate){
            send(Intent().putExtra("KEY_TYPE",10010))
            status="SDK 模拟位置与真实灯源不同，模拟时不显示读秒"
            return false
        }
        val path=try{n.naviPath?.coordList?.toList()}catch(_:Exception){null}?:return false
        if(path.size<2)return false
        requestPath(c,path.map{doubleArrayOf(it.latitude,it.longitude)},intArrayOf())
        return false // SDK retains ordinary GPS navigation; no external position substitution.
    }
    /** Constrain the background car replan; MotoNav keeps the selected motorcycle geometry. */
    fun requestWeb(c:Context,route:WebMotorRoute,emulate:Boolean) {
        stop();start(c)
        routeSource="web-motorcycle"
        if(!TrafficLightOverlay.enabled || c.packageName!="com.autonavi.amapauto"){status="后台路线同步已关闭";return}
        if(emulate){status="网页模拟未同步真实位置灯源";return}
        webRouteKey=route.key
        val track=route.tracker()
        val explicit=route.request.vias.mapNotNull { via ->
            val progress=track.nearestProgress(via.lat,via.lng)
            val point=track.at(progress)
            if(CarSignalAlignment.metres(via.lat,via.lng,point[0],point[1])>100)null
            else track.cum.indexOfFirst{it>=progress}.takeIf{it>=0}
        }.toIntArray()
        FusionCarUi.prepare(c);muteProvider()
        requestPath(c,route.points,explicit)
    }
    private fun requestPath(c:Context,path:List<DoubleArray>,explicit:IntArray) {
        if(path.size<2)return
        val first=path.first();val last=path.last()
        if(!CarSignalAlignment.coordinate(first[0],first[1])||!CarSignalAlignment.coordinate(last[0],last[1]))return
        requested=true;simulation=false;epoch=NaviBridge.navigationEpoch();requestedAt=now()
        expectedLat=last[0];expectedLng=last[1]
        guideAt=0;guides=0
        status="正在准备路口信息"
        calculating=false;startSent=false
        send(Intent().putExtra("KEY_TYPE",10010))
        val i=Intent().putExtra("KEY_TYPE",10007)
            .putExtra("EXTRA_SLAT",first[0]).putExtra("EXTRA_SLON",first[1])
            .putExtra("EXTRA_SNAME","MotoNav 起点")
            .putExtra("EXTRA_DLAT",last[0]).putExtra("EXTRA_DLON",last[1])
            .putExtra("EXTRA_DNAME","MotoNav 同步终点").putExtra("EXTRA_DEV",0)
            .putExtra("EXTRA_M",-1)
        // Preserve the existing SDK selection; web routes prioritize their detours/vias.
        routeAnchors=if(webRouteKey.isNotEmpty())
            RouteAnchorSelector.select(path.toTypedArray(),explicit).map{path[it]}
        else {
            val cumulative=DoubleArray(path.size)
            for(k in 1 until path.size)cumulative[k]=cumulative[k-1]+CarSignalAlignment.metres(path[k-1][0],path[k-1][1],path[k][0],path[k][1])
            if(cumulative.last()>1000)(1..3).map{part->path[cumulative.indexOfFirst{it>=cumulative.last()*part/4.0}.coerceAtLeast(0)]}
            else emptyList()
        }
        val anchors=listOf("F","S","T")
        routeAnchors.forEachIndexed{a,point->
            i.putExtra("EXTRA_${anchors[a]}MIDLAT",point[0])
                .putExtra("EXTRA_${anchors[a]}MIDLON",point[1])
                .putExtra("EXTRA_${anchors[a]}MIDNAME","MotoNav 路线点${a+1}")
        }
        val token=epoch;val routeKey=webRouteKey
        fun valid()=current()&&epoch==token&&webRouteKey==routeKey
        fun submit(attempt:Int){
            if(!valid())return
            requestedAt=now();guideAt=0;calculating=false;startSent=false
            muteProvider();send(i)
            Log.i("MotoNavCarSync","route-request epoch=$token source=$routeSource points=${path.size} anchors=${routeAnchors.size} attempt=$attempt")
            if(attempt<3)handler.postDelayed({
                val destination=CarSignalAlignment.metres(expectedLat,expectedLng,endLat,endLng)<=100
                if(valid() && !(startSent && carType==0 && guideAt>=requestedAt && now()-guideAt<3000 && destination)){
                    FusionCarUi.prepare(c);muteProvider();send(Intent().putExtra("KEY_TYPE",10010))
                    handler.postDelayed({if(valid())submit(attempt+1)},600)
                }
            },16000)
        }
        handler.postDelayed({submit(1)},600)
    }

    fun standard(i:Intent){
        if(!current() || i.getIntExtra("KEY_TYPE",-1)!=10019)return
        when(i.getIntExtra("EXTRA_STATE",-1)){
            5 -> {calculating=true;status="正在准备路口信息"}
            6 -> if(calculating&&!startSent){
                startSent=true;muteProvider();send(Intent().putExtra("KEY_TYPE",10009))
                status="路口信息正在同步"
                Log.i("MotoNavCarSync","route-ready start-navigation epoch=$epoch")
            }
            7 -> {calculating=false;status="路口信息暂不可用，继续正常导航"}
            9,12 -> guideAt=0
        }
    }
    fun stop(expectedEpoch:Long?=null) {
        if(expectedEpoch!=null&&(webRouteKey.isNotEmpty()||expectedEpoch!=epoch))return
        val owned=requested
        requested=false;guideAt=0;status="联动已停止"
        webRouteKey="";routeAnchors=emptyList();routeSource="sdk"
        if(owned)send(Intent().putExtra("KEY_TYPE",10010))
    }
    private fun receive(i:Intent) {
        if(!current())return
        val stamp=i.getLongExtra("at",0)
        if(stamp<requestedAt || now()-stamp !in 0..2500)return
        guides++
        guideAt=stamp;carType=i.getIntExtra("type",-1)
        carLat=i.getDoubleExtra("lat",Double.NaN);carLng=i.getDoubleExtra("lng",Double.NaN)
        endLat=i.getDoubleExtra("endLat",Double.NaN);endLng=i.getDoubleExtra("endLng",Double.NaN)
        carIcon=i.getIntExtra("icon",0);carTurnMetres=i.getIntExtra("turnMetres",-1)
        carRoad=i.getStringExtra("road").orEmpty();carNextRoad=i.getStringExtra("nextRoad").orEmpty()
        carRemaining=i.getIntExtra("remaining",-1)
        val destination=CarSignalAlignment.metres(expectedLat,expectedLng,endLat,endLng)<=100
        val correctMode=carType==(if(simulation)1 else 0)
        status=when{
            !destination->"正在核对目的地"
            !correctMode->"等待路口信息服务就绪"
            else->"正在核对前方路口"
        }
        if(now()-logAt>=1500){logAt=now();Log.i("MotoNavCarSync",debug().toString())}
    }
    /** Route agreement alone is NOT evidence of a signal location on a web route. */
    fun webRouteAgreement():String {
        if(webRouteKey.isEmpty() || !current())return "inactive"
        if(guideAt<requestedAt || now()-guideAt !in 0..2999)return "stale-guide"
        if(carType!=0)return "mode"
        if(CarSignalAlignment.metres(expectedLat,expectedLng,endLat,endLng)>100)return "destination"
        if(!WebMotorGuide.guidanceFresh())return "web-position"
        return CarSignalAlignment.guideDisagreement(carRoad,carNextRoad,carIcon,carTurnMetres,carRemaining,
            WebMotorSignals.currentRoad().ifEmpty{WebMotorGuide.currentRoad},WebMotorGuide.nextRoad,WebMotorGuide.direction,WebMotorGuide.turnMeters,WebMotorGuide.remaining)
    }
    fun webAnchors(key:String)=if(webRouteKey==key)routeAnchors.map{it.clone()} else emptyList()
    fun webSummary():String {
        if(webRouteKey.isEmpty() || !requested)return status
        if(guideAt==0L)return "后台路线同步中 · ${routeAnchors.size} 个关键点"
        return if(webRouteAgreement().isEmpty())"后台路线当前路段匹配 · ${routeAnchors.size} 个关键点"
            else "后台路线核对中 · 摩托车导航继续"
    }
    fun aligned():Boolean {
        // Web signals are SDK-marked road nodes geometrically matched to the
        // unchanged motorcycle route; a step count/turn is never sufficient.
        if(WebMotorGuide.active)return webRouteKey.isNotEmpty() &&
            webRouteAgreement().isEmpty() && WebMotorSignals.distance()>=0
        if(webRouteKey.isNotEmpty())return false
        val position=NaviBridge.trafficLightPosition()
        val direct=CarSignalAlignment.matches(requested,simulation,carType,requestedAt,guideAt,now(),
            expectedLat,expectedLng,endLat,endLng,position[0],position[1],carLat,carLng)
        if(direct)return true
        // Native channel observed 0,0 coordinates. Real navigation only: require
        // matching road pair, maneuver and both segment/whole-route distances.
        return requested&&!simulation&&carType==0&&guideAt>=requestedAt&&now()-guideAt in 0..2999&&
            CarSignalAlignment.metres(expectedLat,expectedLng,endLat,endLng)<=100&&
            !CarSignalAlignment.coordinate(carLat,carLng)&&
            CarSignalAlignment.guideAgreement(carRoad,carNextRoad,carIcon,carTurnMetres,carRemaining,
                NaviBridge.uiCurRoad,NaviBridge.uiNextRoad,NaviBridge.uiDir,NaviBridge.trafficLightGuidanceTurnMetres(),NaviBridge.uiRemainM)
    }
    fun allows(wanted:Int,lightMetres:Int):Boolean {
        if(!aligned())return false
        val ownTurn=if(WebMotorGuide.active)WebMotorGuide.turnMeters else NaviBridge.uiDist
        val carLight=CarSignalAlignment.lightInCarReference(lightMetres,ownTurn,carTurnMetres)
        // Eligibility was established in the SDK's 300m range. Translation can
        // exceed 300m by the permitted sampling skew; only the offset is needed.
        val carWanted=TrafficSignalPolicy.desiredMovementAtOffset(CarSignalAlignment.turn(carIcon),carTurnMetres,carLight)
        // Both engines must approach the same maneuver, at a similar distance.
        if(wanted!=carWanted)return false
        return wanted==4 || kotlin.math.abs(carTurnMetres-(if(WebMotorGuide.active)WebMotorGuide.turnMeters else NaviBridge.trafficLightGuidanceTurnMetres()))<=35
    }
    fun summary():String=if(requested&&now()-requestedAt>30000&&guideAt==0L)"路口信息服务暂未就绪，可在高级诊断完成首次初始化" else status
    fun debug()=JSONObject().put("source",routeSource).put("webRouteKey",webRouteKey)
        .put("anchors",org.json.JSONArray(routeAnchors.map{org.json.JSONArray(it.toList())}))
        .put("webAgreement",if(webRouteKey.isNotEmpty())webRouteAgreement() else "")
        .put("webRoad",WebMotorGuide.currentRoad).put("webNextRoad",WebMotorGuide.nextRoad)
        .put("webTurn",WebMotorGuide.direction).put("webTurnMetres",WebMotorGuide.turnMeters).put("webRemaining",WebMotorGuide.remaining)
        .put("requested",requested).put("simulation",simulation).put("epoch",epoch)
        .put("guides",guides).put("carType",carType).put("carIcon",carIcon)
        .put("carCoordinatePresent",CarSignalAlignment.coordinate(carLat,carLng))
        .put("carTurnMetres",carTurnMetres).put("guideAgeMs",if(guideAt>0)now()-guideAt else -1)
        .put("carRoad",carRoad).put("carNextRoad",carNextRoad).put("carRemaining",carRemaining)
        .put("sdkRoad",NaviBridge.uiCurRoad).put("sdkNextRoad",NaviBridge.uiNextRoad)
        .put("sdkTurn",NaviBridge.uiDir).put("sdkTurnMetres",NaviBridge.uiDist).put("sdkRemaining",NaviBridge.uiRemainM)
        .put("sdkRawTurnMetres",NaviBridge.trafficLightGuidanceTurnMetres())
        .put("guideDisagreement",CarSignalAlignment.guideDisagreement(carRoad,carNextRoad,carIcon,carTurnMetres,carRemaining,
            NaviBridge.uiCurRoad,NaviBridge.uiNextRoad,NaviBridge.uiDir,NaviBridge.trafficLightGuidanceTurnMetres(),NaviBridge.uiRemainM))
        .put("destinationMatched",CarSignalAlignment.metres(expectedLat,expectedLng,endLat,endLng)<=100)
        .put("positionGap",CarSignalAlignment.metres(NaviBridge.trafficLightPosition()[0],NaviBridge.trafficLightPosition()[1],carLat,carLng).takeIf{it.isFinite()}?:-1)
        .put("aligned",aligned()).put("status",summary())
}
