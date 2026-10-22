package com.example.motonav;

/** Map content policy, independent of Android so every style can be tested. */
public final class MapStylePolicy {
    public static final int ORIGINAL = 0, VICE_CITY = 1, MINIMAL = 5;
    public static final int DRAGON_RADAR = 11;
    private MapStylePolicy() {}
    public static boolean isValid(int theme) { return theme==ORIGINAL || theme==VICE_CITY || theme==MINIMAL || theme==DRAGON_RADAR; }
    public static int normalize(int theme) { return isValid(theme) ? theme : VICE_CITY; }
    public static String cacheKey(int theme, boolean buildings) {
        if(theme==ORIGINAL)return "amap-original-v1";
        if(theme==MINIMAL)return "amap-roads-only-v1";
        return buildings ? "amap-buildings-v1" : "amap-sdk-v3";
    }
    public static boolean usesBasemap(int theme) { return theme != DRAGON_RADAR; }
    public static boolean usesAmapCapture(int theme) { return usesBasemap(theme); }
}
