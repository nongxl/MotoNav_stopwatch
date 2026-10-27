package com.yuminaga.motonav;
/** All persisted source IDs migrate to the sole supported provider. */
public final class MapProviderPolicy {
    private MapProviderPolicy() {}
    public static int normalize(int source) { return 3; }
    public static boolean usesAmap(int source,int theme) { return MapStylePolicy.usesBasemap(theme); }
}
