package com.example.motonav;

public final class MapStylePolicyTest {
    private static void require(boolean ok, String message) { if (!ok) throw new AssertionError(message); }
    public static void main(String[] args) {
        for(int theme=-100;theme<100;theme++) {
            boolean allowed=theme==0 || theme==1 || theme==5 || theme==11;
            require(MapStylePolicy.isValid(theme)==allowed,"Only four style IDs: "+theme);
            require(MapStylePolicy.normalize(theme)==(allowed?theme:1),"Removed preference migration");
        }
        java.util.HashSet<String> caches=new java.util.HashSet<>();
        for(int theme:new int[]{0,1,5}) {
            require(MapStylePolicy.usesAmapCapture(theme),"SDK capture required");
            require(caches.add(MapStylePolicy.cacheKey(theme,false)),"Style cache isolation");
        }
        require(caches.add(MapStylePolicy.cacheKey(1,true)),"Building cache isolation");
        int dragon = MapStylePolicy.DRAGON_RADAR;
        require(dragon == 11 && MapStylePolicy.isValid(dragon), "Wire ID must stay compatible with firmware");
        require(!MapStylePolicy.usesBasemap(dragon), "Radar must not fetch or transmit a basemap");
        require(!MapStylePolicy.usesAmapCapture(dragon), "Radar must not start a background map surface");
        require(!MapStylePolicy.isValid(-1) && !MapStylePolicy.isValid(12), "Invalid preferences must be rejected");
        System.out.println("PASS: four style IDs, removed preference migration, isolated caches and route-only radar");
    }
}
