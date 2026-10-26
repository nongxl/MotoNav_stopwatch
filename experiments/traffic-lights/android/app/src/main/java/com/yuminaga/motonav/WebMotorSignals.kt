package com.yuminaga.motonav

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.util.Log
import com.amap.api.navi.AMapNavi
import com.amap.api.navi.AMapNaviListener
import com.amap.api.navi.model.NaviLatLng
import java.lang.reflect.Proxy
import java.util.concurrent.Executors
import java.util.concurrent.Future

/** Metadata query only. Never starts SDK navigation or replaces motorcycle geometry. */
object WebMotorSignals {
    private val main=Handler(Looper.getMainLooper())
    private var navi:AMapNavi?=null
    private var listener:AMapNaviListener?=null
    private var generation=0L
    private var routeKey=""
    private var index:RouteSignalIndex?=null
    private var roadIndexes:List<RouteRoadIndex> = emptyList()
    private var cameras:List<RouteCameraIndex.Marker> = emptyList()
    private var lastCameraMap=""
    private var lastCameraLimit=""
    private val worker=Executors.newSingleThreadExecutor{r->Thread(r,"MotoNav-road-nodes").apply{priority=Thread.MIN_PRIORITY}}
    private var job:Future<*>?=null
    var status="未加载信号灯节点";private set
    fun stop(){
        generation++;job?.cancel(true);job=null;listener?.let{runCatching{navi?.removeAMapNaviListener(it)}}
        listener=null;navi=null;routeKey="";index=null;status="信号灯节点已清除"
        cameras=emptyList();roadIndexes=emptyList()
        if(lastCameraMap.isNotEmpty()||lastCameraLimit.isNotEmpty()){BleLink.send("#c");BleLink.send("#k")}
        lastCameraMap="";lastCameraLimit=""
    }
    fun load(c:Context,route:WebMotorRoute,anchors:List<DoubleArray>){
        stop();routeKey=route.key
        val token=generation
        status="正在核对沿途信号灯节点"
        val engine=runCatching{AMapNavi.getInstance(c.applicationContext)}.getOrNull() ?: return
        navi=engine
        var consumed=false
        var attempts=0;var attemptedAt=0L
        var submit:(()->Unit)?=null
        fun current()=generation==token && WebMotorGuide.active && WebMotorGuide.route?.key==route.key
        fun finish(success:Boolean){
            if(!current() || consumed)return
            consumed=true
            listener?.let{engine.removeAMapNaviListener(it)};listener=null
            if(!success){status="信号灯节点暂不可用，摩托车导航继续";return}
            val paths=engine.naviPaths?.values?.toList().orEmpty().ifEmpty{listOfNotNull(engine.naviPath)}
            job=worker.submit {
                val result=runCatching{
                val nodes=ArrayList<RouteSignalIndex.Node>()
                val coordinateLists=paths.associateWith{it.lightList.orEmpty().toList()}
                for(path in paths)for(step in path.steps.orEmpty())for(link in step.links.orEmpty()){
                    // A real point list has priority over a marked link endpoint.
                    if(coordinateLists[path].orEmpty().isNotEmpty() || !link.trafficLights)continue
                    val coords=link.coords.orEmpty();val end=coords.lastOrNull() ?: continue
                    var distance=0.0;var back=coords.lastIndex
                    while(back>0 && distance<25){
                        distance+=CarSignalAlignment.metres(coords[back].latitude,coords[back].longitude,coords[back-1].latitude,coords[back-1].longitude);back--
                    }
                    if(distance<8)continue
                    val start=coords[back];val after=coords[(back+1).coerceAtMost(coords.lastIndex)]
                    val segment=CarSignalAlignment.metres(start.latitude,start.longitude,after.latitude,after.longitude)
                    val fraction=if(distance>25 && segment>0)((distance-25)/segment).coerceIn(0.0,1.0) else 0.0
                    val backLat=start.latitude+(after.latitude-start.latitude)*fraction
                    val backLng=start.longitude+(after.longitude-start.longitude)*fraction
                    nodes.add(RouteSignalIndex.Node(end.latitude,end.longitude,backLat,backLng,link.roadName))
                }
                val roads=Array((route.points.size-1).coerceAtLeast(0)){""}
                for(step in route.steps)for(i in step.startIndex until step.endIndex)if(i in roads.indices)roads[i]=step.road.trim()
                val target=route.tracker()
                val matchedCameras=ArrayList<RouteCameraIndex.Marker>()
                val references=ArrayList<RouteRoadIndex>()
                for(path in paths){
                    val points=ArrayList<DoubleArray>();val sourceRoads=ArrayList<String>()
                    for(step in path.steps.orEmpty())for(link in step.links.orEmpty())for(c in link.coords.orEmpty()){
                        val last=points.lastOrNull()
                        if(last==null || last[0]!=c.latitude || last[1]!=c.longitude){
                            if(last!=null)sourceRoads.add(link.roadName.orEmpty().trim())
                            points.add(doubleArrayOf(c.latitude,c.longitude))
                        }
                    }
                    if(points.size<2)continue
                    val raw=path.allCameras.orEmpty().map{RouteCameraIndex.Camera(it.y,it.x,it.cameraType,it.cameraSpeed.coerceAtLeast(0))}
                    val reference=runCatching{MotorTrack(points.toTypedArray())}.getOrNull() ?: continue
                    references.add(RouteRoadIndex(reference,sourceRoads.toTypedArray(),target))
                    for(point in coordinateLists[path].orEmpty()) {
                        RouteSignalIndex.sdkPoint(reference,sourceRoads.toTypedArray(),point.latitude,point.longitude)?.let{nodes.add(it)}
                    }
                    matchedCameras.addAll(RouteCameraIndex.match(reference,sourceRoads.toTypedArray(),target,roads,raw))
                    Log.i("MotoNavWebSignals","camera metadata raw=${raw.size} accepted=${matchedCameras.size}")
                }
                val signals=RouteSignalIndex(target,roads,nodes,references)
                Log.i("MotoNavWebSignals","route=${route.key} raw=${nodes.size} accepted=${signals.signals.size}")
                Log.i("MotoNavWebSignals","SDK lamp coordinate list=${coordinateLists.values.sumOf{it.size}}; endpoint fallback only for paths without a list")
                Triple(signals,matchedCameras.sortedBy{it.progress},references.toList())
                }
                main.post {
                    if(current())result.onSuccess{matched->index=matched.first;cameras=matched.second;roadIndexes=matched.third;status="已核对 ${matched.first.signals.size} 个信号灯、${matched.second.size} 个电子眼"}
                        .onFailure{status="信号灯节点读取失败，摩托车导航继续";Log.w("MotoNavWebSignals","metadata",it)}
                }
            }
        }
        listener=Proxy.newProxyInstance(AMapNaviListener::class.java.classLoader,arrayOf(AMapNaviListener::class.java)){proxy,method,args->
            when(method.name){
                "hashCode"->System.identityHashCode(proxy)
                "equals"->proxy===args?.getOrNull(0)
                "toString"->"MotoNavWebSignalMetadata"
                "onCalculateRouteSuccess"->{main.post{finish(true)};null}
                "onInitNaviSuccess"->{main.post{if(current()&&!consumed)submit?.invoke()};null}
                "onCalculateRouteFailure","onInitNaviFailure"->{
                    val reason=args?.getOrNull(0)
                    Log.w("MotoNavWebSignals","${method.name}: $reason")
                    main.post{if(current()&&!consumed){
                        if(attempts>=3)finish(false)
                        else main.postDelayed({if(current()&&!consumed)submit?.invoke()},3000)
                    }};null
                }
                else->null
            }
        } as AMapNaviListener
        engine.addAMapNaviListener(listener)
        submit={
            val now=android.os.SystemClock.elapsedRealtime()
            if(current() && !consumed && attempts<3 && (attempts==0 || now-attemptedAt>=1500)){
                attempts++;attemptedAt=now
                runCatching{
                    engine.setIsNaviTravelView(false);engine.setMultipleRouteNaviMode(false)
                    val first=route.points.first();val last=route.points.last()
                    val accepted=engine.calculateDriveRoute(listOf(NaviLatLng(first[0],first[1])),listOf(NaviLatLng(last[0],last[1])),
                        anchors.map{NaviLatLng(it[0],it[1])},0)
                    Log.i("MotoNavWebSignals","metadata request attempt=$attempts accepted=$accepted")
                }.onFailure{Log.w("MotoNavWebSignals","metadata request",it)}
            }
        }
        submit?.invoke()
        for(delay in listOf(12000L,25000L))main.postDelayed({if(current()&&!consumed)submit?.invoke()},delay)
        main.postDelayed({if(current()&&!consumed)finish(false)},40000)
    }
    fun mapLights():List<DoubleArray> = if(WebMotorGuide.active && WebMotorGuide.route?.key==routeKey)
        index?.signals?.map{doubleArrayOf(it.lat,it.lng)}.orEmpty() else emptyList()
    fun pushCameras(){
        val active=WebMotorGuide.route?.key==routeKey && WebMotorGuide.mapGuidanceFresh()
        val ahead=if(active)cameras.filter{it.progress-WebMotorGuide.routeProgress in 0.0..1000.0}.take(10) else emptyList()
        val frame=MapFeeder.frameId()
        val points=ahead.mapNotNull{camera->MapFeeder.toFrame(camera.lat,camera.lng)?.let{p->
            ",${p[0]},${p[1]},${camera.speed},${CamIcons.iconOf(camera.type)}"
        }}
        val map=if(frame!=null && points.isNotEmpty())"#c,$frame${points.joinToString("")}" else "#c"
        if(map!=lastCameraMap){lastCameraMap=map;BleLink.send(map)}
        val range=(WebMotorGuide.speed/3.6*20).toInt().coerceIn(500,1000)
        val nearest=ahead.firstOrNull{it.speed>0 && it.progress-WebMotorGuide.routeProgress<=range}
        val limit=if(nearest!=null)"#k,${nearest.speed},${(((nearest.progress-WebMotorGuide.routeProgress).toInt()+5)/10)*10}" else "#k"
        if(limit!=lastCameraLimit){lastCameraLimit=limit;BleLink.send(limit)}
    }
    fun distance():Int {
        if(!WebMotorGuide.guidanceFresh() || WebMotorGuide.route?.key!=routeKey)return -1
        return index?.distance(WebMotorGuide.routeProgress) ?: -1
    }
    fun approachKey():String {
        val i=index ?: return ""
        if(WebMotorGuide.route?.key!=routeKey)return ""
        val next=i.next(WebMotorGuide.routeProgress)
        return if(next>=0)"$routeKey:signal:$next" else ""
    }
    fun currentRoad():String {
        if(!WebMotorGuide.guidanceFresh() || WebMotorGuide.route?.key!=routeKey)return ""
        return RouteSignalIndex.roadAt(roadIndexes,WebMotorGuide.routeProgress)
    }
    fun debug()=org.json.JSONObject().put("status",status).put("count",index?.signals?.size ?: 0).put("cameras",cameras.size).put("cameraMap",lastCameraMap).put("cameraLimit",lastCameraLimit)
        .put("metres",distance()).put("approachKey",approachKey()).put("routeKey",routeKey).put("matchedRoad",currentRoad())
}
