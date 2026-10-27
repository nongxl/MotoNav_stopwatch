package com.yuminaga.motonav;

/** One light phase. Incoming snapshots refresh freshness without restarting each second. */
public final class TrafficCountdown {
    private long deadline, seen;
    private int status, source;
    private boolean active;
    public void update(int src, int st, int seconds, long now) {
        long candidate=now+seconds*1000L;
        boolean same=active && src==source && st==status && now-seen<3000;
        // A phase or substantial upstream correction is authoritative. Normal packet
        // jitter (often 500-1500 ms on this phone) must not restart the displayed second.
        if(!same || Math.abs(candidate-deadline)>1800) deadline=candidate;
        else if(seconds==0) deadline=Math.min(deadline,now);
        source=src;status=st;seen=now;active=true;
    }
    public long remaining(long now) { return active ? Math.max(0,deadline-now) : 0; }
    public void clear() {active=false;}
}
