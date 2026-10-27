package com.yuminaga.motonav;

import java.util.*;

/** SDK route-camera points, accepted only on the same named, directed web road. */
public final class RouteCameraIndex {
    public static final class Camera {
        public final double lat,lng;
        public final int type,speed;
        public Camera(double lat,double lng,int type,int speed){this.lat=lat;this.lng=lng;this.type=type;this.speed=speed;}
    }
    public static final class Marker {
        public final double progress,lat,lng;
        public final int type,speed;
        Marker(double p,Camera c){progress=p;lat=c.lat;lng=c.lng;type=c.type;speed=c.speed;}
    }
    public static List<Marker> match(MotorTrack source,String[] sourceRoads,MotorTrack web,String[] webRoads,List<Camera> cameras){
        List<Marker> result=new ArrayList<>();
        for(Camera camera:cameras){
            if(Thread.currentThread().isInterrupted())return result;
            if(!CarSignalAlignment.coordinate(camera.lat,camera.lng) || camera.speed<0 || camera.speed>250)continue;
            double best=Double.POSITIVE_INFINITY,progress=-1;int segment=-1;
            List<Double> passages=new ArrayList<>();
            for(int i=1;i<source.points.length;i++){
                double[] a=source.points[i-1],b=source.points[i];
                double scale=Math.cos(Math.toRadians(camera.lat))*111320;
                double px=(camera.lng-a[1])*scale,py=(camera.lat-a[0])*111320;
                double bx=(b[1]-a[1])*scale,by=(b[0]-a[0])*111320;
                double sq=bx*bx+by*by;if(sq<.01)continue;
                double f=Math.max(0,Math.min(1,(px*bx+py*by)/sq));
                double d=Math.hypot(px-f*bx,py-f*by);if(d>18)continue;
                double p=source.cum[i-1]+(source.cum[i]-source.cum[i-1])*f;
                passages.add(p);
                if(d<best){best=d;progress=p;segment=i-1;}
            }
            if(progress<25 || segment<0 || segment>=sourceRoads.length)continue;
            boolean ambiguous=false;
            for(double p:passages)if(Math.abs(p-progress)>80){ambiguous=true;break;}
            if(ambiguous)continue;
            double[] back=source.at(progress-25);
            RouteSignalIndex.Node n=new RouteSignalIndex.Node(camera.lat,camera.lng,back[0],back[1],sourceRoads[segment]);
            RouteSignalIndex matched=new RouteSignalIndex(web,webRoads,Collections.singletonList(n));
            if(matched.signals.size()==1)result.add(new Marker(matched.signals.get(0).progress,camera));
        }
        result.sort(Comparator.comparingDouble(m->m.progress));
        for(int i=result.size()-1;i>0;i--){Marker a=result.get(i-1),b=result.get(i);
            if(b.progress-a.progress<3 && b.type==a.type && b.speed==a.speed)result.remove(i);
        }
        return result;
    }
}
