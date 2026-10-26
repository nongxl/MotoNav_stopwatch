package com.yuminaga.motonav;

import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.ArrayList;

/** Monotonic, non-persistent upstream snapshots. Has no navigation side effects. */
public final class TrafficSignalState {
    public static final long TTL_MS=3000;
    public static final class Light {
        public final int status, seconds, dir, wait, show;
        public Light(int status,int seconds,int dir,int wait,int show) {
            this.status=status;this.seconds=seconds;this.dir=dir;this.wait=wait;this.show=show;
        }
    }
    private final Map<Integer,TrafficCountdown> clocks=new HashMap<>();
    private Light[] lights=new Light[0];
    private long seen;
    private int selected=-1;
    public void select(int dir) { selected=dir; }
    public void clear() { lights=new Light[0];clocks.clear(); }
    public boolean accept(Light[] incoming,long now) {
        HashSet<Integer> dirs=new HashSet<>();
        if(incoming==null || incoming.length>32) { clear();return false; }
        for(Light l:incoming) {
            if(l==null || l.status< -1 || l.status>255 || l.seconds<0 || l.seconds>999 ||
               l.dir<0 || l.dir>255 || l.wait<0 || l.wait>999 || l.show<0 || l.show>255 || !dirs.add(l.dir)) {
                clear();return false;
            }
        }
        // A gap clears the previous phase even if the next packet has the same colour.
        if(lights.length>0 && now-seen>=TTL_MS)clocks.clear();
        clocks.keySet().retainAll(dirs);
        for(Light l:incoming) {
            TrafficCountdown clock=clocks.get(l.dir);
            if(clock==null){clock=new TrafficCountdown();clocks.put(l.dir,clock);}
            clock.update(1,l.status,l.seconds,now);
        }
        lights=incoming.clone();seen=now;return true;
    }
    public TrafficSignalPacket snapshot(long now) {
        long age=now-seen;
        if(lights.length==0 || age<0 || age>=TTL_MS)return null;
        for(Light l:lights)if(selected<0 || l.dir==selected) {
            return new TrafficSignalPacket(l.status,l.dir,l.wait,l.show,
                clocks.get(l.dir).remaining(now),now,seen+TTL_MS);
        }
        return null; // Never silently substitute another direction.
    }
    public TrafficSignalPacket[] snapshots(long now){
        long age=now-seen;
        if(age<0||age>=TTL_MS)return new TrafficSignalPacket[0];
        ArrayList<TrafficSignalPacket> result=new ArrayList<>();
        // Stable display order. Each value comes from this one authoritative snapshot.
        for(int dir:new int[]{1,4,2,0})for(Light l:lights)if(l.dir==dir)
            result.add(new TrafficSignalPacket(l.status,l.dir,l.wait,l.show,clocks.get(l.dir).remaining(now),now,seen+TTL_MS));
        return result.toArray(new TrafficSignalPacket[0]);
    }
}
