package com.yuminaga.motonav;

/** A command sent is not an engine acknowledgement. Bounded, session-owned startup only. */
public final class ProviderStartup {
    public static final int NONE=0,WARM=1,PLAN=2,START=3,FAILED=4;
    private long begun,warmedAt,planAt,startAt;
    private int warms,plans,starts;
    private boolean active,ready,running,choosing;
    public void begin(long now){begin(now,false);}
    public void begin(long now,boolean choose){stop();active=true;begun=now;choosing=choose;}
    public void stop(){active=false;ready=false;running=false;choosing=false;warms=plans=starts=0;}
    public boolean choosing(){return active&&choosing;}
    public boolean calculatedRoute(){return active&&ready;}
    public void confirm(long now){if(active&&choosing&&ready){choosing=false;begun=now;starts=0;}}
    public void calculated(){if(active&&plans>0&&!running)ready=true;}
    public void planFailed(){ready=false;planAt=-1;}
    public boolean acceptsGuide(){return active&&starts>0;}
    public boolean running(){return running;}
    public int plans(){return plans;}
    public int starts(){return starts;}
    public int next(long now,boolean engineReady,boolean guideAcknowledged){
        if(!active)return NONE;
        if(guideAcknowledged&&acceptsGuide()){running=true;return NONE;}
        if(running)return NONE; // Stationary callback silence must never restart a running route.
        if(choosing&&ready)return NONE; // Selection time belongs to the user; do not start or replan.
        if(now<begun||now-begun>=90000){active=false;return FAILED;}
        if(warms==0){warms++;warmedAt=now;return WARM;}
        if(!engineReady){
            if(now-warmedAt<12000)return NONE;
            if(warms>=3){active=false;return FAILED;}
            warms++;warmedAt=now;return WARM;
        }
        if(ready){
            if(starts==0||now-startAt>=2500){
                if(starts<4){starts++;startAt=now;return START;}
                ready=false;planAt=-1;
            }else return NONE;
        }
        if(plans==0||planAt<0||now-planAt>=20000){
            if(plans>=3){active=false;return FAILED;}
            plans++;starts=0;planAt=now;ready=false;return PLAN;
        }
        return NONE;
    }
}
