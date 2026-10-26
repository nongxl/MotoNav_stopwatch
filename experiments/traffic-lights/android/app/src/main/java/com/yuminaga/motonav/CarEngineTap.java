package com.yuminaga.motonav;

import android.app.Application;
import android.content.Intent;
import android.os.SystemClock;
import android.util.Log;
import java.lang.reflect.Method;
import java.util.HashMap;
import java.util.Map;

/** Car-process adapter. Android/reflection only: never initializes the MotoNav SDK here. */
public final class CarEngineTap {
    public static final String ACTION="com.motonav.fusion.CAR_GUIDANCE";
    private static final Map<String,Method> methods=new HashMap<>();
    private static long errorAt;
    private static Object call(Object o,String name) throws Exception {
        Method m=methods.get(name);
        if(m==null){m=o.getClass().getMethod(name);methods.put(name,m);}
        return m.invoke(o);
    }
    private static double number(Object o,String name) throws Exception {
        return ((Number)call(o,name)).doubleValue();
    }
    public static synchronized void guide(Object g) {
        if(g==null)return;
        try {
            Application app=(Application)Class.forName("android.app.ActivityThread")
                .getMethod("currentApplication").invoke(null);
            if(app==null)return;
            Intent i=new Intent(ACTION).setPackage(app.getPackageName());
            i.putExtra("at",SystemClock.elapsedRealtime());
            i.putExtra("type",(int)number(g,"getType"));
            i.putExtra("lat",number(g,"getCarLatitude"));
            i.putExtra("lng",number(g,"getCarLongitude"));
            i.putExtra("bearing",number(g,"getCarDirection"));
            i.putExtra("speed",number(g,"getCurSpeed"));
            i.putExtra("icon",(int)number(g,"getIcon"));
            i.putExtra("turnMetres",(int)number(g,"getSegRemainDis"));
            i.putExtra("remaining",(int)number(g,"getRouteRemainDis"));
            i.putExtra("endLat",number(g,"getEndPOILatitude"));
            i.putExtra("endLng",number(g,"getEndPOILongitude"));
            i.putExtra("road",String.valueOf(call(g,"getCurRoadName")));
            i.putExtra("nextRoad",String.valueOf(call(g,"getNextRoadName")));
            app.sendBroadcast(i,"com.yuminaga.motonav.permission.CAR_GUIDANCE");
        } catch(Throwable e) {
            long t=SystemClock.elapsedRealtime();
            if(t-errorAt>10000){errorAt=t;Log.w("MotoNavCarTap","guide adapter: "+e.getClass().getSimpleName());}
        }
    }
}
