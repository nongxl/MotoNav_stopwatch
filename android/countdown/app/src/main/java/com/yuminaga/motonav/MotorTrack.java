package com.yuminaga.motonav;

/** Geometry-only tracker for the experimental web motorcycle route. All coordinates are GCJ-02. */
public final class MotorTrack {
    public final double[][] points;
    public final double[] cum;
    public final double total;
    private double progress;
    private double lastLat, lastLng;
    private long lastAt = -1;
    private int offCount, arrivalCount;
    private long nearSince;
    private boolean arrivalArmed, arrived;

    public static final class Match {
        public final double progress, offset, lat, lng, remaining, endDistance;
        public final boolean offRoute, arrived;
        Match(double p, double d, double la, double lo, double remaining, double endDistance,
              boolean offRoute, boolean arrived) {
            this.progress=p; this.offset=d; this.lat=la; this.lng=lo;
            this.remaining=remaining; this.endDistance=endDistance;
            this.offRoute=offRoute; this.arrived=arrived;
        }
    }

    public MotorTrack(double[][] pts) {
        if (pts.length < 2) throw new IllegalArgumentException("路线坐标不足");
        points = new double[pts.length][2];
        cum = new double[pts.length];
        for (int i=0;i<pts.length;i++) {
            if (!valid(pts[i][0],pts[i][1])) throw new IllegalArgumentException("路线坐标无效");
            points[i]=pts[i].clone();
            if (i>0) cum[i]=cum[i-1]+distance(pts[i-1][0],pts[i-1][1],pts[i][0],pts[i][1]);
        }
        total=cum[cum.length-1];
        if (total < 3 || total > 5_000_000) throw new IllegalArgumentException("路线长度无效");
    }

    public static boolean valid(double lat,double lng) {
        return Double.isFinite(lat) && Double.isFinite(lng) && lat >= -90 && lat <= 90 && lng >= -180 && lng <= 180;
    }
    public static double distance(double la,double lo,double lb,double lp) {
        double x=(lp-lo)*Math.cos(Math.toRadians((la+lb)/2))*111320;
        return Math.hypot(x,(lb-la)*111320);
    }
    public static float bearing(double la,double lo,double lb,double lp) {
        return (float)((Math.toDegrees(Math.atan2((lp-lo)*Math.cos(Math.toRadians((la+lb)/2)),lb-la))+360)%360);
    }

    public double[] at(double p) {
        p=Math.max(0,Math.min(total,p));
        int i=1;
        while (i<cum.length-1 && cum[i]<p) i++;
        double t=cum[i]>cum[i-1]?(p-cum[i-1])/(cum[i]-cum[i-1]):0;
        return new double[]{points[i-1][0]+t*(points[i][0]-points[i-1][0]),
            points[i-1][1]+t*(points[i][1]-points[i-1][1])};
    }

    public double nearestProgress(double lat,double lng) { return project(lat,lng,0,total,Double.NaN)[0]; }

    /** Reject duplicate/stale timestamps and inaccurate fixes without changing progress or arrival counters. */
    public Match update(double lat,double lng,double accuracy,double speedMps,long elapsedMs) {
        if (!valid(lat,lng) || !Double.isFinite(accuracy) || accuracy<0 || accuracy>50 ||
            elapsedMs<0 || (lastAt>=0 && elapsedMs-lastAt<400)) {
            arrivalCount=0; nearSince=0; return null;
        }
        if (lastAt>=0 && elapsedMs-lastAt>4000) { arrivalCount=0; nearSince=0; }
        double lo=0, hi=total;
        double preference=Double.NaN;
        if (lastAt>=0) {
            double dt=Math.min(30,(elapsedMs-lastAt)/1000.0);
            double travel=distance(lastLat,lastLng,lat,lng);
            // One GPS teleport must not jump a loop/intersection to the end of the trip.
            double reach=Math.min(2500, Math.max(80, dt*75));
            if (travel>reach+accuracy*2) return null;
            double advance=Math.min(reach, Math.max(100,travel+accuracy*2+35));
            lo=Math.max(0,progress-80); hi=Math.min(total,progress+advance);
            preference=progress;
        } else if (distance(lat,lng,points[0][0],points[0][1])<100) {
            hi=Math.min(total,300); // A loop may end beside its start. Prefer its beginning.
        }
        double[] p=project(lat,lng,lo,hi,preference);
        lastAt=elapsedMs; lastLat=lat; lastLng=lng;
        double threshold=Math.max(55,accuracy*1.5);
        if (p[1]>threshold) { offCount++; arrivalCount=0; nearSince=0; }
        else {
            offCount=0;
            progress=Math.max(progress,p[0]);
        }
        double remain=Math.max(0,total-progress);
        double end=distance(lat,lng,points[points.length-1][0],points[points.length-1][1]);
        if (remain>Math.min(60,total*.6) && end>35 && p[1]<=threshold) arrivalArmed=true;
        boolean near=arrivalArmed && accuracy<=25 && p[1]<=30 && remain<=30 && end<=30;
        if (near) {
            if (arrivalCount++==0) nearSince=elapsedMs;
            if (arrivalCount>=3 && elapsedMs-nearSince>=2000) arrived=true;
        } else { arrivalCount=0; nearSince=0; }
        return new Match(progress,p[1],p[2],p[3],remain,end,offCount>=3,arrived);
    }

    private double[] project(double lat,double lng,double lo,double hi,double preferred) {
        double best=Double.POSITIVE_INFINITY, bestScore=best, bp=lo, blat=lat, blng=lng;
        double cos=Math.cos(Math.toRadians(lat));
        for (int i=1;i<points.length;i++) {
            if (cum[i]<lo || cum[i-1]>hi) continue;
            double len=cum[i]-cum[i-1];
            if (len<.01) continue;
            double ax=(points[i-1][1]-lng)*111320*cos, ay=(points[i-1][0]-lat)*111320;
            double dx=(points[i][1]-points[i-1][1])*111320*cos,dy=(points[i][0]-points[i-1][0])*111320;
            double t=Math.max(0,Math.min(1,-(ax*dx+ay*dy)/(dx*dx+dy*dy)));
            t=Math.max(Math.max(0,(lo-cum[i-1])/len),Math.min(Math.min(1,(hi-cum[i-1])/len),t));
            double d=Math.hypot(ax+t*dx,ay+t*dy), cp=cum[i-1]+t*len;
            double score=d+(Double.isNaN(preferred)?0:Math.abs(cp-preferred)*.015);
            if (score<bestScore) {
                best=d; bestScore=score; bp=cp;
                blat=points[i-1][0]+t*(points[i][0]-points[i-1][0]);
                blng=points[i-1][1]+t*(points[i][1]-points[i-1][1]);
            }
        }
        return new double[]{bp,best,blat,blng};
    }
}
