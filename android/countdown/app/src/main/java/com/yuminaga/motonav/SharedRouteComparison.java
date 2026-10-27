package com.yuminaga.motonav;

/** Symmetric geometry check, sampled by metres with bounded work; no navigation engine. */
public final class SharedRouteComparison {
    public static final class Result {
        public final double coverage,maximumGap,lengthDifference;
        Result(double coverage,double gap,double lengthDifference){this.coverage=coverage;maximumGap=gap;this.lengthDifference=lengthDifference;}
        public boolean needsConfirmation(){return coverage<.95||maximumGap>120||lengthDifference>.08;}
    }
    private static double distance(double[] a,double[] b){return Math.hypot((a[0]-b[0])*111320,(a[1]-b[1])*111320*Math.cos(Math.toRadians((a[0]+b[0])/2)));}
    private static double[] cumulative(double[][] p){double[] c=new double[p.length];for(int i=1;i<p.length;i++)c[i]=c[i-1]+distance(p[i-1],p[i]);return c;}
    private static double[] sample(double[][] p,double[] c,double d){int i=java.util.Arrays.binarySearch(c,d);if(i<0)i=-i-1;i=Math.max(1,Math.min(p.length-1,i));double t=Math.max(0,Math.min(1,(d-c[i-1])/Math.max(.001,c[i]-c[i-1])));return new double[]{p[i-1][0]+t*(p[i][0]-p[i-1][0]),p[i-1][1]+t*(p[i][1]-p[i-1][1])};}
    private static double nearest(double[] p,double[][] line){double k=111320*Math.cos(Math.toRadians(p[0])),best=Double.POSITIVE_INFINITY;for(int i=1;i<line.length;i++){
        double ax=(line[i-1][1]-p[1])*k,ay=(line[i-1][0]-p[0])*111320,bx=(line[i][1]-p[1])*k,by=(line[i][0]-p[0])*111320;
        double dx=bx-ax,dy=by-ay,den=dx*dx+dy*dy,t=den>0?Math.max(0,Math.min(1,(-ax*dx-ay*dy)/den)):0;
        best=Math.min(best,Math.hypot(ax+t*dx,ay+t*dy));if(best<.1)break;
    }return best;}
    private static double[] measure(double[][] a,double[] cumulative,double[][] b){int samples=Math.max(2,Math.min(512,(int)(cumulative[cumulative.length-1]/40)+2));int near=0;double gap=0;for(int i=0;i<samples;i++){
        if(Thread.currentThread().isInterrupted())throw new java.util.concurrent.CancellationException();
        double d=nearest(sample(a,cumulative,cumulative[cumulative.length-1]*i/(samples-1)),b);if(d<=45)near++;gap=Math.max(gap,d);
    }return new double[]{near/(double)samples,gap};}
    public static Result compare(double[][] a,double[][] b){
        if(a.length<2||b.length<2)throw new IllegalArgumentException("Route has no geometry");
        double[] ac=cumulative(a),bc=cumulative(b),ab=measure(a,ac,b),ba=measure(b,bc,a);
        return new Result(Math.min(ab[0],ba[0]),Math.max(ab[1],ba[1]),Math.abs(ac[ac.length-1]-bc[bc.length-1])/Math.max(1,ac[ac.length-1]));
    }
}
