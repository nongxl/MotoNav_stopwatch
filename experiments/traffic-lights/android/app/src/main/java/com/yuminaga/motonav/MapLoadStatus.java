package com.yuminaga.motonav;

/** Public-facing stages must describe a received frame, not merely a successful download. */
public final class MapLoadStatus {
    public static String text(boolean enabled, boolean routeOnly, boolean connected, boolean verified,
            boolean located, boolean pending, int percent, boolean fetching, boolean failed, boolean confirmed) {
        if (!enabled) return "圆屏地图已关闭";
        if (!connected) return "等待连接圆屏";
        if (!verified) return "等待圆屏配对验证";
        if (routeOnly) return "纯路线模式 · 无需加载底图";
        if (!located) return "等待定位后加载地图";
        if (pending) return percent >= 0 ? "地图传往圆屏 " + percent + "%" : "地图已发送 · 等待圆屏确认";
        if (fetching) return confirmed ? "正在准备下一段地图 · 保留当前底图" : "正在获取附近地图…";
        if (failed) return confirmed ? "新地图加载失败 · 保留旧图，等待重试" : "地图加载失败 · 检查网络和地图 Key，等待重试";
        return confirmed ? "圆屏地图已就绪" : "等待地图数据…";
    }
}
