package com.example.motonav;

import com.amap.api.navi.AMapNavi;
import com.amap.api.navi.model.NaviLatLng;
import java.util.ArrayList;

/** Ordinary destination planning. Cycling uses the coordinate API, never paid POI/via routing. */
public final class LocalRouting {
    public static final int DRIVE = 0, BICYCLE = 1;
    public static int normalize(int mode) { return mode == BICYCLE ? BICYCLE : DRIVE; }
    private static boolean valid(NaviLatLng p) {
        return p != null && Double.isFinite(p.getLatitude()) && Double.isFinite(p.getLongitude())
            && Math.abs(p.getLatitude()) <= 90 && Math.abs(p.getLongitude()) <= 180;
    }
    public static boolean calculate(AMapNavi n, int mode, NaviLatLng from, NaviLatLng to,
                                    ArrayList<NaviLatLng> vias, int strategy) {
        if (n == null || !valid(to) || (from != null && !valid(from))) return false;
        if (mode == BICYCLE) {
            if (!vias.isEmpty()) return false;
            n.setIsNaviTravelView(true);
            n.setMultipleRouteNaviMode(false);
            if (from != null) return n.calculateRideRoute(from, to);
            n.startGPS();
            return n.calculateRideRoute(to);
        }
        if (mode != DRIVE) return false;
        n.setIsNaviTravelView(false);
        n.setMultipleRouteNaviMode(true);
        ArrayList<NaviLatLng> ends = new ArrayList<>(); ends.add(to);
        if (from == null) return n.calculateDriveRoute(ends, vias, strategy);
        ArrayList<NaviLatLng> starts = new ArrayList<>(); starts.add(from);
        return n.calculateDriveRoute(starts, ends, vias, strategy);
    }
    private LocalRouting() {}
}
