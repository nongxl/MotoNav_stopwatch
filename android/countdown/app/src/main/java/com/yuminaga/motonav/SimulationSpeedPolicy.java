package com.yuminaga.motonav;

public final class SimulationSpeedPolicy {
    private SimulationSpeedPolicy(){}
    public static boolean allowed(int kmh){return kmh==60||kmh==100||kmh==120;}
    public static int normalize(int kmh){return allowed(kmh)?kmh:60;}
}
