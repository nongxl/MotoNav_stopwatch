package com.example.motonav;

public final class MotorTrackTest {
    private static int checks;
    private static void check(boolean yes,String why) {
        checks++; if (!yes) throw new AssertionError(why);
    }
    private static MotorTrack straight() { return new MotorTrack(new double[][]{{30,104},{30.01,104}}); }
    public static void main(String[] args) {
        MotorTrack t=straight(); long at=1000;
        check(t.update(30,104,5,10,at).remaining>1000,"full route");
        for (int i=1;i<=50;i++) {
            at+=1000;
            MotorTrack.Match m=t.update(30+i*.0002,104,5,23,at);
            check(m!=null,"valid fix");
            check(!m.arrived,"no early arrival, step="+i);
        }
        check(t.update(30.01,104,5,0,at+1000).arrived,"confirmed destination arrival after three close fixes");
        t=straight();
        check(t.update(30,104,5,0,1000)!=null,"initial");
        check(t.update(30.5,104,5,30,2000)==null,"teleport rejected");
        check(t.update(30.0001,104,100,10,2000)==null,"inaccurate rejected");
        check(t.update(30.0001,104,5,10,1000)==null,"duplicate timestamp rejected");
        check(t.update(Double.NaN,104,5,10,3000)==null,"invalid coordinate rejected");
        t=straight(); t.update(30,104,5,0,1000);
        MotorTrack.Match off=null;
        for(int i=0;i<3;i++) off=t.update(30.00005,104.001,5,0,3000+i*1000);
        check(off.offRoute,"three off-route fixes trigger reroute");
        check(!t.update(30.00005,104,5,0,7000).offRoute,"back on route clears off-route");

        // Endpoint is only 22 m from the start, but the route goes around a long block.
        t=new MotorTrack(new double[][]{{30,104},{30.01,104},{30.01,104.01},{30.0002,104.01},{30.0002,104}});
        check(!t.update(30,104,3,0,1000).arrived,"loop start is not arrival");
        for(int i=0;i<5;i++) {
            MotorTrack.Match m=t.update(30.0002,104,3,0,2000+i*1000);
            check(m.remaining>3000 && !m.arrived,"physical proximity cannot shortcut route");
        }

        // Full loop exercise and bearing/interpolation.
        t=new MotorTrack(new double[][]{{30,104},{30.002,104},{30.002,104.002},{30,104.002},{30,104}});
        at=1000;
        for(double p=0;p<t.total;p+=12) {
            double[] xy=t.at(p);
            MotorTrack.Match m=t.update(xy[0],xy[1],3,12,at);at+=1000;
            check(m!=null,"loop fix accepted");
            if(t.total-p>40) check(!m.arrived,"loop cannot finish early");
        }
        double[] end=t.at(t.total);
        boolean arrived=false;
        for(int i=0;i<4;i++) { arrived|=t.update(end[0],end[1],3,0,at).arrived;at+=1000; }
        check(arrived,"loop can finish after traversal");
        check(Math.abs(MotorTrack.bearing(30,104,30.001,104))<.01,"north bearing");
        check(Math.abs(MotorTrack.bearing(30,104,30,104.001)-90)<.01,"east bearing");
        System.out.println("MotorTrack: "+checks+" checks passed");
    }
}
