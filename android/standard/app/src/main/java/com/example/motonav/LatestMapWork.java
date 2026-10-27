package com.example.motonav;

import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.ThreadPoolExecutor;

/** 平滑计算独占低优先级线程，过时的任务取消，不能占用下载线程。 */
public final class LatestMapWork implements AutoCloseable {
    private final ThreadPoolExecutor executor = (ThreadPoolExecutor) Executors.newFixedThreadPool(1, task -> {
        Thread thread = new Thread(task, "map-smoothing");
        thread.setDaemon(true);
        thread.setPriority(Thread.MIN_PRIORITY);
        return thread;
    });
    private Future<?> active;

    public synchronized void submit(Runnable task) {
        cancel();
        active = executor.submit(task);
    }
    public synchronized void cancel() {
        if (active != null) active.cancel(true);
        active = null;
        executor.purge();
    }
    @Override public synchronized void close() {
        cancel();
        executor.shutdownNow();
    }
}
