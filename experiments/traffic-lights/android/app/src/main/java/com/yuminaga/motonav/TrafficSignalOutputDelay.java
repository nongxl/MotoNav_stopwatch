package com.yuminaga.motonav;

/** Short loss of alignment only. Never changes a signal's own expiry or phase. */
public final class TrafficSignalOutputDelay {
    private long missingSince=-1;
    public void reset(){missingSince=-1;}
    public boolean due(long now){
        if(missingSince<0 || now<missingSince)missingSince=now;
        return now-missingSince>=1000;
    }
}
