package com.example.motonav;

/** Filters the existing navigation fixes; never requests another location subscription. */
public final class RideTrackMath {
    /** Older AMap navigation engines emit Unix seconds; newer engines emit milliseconds. */
    public static long timestamp(long stamp, long received) {
        if (stamp <= 0) return received;
        return stamp < 100000000000L ? stamp * 1000L : stamp;
    }
    public static boolean valid(double lat, double lng) {
        return Double.isFinite(lat) && Double.isFinite(lng) && Math.abs(lat) <= 90 && Math.abs(lng) <= 180
            && !(lat == 0 && lng == 0);
    }
    public static double distance(double a, double b, double c, double d) {
        double y = Math.toRadians(c-a), x = Math.toRadians(d-b);
        double h = Math.sin(y/2)*Math.sin(y/2) + Math.cos(Math.toRadians(a))*Math.cos(Math.toRadians(c))*Math.sin(x/2)*Math.sin(x/2);
        return 6371000 * 2 * Math.atan2(Math.sqrt(Math.min(1,h)), Math.sqrt(Math.max(0,1-h)));
    }
    public static final class Gate {
        private long time;
        private double lat, lng;
        public double addedMeters;
        /** 0: discard, 1: continuous point, 2: first point of a new segment. */
        public int offer(double a, double b, long stamp, long received, double accuracy) {
            addedMeters = 0;
            long t = timestamp(stamp,received);
            if (!valid(a,b) || !Double.isFinite(accuracy) || accuracy < 0 || accuracy > 80
                    || Math.abs(t-received) > 60000 || t <= 0) return 0;
            int result = 2;
            if (time > 0) {
                long dt = t-time;
                if (dt < 2000) return 0;
                double m = distance(lat,lng,a,b);
                if (dt <= 120000 && m <= 100 + 90 * (dt/1000.0)) {
                    if (m < 5 && dt < 30000) return 0;
                    addedMeters = m >= 5 ? m : 0;
                    result = 1;
                }
            }
            time=t; lat=a; lng=b;
            return result;
        }
    }
}
