package com.yuminaga.motonav;

/** Recalculate remaining time AND TTL at the actual BLE write, not at enqueue time. */
public final class TrafficSignalPacket implements TrafficSignalWrite {
    public final int status,dir,wait,show;
    private final long remaining,at,expires;
    public TrafficSignalPacket(int status,int dir,int wait,int show,long remaining,long at,long expires) {
        this.status=status;this.dir=dir;this.wait=wait;this.show=show;
        this.remaining=remaining;this.at=at;this.expires=expires;
    }
    public boolean expired(long now){return now<at || now>=expires || now-at>=400;}
    public long remaining(long now){return Math.max(0,remaining-Math.max(0,now-at));}
    public int seconds(long now){return (int)((remaining(now)+999)/1000);}
    public long expiresAt(){return expires;}
    public String encode(long now){
        if(expired(now))return null;
        return "#TL,1,"+status+","+remaining(now)+","+dir+","+wait+","+show+","+(expires-now);
    }
}
