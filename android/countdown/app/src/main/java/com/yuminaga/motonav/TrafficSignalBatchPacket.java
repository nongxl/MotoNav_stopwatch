package com.yuminaga.motonav;
import java.util.HashSet;

/** One authoritative junction snapshot, never an accumulation of different packets. */
public final class TrafficSignalBatchPacket implements TrafficSignalWrite {
    private final TrafficSignalPacket[] lights;
    private final long at;
    public TrafficSignalBatchPacket(TrafficSignalPacket[] lights,long at){
        if(lights==null||lights.length<1||lights.length>4)throw new IllegalArgumentException("light count");
        HashSet<Integer> seen=new HashSet<>();
        for(TrafficSignalPacket p:lights)
            if(p==null||!(p.dir==0||p.dir==1||p.dir==2||p.dir==4)||!seen.add(p.dir))throw new IllegalArgumentException("direction");
        this.lights=lights.clone();this.at=at;
    }
    public boolean expired(long now){
        if(now<at||now-at>=400)return true;
        for(TrafficSignalPacket p:lights)if(p.expired(now))return true;
        return false;
    }
    public String encode(long now){
        if(expired(now))return null;
        StringBuilder b=new StringBuilder("#TLS,1,").append(lights.length);
        for(TrafficSignalPacket p:lights)b.append(',').append(p.dir).append(',').append(p.status)
            .append(',').append(p.remaining(now)).append(',').append(p.wait).append(',').append(p.show)
            .append(',').append(p.expiresAt()-now);
        return b.toString();
    }
}
