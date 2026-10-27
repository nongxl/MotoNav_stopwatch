package com.example.motonav;

public final class CruiseIdleTimerTest {
    private static final double LAT=31.2, LNG=121.5;
    private static int passed;
    private static void require(boolean ok,String message) { if(!ok)throw new AssertionError(message); }
    private static void fix(CruiseIdleTimer timer,long at,double speed,double northMeters,double accuracy) {
        timer.onFix(at,1_700_000_000_000L+at,speed,LAT+northMeters/111_195.0,LNG,accuracy);
    }
    private static void hold(CruiseIdleTimer timer,long first,long last) {
        for(long t=first;t<=last;t+=1000)fix(timer,t,0,0,5);
    }
    private static void test(String name,Runnable body) {body.run();passed++;System.out.println("PASS: "+name);}
    public static void main(String[] args) {
        test("exact ten-minute boundary, including a monotonic clock starting at zero",()->{
            CruiseIdleTimer t=new CruiseIdleTimer();t.start(0);hold(t,0,599_000);
            require(!t.shouldStop(599_999),"must not stop before 10 minutes");
            fix(t,600_000,0,0,5);require(t.shouldStop(600_000),"must stop at 10 minutes");
        });
        test("movement near the deadline starts a new stationary period",()->{
            CruiseIdleTimer t=new CruiseIdleTimer();t.start(0);hold(t,0,599_000);
            fix(t,600_000,20,0,5);require(!t.shouldStop(600_000),"moving at deadline");
            hold(t,601_000,1_200_000);require(!t.shouldStop(1_200_000),"only 599 stationary seconds");
            fix(t,1_201_000,0,0,5);require(t.shouldStop(1_201_000),"new full ten-minute period");
        });
        test("continuously riding never triggers expiry",()->{
            CruiseIdleTimer t=new CruiseIdleTimer();t.start(0);
            for(long at=0;at<=1_200_000;at+=1000){fix(t,at,40,at/1000.0,5);require(!t.shouldStop(at),"moving ride stopped");}
        });
        test("small GPS position drift and low speed noise do not prevent expiry",()->{
            CruiseIdleTimer t=new CruiseIdleTimer();t.start(0);
            for(long at=0;at<=600_000;at+=1000)fix(t,at,at%2000==0?0.7:1.8,at%2000==0?-4:4,8);
            require(t.shouldStop(600_000),"small jitter should remain stationary");
        });
        test("slow rolling is detected from displacement even with zero reported speed",()->{
            CruiseIdleTimer t=new CruiseIdleTimer();t.start(0);
            for(long at=0;at<=1_200_000;at+=1000){fix(t,at,0,at/1000.0*0.2,5);require(!t.shouldStop(at),"slow rolling stopped");}
        });
        test("GPS loss is not evidence of being parked",()->{
            CruiseIdleTimer t=new CruiseIdleTimer();t.start(0);hold(t,0,580_000);
            require(!t.shouldStop(600_000),"stale GPS should not stop cruise");
            hold(t,610_000,1_209_000);require(!t.shouldStop(1_209_000),"GPS gap must reset interval");
            fix(t,1_210_000,0,0,5);require(t.shouldStop(1_210_000),"fresh continuous interval should expire");
        });
        test("a short GPS gap preserves a valid stop interval",()->{
            CruiseIdleTimer t=new CruiseIdleTimer();t.start(0);hold(t,0,590_000);
            fix(t,600_000,0,0,5);require(t.shouldStop(600_000),"ten-second gap is within grace");
        });
        test("repeated cached fixes cannot keep stale GPS fresh",()->{
            CruiseIdleTimer t=new CruiseIdleTimer();t.start(0);hold(t,0,580_000);
            for(long at=581_000;at<=600_000;at+=1000)t.onFix(at,1_700_000_580_000L,0,LAT,LNG,5);
            require(!t.shouldStop(600_000),"cached timestamps must not refresh last fix");
        });
        test("invalid position and poor accuracy interrupt the stationary interval",()->{
            CruiseIdleTimer t=new CruiseIdleTimer();t.start(0);hold(t,0,599_000);
            t.onFix(600_000,1_700_000_600_000L,0,Double.NaN,LNG,5);
            require(!t.shouldStop(600_000),"invalid coordinates");
            hold(t,601_000,1_200_000);fix(t,1_201_000,0,0,100);
            require(!t.shouldStop(1_201_000),"poor accuracy");
        });
        test("SDKs omitting accuracy or fix timestamps still work with fresh fixes",()->{
            CruiseIdleTimer t=new CruiseIdleTimer();t.start(0);
            for(long at=0;at<=600_000;at+=1000)t.onFix(at,0,0,LAT,LNG,0);
            require(t.shouldStop(600_000),"fresh fixes with omitted optional metadata");
        });
        test("manual exit and restarting do not reuse the old timer",()->{
            CruiseIdleTimer t=new CruiseIdleTimer();t.start(0);hold(t,0,600_000);t.stop();
            require(!t.shouldStop(600_000),"stopped detector");t.start(601_000);
            fix(t,600_000,0,0,5);require(!t.shouldStop(1_201_000),"queued old-session sample");
            hold(t,602_000,1_201_000);require(!t.shouldStop(1_201_000),"restart must get full interval");
        });
        test("no fix after starting and clock rollback never produce expiry",()->{
            CruiseIdleTimer t=new CruiseIdleTimer();t.start(500_000);
            require(!t.shouldStop(2_000_000),"no GPS at all");fix(t,501_000,0,0,5);
            require(!t.shouldStop(400_000),"clock older than sample");
        });
        System.out.println("PASS: "+passed+" cruise inactivity scenarios");
    }
}
