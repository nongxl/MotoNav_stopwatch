import com.yuminaga.motonav.*;
import java.nio.file.*;
public class TestJunctionSignals {
    static void check(boolean b,String message){if(!b)throw new AssertionError(message);}
    static TrafficSignalState.Light l(int dir,int status,int seconds){return new TrafficSignalState.Light(status,seconds,dir,dir==1?2:0,0);}
    public static void main(String[] args)throws Exception{
        TrafficJunctionState s=new TrafficJunctionState();
        check(s.accept(true,new TrafficSignalState.Light[]{l(2,-1,2),l(1,1,32),l(4,4,8)},1000),"accept one junction");
        TrafficSignalPacket[] all=s.snapshots(1000);
        check(all.length==3&&all[0].dir==1&&all[1].dir==4&&all[2].dir==2,"stable left/straight/right order");
        check(all[0].seconds(1000)==32&&all[1].seconds(1000)==8&&all[2].seconds(1000)==2,"independent phase seconds");
        TrafficSignalBatchPacket batch=new TrafficSignalBatchPacket(all,1000);
        String wire=batch.encode(1200);
        check(wire.equals("#TLS,1,3,1,1,31800,2,0,2800,4,4,7800,0,0,2800,2,-1,1800,0,0,2800"),"write-time timing/TTL on every phase");
        if(args.length>0)Files.writeString(Path.of(args[0]),wire);
        check(batch.encode(1400)==null,"queue expiry at 400ms");
        check(s.accept(false,new TrafficSignalState.Light[]{l(4,1,60)},1200),"navigation single callback");
        all=s.snapshots(1200);check(all.length==3&&all[0].seconds(1200)==32&&all[1].status==4,"legacy callback cannot wipe or merge multi callback");
        all=s.snapshots(4000);check(all.length==1&&all[0].dir==4&&all[0].status==1,"expired multi callback falls back to entire fresh single snapshot");
        check(s.snapshots(4200).length==0,"all sources expire");
        check(s.accept(true,new TrafficSignalState.Light[]{l(1,1,20),l(4,4,9)},5000),"new junction snapshot");
        check(s.accept(true,new TrafficSignalState.Light[]{l(4,4,8)},6000),"authoritative subset");
        all=s.snapshots(6000);check(all.length==1&&all[0].dir==4,"missing left cleared immediately");
        s.clear();check(s.snapshots(6000).length==0,"route change/disconnect clears every source");
        boolean rejected=false;
        try{new TrafficSignalBatchPacket(new TrafficSignalPacket[]{new TrafficSignalPacket(1,4,0,0,1000,0,3000),new TrafficSignalPacket(4,4,0,0,2000,0,3000)},0);}catch(IllegalArgumentException e){rejected=true;}
        check(rejected,"duplicate directions are not two junction lights");
        s.clear();
        check(s.accept(false,new TrafficSignalState.Light[]{l(1,1,42)},10000),"fresh navigation red");
        check(s.accept(true,new TrafficSignalState.Light[0],10200),"empty camera source");
        all=s.snapshots(10200);check(all.length==1&&all[0].dir==1&&all[0].status==1,"empty camera cannot erase valid navigation red");
        check(s.accept(true,new TrafficSignalState.Light[]{l(1,1,40),l(4,1,26)},11000),"left and straight red at same junction");
        check(s.accept(false,new TrafficSignalState.Light[0],11200),"empty navigation source");
        all=s.snapshots(11200);check(all.length==2&&all[0].dir==1&&all[1].dir==4,"empty navigation preserves complete camera junction");
        check(all[0].seconds(11200)==40&&all[1].seconds(11200)==26,"left/straight retain independent actual seconds");
        check(s.snapshots(14000).length==0,"source-specific clear never extends phase TTL");
        System.out.println("PASS: complete same-snapshot junction, separate clocks, source isolation, absence/expiry, write-time batch encoding");
    }
}
