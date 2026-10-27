package com.yuminaga.motonav;

/** A page may release only its own route callback, never a newer page's callback. */
public final class RouteResultSlot<T> {
    private Object owner;
    private T callback;
    public synchronized void set(Object owner,T callback) {
        if(owner==null || callback==null)throw new IllegalArgumentException("owner/callback required");
        this.owner=owner;
        this.callback=callback;
    }
    public synchronized T current(){return callback;}
    public synchronized boolean owns(Object owner){return this.owner==owner && callback!=null;}
    public synchronized void clear(Object owner) {
        if(this.owner==owner)reset();
    }
    public synchronized void reset(){owner=null;callback=null;}
}
