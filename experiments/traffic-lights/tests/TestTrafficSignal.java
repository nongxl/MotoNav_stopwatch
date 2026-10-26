import com.yuminaga.motonav.*;
public final class TestTrafficSignal {
    private static void check(boolean value,String why){if(!value)throw new AssertionError(why);}
    private static TrafficSignalState.Light light(int st,int s,int dir){return new TrafficSignalState.Light(st,s,dir,0,0);}
    public static void main(String[] args) {
        TrafficSignalState s=new TrafficSignalState();
        check(s.snapshot(0)==null,"no persisted signal");
        s.accept(new TrafficSignalState.Light[]{light(1,25,4)},1000);
        TrafficSignalPacket queued=s.snapshot(1100);
        check(queued.encode(1399).equals("#TL,1,1,24601,4,0,0,2601"),"queue delay must reduce both countdown and TTL");
        check(queued.encode(1500)==null,"discard queued snapshot at 400ms");
        check(s.snapshot(3999)!=null && s.snapshot(4000)==null,"strict three-second upstream deadline");
        s.accept(new TrafficSignalState.Light[]{light(1,23,4)},2200);
        check(s.snapshot(2200).seconds(2200)==24,"normal jitter must not reset the phase deadline");
        s.accept(new TrafficSignalState.Light[]{light(-1,2,4)},3000);
        check(s.snapshot(5000).seconds(5000)==0 && s.snapshot(5000).status==-1,"yellow reaches zero without inventing another colour");
        s.select(9);check(s.snapshot(3100)==null,"missing selected direction must not fall back to another lane");
        s.accept(new TrafficSignalState.Light[]{light(4,8,9),light(1,12,4)},3200);
        check(s.snapshot(3200).dir==9,"selection is independent of list order");
        s.accept(new TrafficSignalState.Light[]{light(1,10,4),light(4,7,9)},4200);
        check(s.snapshot(4200).dir==9 && s.snapshot(4200).status==4,"maintain selected phase");
        check(!s.accept(new TrafficSignalState.Light[]{light(1,2,9),light(4,2,9)},4300),"ambiguous duplicate direction must clear");
        check(s.snapshot(4300)==null,"invalid input clears previous state");
        s.select(-1);s.accept(new TrafficSignalState.Light[]{light(1,8,0)},5000);s.clear();
        check(s.snapshot(5001)==null,"explicit disconnect/disable clear cannot replay previous lights");
        s.accept(new TrafficSignalState.Light[]{light(1,8,0)},9000);
        check(s.snapshot(9000).seconds(9000)==8,"fresh update after gap starts a new phase");
        s.accept(new TrafficSignalState.Light[0],9100);check(s.snapshot(9100)==null,"empty upstream clears display");
        // Drive through an approach, then lose guidance/finish. Stale zero metres must not stick.
        check(!near(301,1000,1000),"outside approach hidden");
        check(near(300,1000,1000) && near(1,1000,1000) && near(0,1000,1000),"300m inclusive and stop-line visible");
        check(!near(-1,1000,1000),"passed light or absent reliable distance hidden");
        check(near(0,1000,3999) && !near(0,1000,4000),"stale guidance cannot pin a zero-distance light");
        check(!near(100,0,1000) && !near(100,1001,1000),"missing and future guidance timestamp hidden");
        check(!TrafficSignalPolicy.isNear(false,true,false,false,50,1000,1000),"no MotoNav navigation hides car-only lights");
        check(!TrafficSignalPolicy.isNear(true,false,false,false,50,1000,1000),"route not yet supplying guidance hidden");
        check(!TrafficSignalPolicy.isNear(true,true,true,false,50,1000,1000),"arrival immediately hides light");
        check(!TrafficSignalPolicy.isNear(true,true,false,true,50,1000,1000),"rerouting or weak GPS hides light");
        check(TrafficSignalPolicy.cameraDirection(1)==1 && TrafficSignalPolicy.cameraDirection(2)==4 &&
            TrafficSignalPolicy.cameraDirection(3)==2,"camera enum left/straight/right converts to standard movement");
        check(TrafficSignalPolicy.standardDirection(1)==1 && TrafficSignalPolicy.standardDirection(2)==2 &&
            TrafficSignalPolicy.standardDirection(4)==4,"standard enum is not camera enum");
        check(TrafficSignalPolicy.standardDirection(3)==0 && TrafficSignalPolicy.cameraDirection(4)==0 &&
            TrafficSignalPolicy.standardDirection(255)==0,"unknown combinations never invent a direction");
        s.select(4);s.accept(new TrafficSignalState.Light[]{light(1,20,TrafficSignalPolicy.cameraDirection(2)),light(4,9,1)},10000);
        check(s.snapshot(10000).dir==4 && s.snapshot(10000).status==1,"straight selection works on camera-list format");
        check(TrafficSignalPolicy.directionLabel(s.snapshot(10000).dir).contains("直行"),"human movement label");
        // Reproduce the reported screenshot: left turn and signal both 240m ahead,
        // while the car is supplying only a straight phase. Do not relabel that phase.
        int left=TrafficSignalPolicy.desiredMovement("L",240,240);
        check(left==1,"left at this signal requests the left phase");
        s.clear();s.select(left);s.accept(new TrafficSignalState.Light[]{light(4,9,4)},11000);
        check(s.snapshot(11000)==null,"left turn must hide a straight-only broadcast");
        s.accept(new TrafficSignalState.Light[]{light(1,32,1),light(4,8,4)},12000);
        check(s.snapshot(12000).dir==1 && s.snapshot(12000).seconds(12000)==32,
            "use actual left countdown, not straight countdown with a new label");
        s.accept(new TrafficSignalState.Light[]{light(4,7,4)},13000);
        check(s.snapshot(13000)==null,"disappearing left entry clears previous left phase");
        s.select(TrafficSignalPolicy.desiredMovement("R",180,180));
        s.accept(new TrafficSignalState.Light[]{light(1,20,1),light(4,6,2),light(1,42,4)},14000);
        check(s.snapshot(14000).dir==2 && s.snapshot(14000).seconds(14000)==6,
            "right turn selects right phase independently of first-item order");
        check(TrafficSignalPolicy.desiredMovement("L",1800,100)==4,
            "near straight signal before a distant left turn is not a left signal");
        check(TrafficSignalPolicy.desiredMovement("L",240,100)==4,
            "signal 140m before the turn is the preceding straight approach");
        check(TrafficSignalPolicy.desiredMovement("L",150,100)==0,
            "50m offset remains ambiguous");
        check(TrafficSignalPolicy.desiredMovement("R",60,240)==0,
            "signal beyond the upcoming turn cannot inherit the turn phase");
        check(TrafficSignalPolicy.desiredMovement("U",60,60)==0 &&
            TrafficSignalPolicy.desiredMovement("A",60,60)==0 &&
            TrafficSignalPolicy.desiredMovement("",60,60)==0,
            "unsupported movement never guesses a phase");
        check(TrafficSignalPolicy.desiredMovement("S",300,250)==4 &&
            TrafficSignalPolicy.desiredMovement("L",20,301)==0,
            "straight guidance is supported only within the display range");
        for(int turn=0;turn<=300;turn+=5)for(int signal=0;signal<=300;signal+=5) {
            int l=TrafficSignalPolicy.desiredMovement("L",turn,signal);
            int r=TrafficSignalPolicy.desiredMovement("R",turn,signal);
            long gap=(long)turn-signal;
            check(l==(Math.abs(gap)<=45?1:gap>80?4:0),"left/preceding/ambiguous boundary");
            check(r==(Math.abs(gap)<=45?2:gap>80?4:0),"right/preceding/ambiguous boundary");
        }
        System.out.println("Traffic overlay timing, TTL, direction and reconnect tests passed");
    }
    private static boolean near(int metres,long updated,long now) {
        return TrafficSignalPolicy.isNear(true,true,false,false,metres,updated,now);
    }
}
