package com.example.motonav;

/** 高德常规底图的白路接近纯白，浅蓝、浅灰地面不能互相充当道路亮度参照。 */
public final class AmapRasterColors {
    private AmapRasterColors() {}
    // 复用现有分类器的 +6 判定，仅把亮度 254/255 的白路归为道路。
    // 黄色主路、绿地、水面的识别仍由原分类器处理。
    public static final int ROAD_REFERENCE_LUM = 248;
}
