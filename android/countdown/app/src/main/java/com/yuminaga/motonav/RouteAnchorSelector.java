package com.yuminaga.motonav;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Select at most three ordered points ON the chosen route for the car broadcast.
 * Explicit vias take priority. Remaining slots preserve large detours before
 * subdividing straight stretches. This constrains a replan; it never imports it.
 */
public final class RouteAnchorSelector {
    private RouteAnchorSelector() {}

    public static int[] select(double[][] points, int[] explicitIndices) {
        if (points == null || points.length < 2) return new int[0];
        double[] cum = new double[points.length];
        for (int i=0;i<points.length;i++) {
            if (points[i] == null || points[i].length < 2 ||
                !CarSignalAlignment.coordinate(points[i][0], points[i][1])) return new int[0];
            if (i>0) cum[i]=cum[i-1]+CarSignalAlignment.metres(
                points[i-1][0],points[i-1][1],points[i][0],points[i][1]);
        }
        double total=cum[cum.length-1];
        if (total < 400) return new int[0];
        double spacing=Math.min(150,total/8);
        List<Integer> chosen=new ArrayList<>();
        if (explicitIndices != null) for(int index:explicitIndices) {
            if(chosen.size()==3) break;
            if(usable(index,cum,chosen,spacing)) chosen.add(index);
        }
        while(chosen.size()<3) {
            Collections.sort(chosen);
            List<Integer> ends=new ArrayList<>();
            ends.add(0); ends.addAll(chosen); ends.add(points.length-1);
            int best=-1, segmentEnd=-1; double score=-1;
            for(int k=1;k<ends.size();k++) {
                int a=ends.get(k-1),b=ends.get(k);
                double length=cum[b]-cum[a];
                if(length<2*spacing) continue;
                for(int i=a+1;i<b;i++) {
                    if(!usable(i,cum,chosen,spacing)) continue;
                    double balance=1-Math.abs(2*(cum[i]-cum[a])/length-1);
                    double candidate=deviation(points[i],points[a],points[b])+length*.025*balance;
                    if(candidate>score) {score=candidate;best=i;segmentEnd=b;}
                }
            }
            if(best<0) break;
            // Avoid placing a generated waypoint exactly on a sharp intersection.
            int shifted=best;
            while(shifted+1<segmentEnd && cum[shifted]-cum[best]<35) shifted++;
            if(cum[shifted]-cum[best]<=80 && usable(shifted,cum,chosen,spacing)) best=shifted;
            chosen.add(best);
        }
        Collections.sort(chosen);
        int[] result=new int[chosen.size()];
        for(int i=0;i<result.length;i++) result[i]=chosen.get(i);
        return result;
    }

    private static boolean usable(int i,double[] cum,List<Integer> chosen,double spacing) {
        if(i<=0 || i>=cum.length-1 || cum[i]<spacing || cum[cum.length-1]-cum[i]<spacing) return false;
        for(int other:chosen) if(Math.abs(cum[i]-cum[other])<spacing) return false;
        return true;
    }
    private static double deviation(double[] p,double[] a,double[] b) {
        double scale=Math.cos(Math.toRadians(a[0]))*111320;
        double px=(p[1]-a[1])*scale,py=(p[0]-a[0])*111320;
        double bx=(b[1]-a[1])*scale,by=(b[0]-a[0])*111320;
        double length=bx*bx+by*by;
        double t=length>0?Math.max(0,Math.min(1,(px*bx+py*by)/length)):0;
        return Math.hypot(px-t*bx,py-t*by);
    }
}
