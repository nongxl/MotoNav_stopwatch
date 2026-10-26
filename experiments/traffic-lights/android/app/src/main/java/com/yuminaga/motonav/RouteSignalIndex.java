package com.yuminaga.motonav;

import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;
import java.util.Collections;

/** Projects SDK signal-bearing link endpoints onto the user's unchanged web route.
 * A marked link, matching approach geometry and named road are all mandatory.
 */
public final class RouteSignalIndex {
    public static final class Node {
        public final double lat,lng,backLat,backLng;
        public final String road;
        public Node(double lat,double lng,double backLat,double backLng,String road) {
            this.lat=lat;this.lng=lng;this.backLat=backLat;this.backLng=backLng;this.road=road;
        }
    }
    public static final class Signal {
        public final double progress,lat,lng;
        Signal(double progress,MotorTrack track){
            this.progress=progress;
            double[] point=track.at(progress);lat=point[0];lng=point[1];
        }
    }
    public final List<Signal> signals=new ArrayList<>();
    /** Use the SDK's actual lamp coordinate, with approach evidence from its route. */
    public static Node sdkPoint(MotorTrack source,String[] roads,double lat,double lng) {
        if(!CarSignalAlignment.coordinate(lat,lng))return null;
        double best=Double.POSITIVE_INFINITY,progress=-1;int segment=-1;
        List<Double> passages=new ArrayList<>();
        for(int i=1;i<source.points.length;i++) {
            if((i&1023)==0 && Thread.currentThread().isInterrupted())return null;
            double[] a=source.points[i-1],b=source.points[i];
            double scale=Math.cos(Math.toRadians(lat))*111320;
            double px=(lng-a[1])*scale,py=(lat-a[0])*111320;
            double bx=(b[1]-a[1])*scale,by=(b[0]-a[0])*111320;
            double sq=bx*bx+by*by;if(sq<.01)continue;
            double t=Math.max(0,Math.min(1,(px*bx+py*by)/sq));
            double offset=Math.hypot(px-t*bx,py-t*by);if(offset>18)continue;
            double p=source.cum[i-1]+(source.cum[i]-source.cum[i-1])*t;
            passages.add(p);
            if(offset<best){best=offset;progress=p;segment=i-1;}
        }
        if(progress<8 || roads==null || segment<0 || segment>=roads.length)return null;
        for(double p:passages)if(Math.abs(p-progress)>80)return null;
        String road=roads[segment];
        if(road==null || road.trim().isEmpty() || road.trim().equals("无名道路"))return null;
        double[] back=source.at(Math.max(0,progress-25));
        return new Node(lat,lng,back[0],back[1],road);
    }
    public RouteSignalIndex(MotorTrack track,String[] roads,List<Node> nodes) {
        this(track,roads,nodes,Collections.emptyList());
    }
    public RouteSignalIndex(MotorTrack track,String[] roads,List<Node> nodes,List<RouteRoadIndex> references) {
        for(Node n:nodes) {
            if(Thread.currentThread().isInterrupted())return;
            double back=CarSignalAlignment.metres(n.lat,n.lng,n.backLat,n.backLng);
            if(back<8 || back>100 || n.road==null || n.road.trim().isEmpty() || n.road.equals("无名道路"))continue;
            double best=Double.POSITIVE_INFINITY,progress=-1;
            List<Double> matches=new ArrayList<>();
            for(int i=1;i<track.points.length;i++) {
                if((i&4095)==0 && Thread.currentThread().isInterrupted())return;
                double[] a=track.points[i-1],b=track.points[i];
                double scale=Math.cos(Math.toRadians(n.lat))*111320;
                double px=(n.lng-a[1])*scale,py=(n.lat-a[0])*111320;
                double bx=(b[1]-a[1])*scale,by=(b[0]-a[0])*111320;
                double square=bx*bx+by*by;
                if(square<.01)continue;
                double t=Math.max(0,Math.min(1,(px*bx+py*by)/square));
                double offset=Math.hypot(px-t*bx,py-t*by);
                if(offset>18 || roads==null || i-1>=roads.length)continue;
                double p=track.cum[i-1]+(track.cum[i]-track.cum[i-1])*t;
                if(p<back)continue;
                if(!n.road.trim().equals(roads[i-1])) {
                    // A coarse web step label may span several actual roads.
                    // Resolve the incoming name through an independently verified
                    // 40m SDK/web corridor, never by stripping/similarizing names.
                    String actual=roadAt(references,Math.max(0,p-Math.min(3,back/2)));
                    if(!n.road.trim().equals(actual))continue;
                }
                double[] before=track.at(p-back);
                if(CarSignalAlignment.metres(before[0],before[1],n.backLat,n.backLng)>18)continue;
                float incoming=MotorTrack.bearing(n.backLat,n.backLng,n.lat,n.lng);
                float projected=MotorTrack.bearing(before[0],before[1],n.lat,n.lng);
                double angle=Math.abs(incoming-projected);angle=Math.min(angle,360-angle);
                if(angle>30)continue;
                matches.add(p);
                if(offset<best){best=offset;progress=p;}
            }
            if(progress<0)continue;
            boolean ambiguous=false;
            for(double p:matches)if(Math.abs(p-progress)>80){ambiguous=true;break;}
            if(!ambiguous)signals.add(new Signal(progress,track));
        }
        signals.sort(Comparator.comparingDouble(s->s.progress));
        // Repeated SDK link endpoints describe one signal, not extra intersections.
        for(int i=signals.size()-1;i>0;i--) {
            Signal a=signals.get(i-1),b=signals.get(i);
            if(b.progress-a.progress<8 && CarSignalAlignment.metres(a.lat,a.lng,b.lat,b.lng)<3)signals.remove(i);
        }
    }
    public static String roadAt(List<RouteRoadIndex> references,double progress) {
        String chosen="";
        for(RouteRoadIndex reference:references) {
            String road=reference.roadAt(progress);
            if(road.isEmpty())continue;
            if(!chosen.isEmpty() && !chosen.equals(road))return "";
            chosen=road;
        }
        return chosen;
    }
    public int next(double progress) {
        if(!Double.isFinite(progress))return -1;
        for(int i=0;i<signals.size();i++)if(signals.get(i).progress>=progress)return i;
        return -1;
    }
    public int distance(double progress) {
        int i=next(progress);
        if(i<0)return -1;
        // With no upstream junction ID, tightly spaced signals are ambiguous.
        if(i>0 && signals.get(i).progress-signals.get(i-1).progress<80)return -1;
        if(i+1<signals.size() && signals.get(i+1).progress-signals.get(i).progress<80)return -1;
        double d=signals.get(i).progress-progress;
        return d>=0 && d<=300 ? (int)Math.round(d) : -1;
    }
}
