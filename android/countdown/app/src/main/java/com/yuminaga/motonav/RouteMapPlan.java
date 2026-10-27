package com.yuminaga.motonav;

import java.util.ArrayList;
import java.util.List;

/** 沿实际路线安排相互重叠的细节底图，转弯时不再只按车头方向预取。 */
public final class RouteMapPlan {
    public static final double STEP = 600.0;
    public static final double HALF = 1200.0;
    public static final double LOOK_AHEAD = 800.0;
    public static final class Node {
        public final double x, y, distance;
        public Node(double x, double y, double distance) {
            this.x = Math.rint(x / 2) * 2; this.y = Math.rint(y / 2) * 2; this.distance = distance;
        }
    }
    private final double[] points, cumulative;
    public final List<Node> nodes = new ArrayList<>();
    public final double length;
    public RouteMapPlan(double[] xy) {
        if (xy.length < 4 || xy.length % 2 != 0) throw new IllegalArgumentException("路线至少需要两个点");
        points = xy.clone(); cumulative = new double[xy.length / 2];
        for (int i = 0; i < xy.length; i++) if (!Double.isFinite(xy[i])) throw new IllegalArgumentException("路线坐标无效");
        for (int i = 1; i < cumulative.length; i++) cumulative[i] = cumulative[i-1] + Math.hypot(xy[2*i]-xy[2*i-2], xy[2*i+1]-xy[2*i-1]);
        length = cumulative[cumulative.length-1];
        if (length / STEP > 20000) throw new IllegalArgumentException("路线过长");
        for (double s = 0; s < length; s += STEP) nodes.add(at(s));
        nodes.add(at(length));
    }
    public Node at(double distance) {
        double s = Math.max(0, Math.min(length, distance));
        int lo = 1, hi = cumulative.length-1;
        while (lo < hi) { int mid = (lo+hi)/2; if (cumulative[mid] < s) lo = mid+1; else hi = mid; }
        int i = lo;
        double span = cumulative[i]-cumulative[i-1], t = span == 0 ? 0 : (s-cumulative[i-1])/span;
        return new Node(points[2*i-2] + (points[2*i]-points[2*i-2])*t, points[2*i-1] + (points[2*i+1]-points[2*i-1])*t, s);
    }
    /** 交叉路口优先靠近上一段进度的投影，避免跳到远处重叠路段。 */
    public double progress(double x, double y, double previous) {
        double best = Double.POSITIVE_INFINITY, result = previous;
        for (int i = 1; i < cumulative.length; i++) {
            if (previous >= 0 && (cumulative[i] < previous-150 || cumulative[i-1] > previous+5000)) continue;
            double ax=points[2*i-2], ay=points[2*i-1], dx=points[2*i]-ax, dy=points[2*i+1]-ay;
            double d=dx*dx+dy*dy, t=d==0 ? 0 : Math.max(0, Math.min(1, ((x-ax)*dx+(y-ay)*dy)/d));
            double error=Math.hypot(x-ax-dx*t,y-ay-dy*t);
            double s=cumulative[i-1]+(cumulative[i]-cumulative[i-1])*t;
            double score=error + (previous < 0 ? s*1e-9 : Math.abs(s-previous)*1e-5);
            if (score < best) { best=score; result=s; }
        }
        // 明显偏离旧进度时允许重新找回路线；偏航重算会另建计划。
        if (best > 400 && previous >= 0) return progress(x,y,-1);
        return Math.max(0,result);
    }
    public int target(double progress, boolean initial) {
        return Math.max(0, Math.min(nodes.size()-1, (int)Math.floor((progress+(initial ? STEP/2 : LOOK_AHEAD))/STEP)));
    }
}
