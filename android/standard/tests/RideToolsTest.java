package com.example.motonav;

public class RideToolsTest {
    private static int checks;
    private static void check(boolean value, String message) {
        checks++;
        if (!value) throw new AssertionError(message);
    }
    private static long base = 1792296000000L;
    private static int fix(RideTrackMath.Gate g, double lat, double lng, long seconds, double accuracy) {
        return g.offer(lat,lng,base+seconds*1000,base+seconds*1000,accuracy);
    }
    public static void main(String[] args) {
        RideTrackMath.Gate g = new RideTrackMath.Gate();
        check(fix(g,31,121,0,8)==2,"first GPS fix starts segment");
        check(fix(g,31,121,0,8)==0,"duplicate fix rejected");
        check(fix(g,31.001,121,1,8)==0,"one-second sampling rejected");
        check(fix(g,31.001,121,2,8)==1,"moving point accepted");
        check(Math.abs(g.addedMeters-111.195)<0.1,"distance is measured in metres");
        check(fix(g,31.00101,121,4,8)==0,"short stationary jitter ignored");
        check(fix(g,31.00101,121,32,8)==1 && g.addedMeters==0,"stationary heartbeat adds no fake mileage");
        check(fix(g,31.002,121,34,200)==0,"low accuracy fix ignored");
        check(fix(g,Double.NaN,121,34,8)==0,"NaN rejected");
        check(fix(g,0,0,34,8)==0,"zero coordinate rejected");
        check(fix(g,91,121,34,8)==0,"latitude range enforced");
        check(fix(g,32,121,34,8)==2 && g.addedMeters==0,"GPS teleport creates gap, no fake distance");
        check(fix(g,32.001,121,200,8)==2 && g.addedMeters==0,"long GPS outage creates new segment");
        check(fix(g,32.002,121,198,8)==0,"out-of-order timestamp rejected");
        check(g.offer(32.002,121,base,base+202000,8)==0,"stale cached fix rejected");
        check(g.offer(32.002,121,base+300000,base+202000,8)==0,"future fix rejected");
        check(g.offer(32.002,121,0,base+202000,8)==1,"missing SDK time uses captured receive time");
        RideTrackMath.Gate seconds = new RideTrackMath.Gate();
        check(seconds.offer(31,121,base/1000,base,8)==2,"old SDK Unix seconds accepted");
        check(seconds.offer(31.001,121,base/1000+2,base+2000,8)==1,"seconds fixes retain two-second sampling");
        check(seconds.offer(31.002,121,base/1000,base+200000,8)==0,"old seconds cache still rejected when stale");
        check(RideTrackMath.timestamp(base/1000,base)==base,"seconds converted for saved GPX timestamps");
        check(Math.abs(RideTrackMath.distance(31,121,31,121))<0.01,"same position distance zero");
        check(RideTrackMath.distance(0,179.999,0,-179.999)<225,"date-line distance takes short path");

        check(status(false,false,true,true,true,true,50,true,true,false).contains("关闭"),"disabled wins");
        check(status(true,false,false,true,true,true,50,true,true,false).contains("连接"),"disconnect never shows ready");
        check(status(true,false,true,false,true,true,50,true,true,false).contains("验证"),"unverified connection never shows ready");
        check(status(true,true,true,true,false,false,-1,false,true,false).contains("无需"),"radar style requires no basemap");
        check(status(true,false,true,true,false,false,-1,false,false,false).contains("定位"),"no pose waits for fix");
        check(status(true,false,true,true,true,true,43,true,false,false).contains("43%"),"staged map can transfer while buildings download");
        check(status(true,false,true,true,true,true,-1,false,false,false).contains("等待圆屏确认"),"empty transmit queue is not a receipt");
        check(!status(true,false,true,true,true,true,99,false,false,false).contains("就绪"),"99 percent not ready");
        check(status(true,false,true,true,true,false,-1,true,false,true).contains("下一段"),"existing map retained during fetch");
        check(status(true,false,true,true,true,false,-1,false,true,true).contains("保留旧图"),"fetch error preserves confirmed map");
        check(status(true,false,true,true,true,false,-1,false,false,true).contains("已就绪"),"only firmware confirmation shows ready");
        System.out.println("RideToolsTest: "+checks+" checks passed");
    }
    private static String status(boolean a,boolean b,boolean c,boolean d,boolean e,boolean f,int g,boolean h,boolean i,boolean j) {
        return MapLoadStatus.text(a,b,c,d,e,f,g,h,i,j);
    }
}
