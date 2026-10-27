package com.example.motonav;

/** Destination-ahead guidance is not a completed trip. Terminal SDK events are one-shot. */
public final class ArrivalPolicy {
    private static final int PREPARED=1, RUNNING=2, ARRIVED=3, CANCELLED=4;
    private long generation;
    private int state=CANCELLED;
    private boolean simulation;

    public synchronized long prepare() {
        generation++; state=PREPARED; simulation=false;
        return generation;
    }
    public synchronized long generation() { return generation; }
    public synchronized boolean isCurrent(long token) { return token==generation; }
    public synchronized boolean isOpen(long token) {
        return token==generation && (state==PREPARED || state==RUNNING);
    }
    public synchronized boolean begin(long token, boolean emulate) {
        if (token!=generation || state!=PREPARED) return false;
        state=RUNNING; simulation=emulate;
        return true;
    }
    public synchronized boolean acceptsGuidance(long token) { return token==generation && state==RUNNING; }
    public synchronized boolean isSimulation(long token) { return acceptsGuidance(token) && simulation; }
    public synchronized boolean arrive(long token) {
        if (!acceptsGuidance(token)) return false;
        state=ARRIVED;
        return true;
    }
    public synchronized void cancel(long token) { if (token==generation) state=CANCELLED; }

    /** Legacy firmware treats any A,* as completion, even A,800 or a predicted A,0. */
    public static String guidanceDirection(String direction) {
        return "A".equals(direction) ? "S" : direction;
    }
}
