package com.yuminaga.motonav;

/** Monotonic cruise-only inactivity detector. All calls are made on the main thread. */
final class CruiseIdleTimer {
    static final long TIMEOUT_MS = 10 * 60 * 1000L;
    static final long MAX_FIX_GAP_MS = 15_000L;
    private static final double MOVING_KMH = 3.0;
    private boolean running;
    private long startedAt;
    private long stationarySince = -1;
    private long lastFixAt = -1;
    private long lastFixStamp;
    private double anchorLat = Double.NaN, anchorLng = Double.NaN, anchorAccuracy;

    void start(long now) {
        running = true;
        startedAt = now;
        lastFixStamp = 0;
        clearInterval();
    }

    void stop() {
        running = false;
        clearInterval();
        lastFixStamp = 0;
    }

    private void clearInterval() {
        stationarySince = -1;
        lastFixAt = -1;
        anchorLat = anchorLng = Double.NaN;
    }

    void onFix(long receivedAt, long fixStamp, double kmh, double lat, double lng, double accuracy) {
        // Ignore callbacks queued before this cruise session and duplicate/cached fixes.
        if (!running || receivedAt < startedAt || receivedAt < lastFixAt) return;
        if (fixStamp > 0 && lastFixStamp > 0 && fixStamp <= lastFixStamp) return;
        if (!Double.isFinite(kmh) || kmh < 0 || !Double.isFinite(lat) || Math.abs(lat) > 90 ||
                !Double.isFinite(lng) || Math.abs(lng) > 180 || !Double.isFinite(accuracy) ||
                accuracy < 0 || accuracy > 50) {
            clearInterval();
            return;
        }
        if (lastFixAt >= 0 && receivedAt - lastFixAt > MAX_FIX_GAP_MS) clearInterval();
        lastFixAt = receivedAt;
        if (fixStamp > 0) lastFixStamp = fixStamp;
        // Some SDK fixes omit accuracy (zero). Use a conservative positional deadband.
        double error = accuracy > 0 ? accuracy : 15;
        if (kmh >= MOVING_KMH) {
            stationarySince = -1;
            anchorLat = lat;
            anchorLng = lng;
            anchorAccuracy = error;
            return;
        }
        if (Double.isNaN(anchorLat)) {
            anchorLat = lat;
            anchorLng = lng;
            anchorAccuracy = error;
        } else if (distanceMeters(anchorLat, anchorLng, lat, lng) >=
                Math.max(25, 2 * Math.max(anchorAccuracy, error))) {
            // Also recognize slow rolling when reported speed rounds down to zero.
            stationarySince = -1;
            anchorLat = lat;
            anchorLng = lng;
            anchorAccuracy = error;
            return;
        }
        if (stationarySince < 0) stationarySince = receivedAt;
    }

    boolean shouldStop(long now) {
        return running && stationarySince >= 0 && lastFixAt >= 0 &&
                now >= lastFixAt && now - lastFixAt <= MAX_FIX_GAP_MS &&
                now - stationarySince >= TIMEOUT_MS;
    }

    private static double distanceMeters(double lat1, double lng1, double lat2, double lng2) {
        double a = Math.toRadians(lat2 - lat1), b = Math.toRadians(lng2 - lng1);
        double h = Math.sin(a / 2) * Math.sin(a / 2) + Math.cos(Math.toRadians(lat1)) *
                Math.cos(Math.toRadians(lat2)) * Math.sin(b / 2) * Math.sin(b / 2);
        return 6_371_000 * 2 * Math.asin(Math.sqrt(Math.min(1, Math.max(0, h))));
    }
}
