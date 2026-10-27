package com.yuminaga.motonav;

import java.util.ArrayList;
import java.util.Comparator;

/** Display-only projection onto the actual native path; no routing or guidance engine. */
public final class NativeSignalGeometry {
    private final MotorTrack path;
    private final double[] signals;
    public static final class Approach {
        public final int index,metres;
        Approach(int i,int m){index=i;metres=m;}
    }
    public NativeSignalGeometry(double[][] points,double[][] lights){
        path=new MotorTrack(points);
        ArrayList<Double> accepted=new ArrayList<>();
        for(double[] light:lights){
            if(light.length<2||!MotorTrack.valid(light[0],light[1]))continue;
            double progress=path.nearestProgress(light[0],light[1]);double[] p=path.at(progress);
            if(MotorTrack.distance(p[0],p[1],light[0],light[1])<=55)accepted.add(progress);
        }
        accepted.sort(Comparator.naturalOrder());
        ArrayList<Double> unique=new ArrayList<>();
        for(double p:accepted)if(unique.isEmpty()||p-unique.get(unique.size()-1)>12)unique.add(p);
        signals=new double[unique.size()];for(int i=0;i<signals.length;i++)signals[i]=unique.get(i);
    }
    public Approach locate(double lat,double lng,double accuracy){
        if(!MotorTrack.valid(lat,lng)||!Double.isFinite(accuracy)||accuracy<0||accuracy>40)return new Approach(-1,-1);
        double progress=path.nearestProgress(lat,lng);double[] p=path.at(progress);
        if(MotorTrack.distance(lat,lng,p[0],p[1])>Math.max(45,accuracy*1.5))return new Approach(-1,-1);
        for(int i=0;i<signals.length;i++)if(signals[i]>=progress-12)
            return new Approach(i,(int)Math.round(Math.max(0,signals[i]-progress)));
        return new Approach(-1,-1);
    }
}
