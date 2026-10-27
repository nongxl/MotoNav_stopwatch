package com.yuminaga.motonav;

import java.util.ArrayList;
import java.util.List;

/** Resolves fine-grained SDK road names only where both directed polylines overlap.
 * Web instructions often name a whole multi-road step after its first road.
 * This index is metadata; it never moves guidance onto the SDK route.
 */
public final class RouteRoadIndex {
    private static final double MAX_OFFSET = 5.0;
    private final MotorTrack source, web;
    private final String[] roads;
    public RouteRoadIndex(MotorTrack source, String[] roads, MotorTrack web) {
        this.source=source; this.roads=roads.clone(); this.web=web;
    }
    public String roadAt(double progress) {
        if(!Double.isFinite(progress) || progress<0 || progress>web.total) return "";
        double[] point=web.at(progress);
        double best=Double.POSITIVE_INFINITY, chosen=-1;
        int segment=-1;
        List<Double> passages=new ArrayList<>();
        for(int i=1;i<source.points.length;i++) {
            if((i&1023)==0 && Thread.currentThread().isInterrupted())return "";
            double[] a=source.points[i-1],b=source.points[i];
            double scale=Math.cos(Math.toRadians(point[0]))*111320;
            double px=(point[1]-a[1])*scale,py=(point[0]-a[0])*111320;
            double bx=(b[1]-a[1])*scale,by=(b[0]-a[0])*111320;
            double sq=bx*bx+by*by;if(sq<.01)continue;
            double t=Math.max(0,Math.min(1,(px*bx+py*by)/sq));
            double offset=Math.hypot(px-t*bx,py-t*by);
            if(offset>MAX_OFFSET)continue;
            double p=source.cum[i-1]+(source.cum[i]-source.cum[i-1])*t;
            // Verify a 40m corridor, not just an intersecting or nearby point.
            double before=Math.min(20,Math.min(progress,p));
            double after=Math.min(20,Math.min(web.total-progress,source.total-p));
            if(before+after<25)continue;
            if(!close(source.at(p-before),web.at(progress-before)) ||
               !close(source.at(p+after),web.at(progress+after)))continue;
            float sb=heading(source,p,before,after),wb=heading(web,progress,before,after);
            double angle=Math.abs(sb-wb);angle=Math.min(angle,360-angle);
            if(angle>20)continue;
            passages.add(p);
            if(offset<best){best=offset;chosen=p;segment=i-1;}
        }
        if(segment<0 || segment>=roads.length)return "";
        for(double p:passages)if(Math.abs(p-chosen)>80)return "";
        String road=roads[segment];
        return road==null || road.trim().isEmpty() || road.trim().equals("无名道路") ? "" : road.trim();
    }
    private static boolean close(double[] a,double[] b) {
        return MotorTrack.distance(a[0],a[1],b[0],b[1])<=MAX_OFFSET;
    }
    private static float heading(MotorTrack t,double p,double before,double after) {
        double[] a=t.at(p-before),b=t.at(p+after);
        return MotorTrack.bearing(a[0],a[1],b[0],b[1]);
    }
}
