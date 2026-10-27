package com.yuminaga.motonav;

/** Retains junction identity while stopped, never a phase or a countdown value. */
public final class TrafficStopContext {
    public static final long FIX_TTL_MS=5000,CONTEXT_TTL_MS=180000;
    private long fixAt=-1,fixStamp=-1,confirmedAt;
    private double lat,lng,anchorLat,anchorLng;
    private boolean stopped,reliable,alignment;
    private String key="";
    private int metres=-1;
    public void clear(){key="";metres=-1;confirmedAt=0;alignment=false;}
    public void invalidateAlignment(){alignment=false;}
    public void reset(){clear();fixAt=fixStamp=-1;stopped=reliable=false;}
    public void fix(double la,double lo,double accuracy,double speed,long stamp,long wall,long elapsed){
        if(stamp<=fixStamp)return; // Cached fixes cannot renew a stop lease.
        fixStamp=stamp;
        reliable=CarSignalAlignment.coordinate(la,lo)&&Double.isFinite(accuracy)&&accuracy>=0&&accuracy<=25&&
            Double.isFinite(speed)&&speed>=0&&wall-stamp>=-2000&&wall-stamp<=5000;
        stopped=reliable&&speed<=1.4;
        if(!reliable){clear();fixAt=-1;return;}
        if(!stopped)clear();
        lat=la;lng=lo;fixAt=elapsed;
        if(!key.isEmpty()&&CarSignalAlignment.metres(anchorLat,anchorLng,lat,lng)>8)clear();
    }
    public void remember(String identity,int distance,long now){
        if(identity==null||identity.isEmpty()||distance<0||distance>300||!fresh(now))return;
        if(!key.equals(identity)){anchorLat=lat;anchorLng=lng;}
        key=identity;metres=distance;confirmedAt=now;alignment=true;
    }
    private boolean fresh(long now){return reliable&&fixAt>=0&&now>=fixAt&&now-fixAt<FIX_TTL_MS;}
    public boolean holds(String identity,long now){
        return stopped&&alignment&&!key.isEmpty()&&key.equals(identity)&&fresh(now)&&now>=confirmedAt&&now-confirmedAt<CONTEXT_TTL_MS&&
            CarSignalAlignment.metres(anchorLat,anchorLng,lat,lng)<=8;
    }
    public int distance(String identity,long now){return holds(identity,now)?metres:-1;}
    public int frameDistance(String identity,long now){
        return stopped&&!key.isEmpty()&&key.equals(identity)&&now>=confirmedAt&&now-confirmedAt<CONTEXT_TTL_MS?metres:-1;
    }
}
