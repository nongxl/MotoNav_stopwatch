package com.yuminaga.motonav;

/** Display interpolation only. Guidance, selected route and arrival stay native.
 * The provider's simulation callbacks omit car coordinates, so use its current
 * leg and remaining metres on the exact copied native geometry. No GPS injection.
 */
public final class NativeSimulationDisplay {
    private final MotorTrack geometry;
    private final int nativeMetres;
    private final int[] starts,lengths;
    private double progress,end,speed,drawn;
    private long at=-1,seen=-1;
    public static final class Pose {
        public final double lat,lng; public final float bearing;public final int speedKmh;
        Pose(double la,double lo,float heading,int kmh){lat=la;lng=lo;bearing=heading;speedKmh=kmh;}
    }
    public NativeSimulationDisplay(double[][] points,int metres,int[] segmentStarts,int[] segmentMetres){
        geometry=new MotorTrack(points);nativeMetres=Math.max(1,metres);
        starts=segmentStarts.clone();lengths=segmentMetres.clone();end=geometry.total;
    }
    public void guide(int segment,int turnRemaining,int remaining,long stamp){
        if(stamp<0||stamp<seen)return;
        double p=geometry.total*Math.max(0,Math.min(1,1.0-(double)remaining/nativeMetres));
        double limit=geometry.total;
        if(segment>=0&&segment<starts.length&&segment<lengths.length&&lengths[segment]>0&&turnRemaining>=0&&turnRemaining<=lengths[segment]+5){
            int a=starts[segment],b=segment+1<starts.length?starts[segment+1]:geometry.points.length-1;
            if(a>=0&&b>=a&&b<geometry.points.length){
                double begin=geometry.cum[a];limit=geometry.cum[b];
                p=begin+(limit-begin)*Math.max(0,Math.min(1,1.0-(double)turnRemaining/lengths[segment]));
            }
        }
        seen=stamp;end=limit;
        if(at<0){progress=p;drawn=p;at=stamp;return;}
        if(p<=progress+.05)return; // Duplicate callbacks must not double estimated speed.
        long dt=stamp-at;
        if(dt>=100){double measured=(p-progress)*3600.0/dt;speed=Math.min(250,Math.max(0,measured));}
        progress=p;at=stamp;
    }
    public Pose sample(long stamp){
        if(at<0||stamp<at||stamp-seen>3500)return null;
        double age=Math.min(1.0,(stamp-at)/1000.0);
        drawn=Math.min(geometry.total,Math.max(drawn,Math.min(end,progress+age*speed/3.6)));
        double[] p=geometry.at(drawn),next=geometry.at(Math.min(geometry.total,drawn+4));
        if(drawn>=geometry.total-1)next=p;
        return new Pose(p[0],p[1],MotorTrack.bearing(p[0],p[1],next[0],next[1]),stamp-at>2500?0:(int)Math.round(speed));
    }
}
