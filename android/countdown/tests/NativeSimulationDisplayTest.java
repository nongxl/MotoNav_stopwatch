package com.yuminaga.motonav;
public final class NativeSimulationDisplayTest {
    static void yes(boolean ok,String text){if(!ok)throw new AssertionError(text);}
    public static void main(String[] args){
        double latStep=1.0/111320,lngStep=latStep/Math.cos(Math.toRadians(30));
        double[][] path={{30,104},{30+1000*latStep,104},{30+1000*latStep,104+1000*lngStep}};
        NativeSimulationDisplay d=new NativeSimulationDisplay(path,3000,new int[]{0,1},new int[]{1000,2000});
        d.guide(0,500,2500,0);NativeSimulationDisplay.Pose p=d.sample(0);
        yes(MotorTrack.distance(p.lat,p.lng,30+500*latStep,104)<1,"First leg midpoint");
        d.guide(1,1500,1500,1000);p=d.sample(1000);
        yes(MotorTrack.distance(p.lat,p.lng,path[1][0],104+250*lngStep)<1,"Use native current leg instead of total-distance ratio");
        yes(Math.abs(p.bearing-90)<1,"Turn follows native leg geometry");
        d.guide(1,1500,1500,1001);p=d.sample(1001);
        yes(p.speedKmh>0,"Duplicate callback does not erase measured motion");
        yes(d.sample(5000)==null,"Stale guidance does not keep simulating positions");
        d=new NativeSimulationDisplay(path,3000,new int[]{0,1},new int[]{1000,2000});
        d.guide(0,1,2001,0);d.guide(0,0,2000,100);p=d.sample(1000);
        yes(MotorTrack.distance(p.lat,p.lng,path[1][0],path[1][1])<1,"Prediction stops at leg boundary");
        System.out.println("Native simulation display geometry, corner and freshness tests passed");
    }
}
