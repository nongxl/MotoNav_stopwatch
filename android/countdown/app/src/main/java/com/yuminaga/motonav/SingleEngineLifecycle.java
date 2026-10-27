package com.yuminaga.motonav;

/** One native trip; only the provider's terminal event can complete it. */
public final class SingleEngineLifecycle {
    private final ArrivalPolicy trip=new ArrivalPolicy();
    private long token;
    private boolean rerouting;
    public boolean begin(boolean simulation){return trip.begin(token,simulation);}
    public boolean open(){return trip.isOpen(token);}
    public boolean running(){return trip.acceptsGuidance(token);}
    public boolean arrived(){return !trip.isOpen(token)&&arrivals>0;}
    private int arrivals;
    public void reset(){token=trip.prepare();rerouting=false;arrivals=0;}
    public void cancel(){trip.cancel(token);rerouting=false;}
    public boolean rerouting(){return running()&&rerouting;}
    public boolean event(int code){
        if(!running())return false;
        if(code==39){if(!trip.arrive(token))return false;arrivals++;rerouting=false;return true;}
        if(code==307)rerouting=true;
        else if(code==305||code==306)rerouting=false;
        else if(code==308||code==9)cancel();
        return false;
    }
}
