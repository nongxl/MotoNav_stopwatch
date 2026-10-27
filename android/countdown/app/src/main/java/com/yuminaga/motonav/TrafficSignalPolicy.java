package com.yuminaga.motonav;

/** Display eligibility only. Does not associate two route engines or predict phases. */
public final class TrafficSignalPolicy {
    private TrafficSignalPolicy() {}
    public static final int RANGE_METRES=300;
    public static final long GUIDANCE_TTL_MS=3000;
    public static boolean isNear(boolean active,boolean hasInfo,boolean arrived,
                                 boolean unreliable,int metres,long updatedAt,long now) {
        long age=now-updatedAt;
        return active && hasInfo && !arrived && !unreliable && updatedAt>0 &&
            age>=0 && age<GUIDANCE_TTL_MS && metres>=0 && metres<=RANGE_METRES;
    }
    // CameraLightInfo direction enum differs from standard 60073 direction codes.
    // Verified against the bundled car FloatViewUtils.TYPE_DIRECTIONS drawables.
    public static int cameraDirection(int raw) {
        switch(raw) {case 1:return 1;case 2:return 4;case 3:return 2;default:return 0;}
    }
    public static int standardDirection(int raw) {
        return raw==1 || raw==2 || raw==4 ? raw : 0;
    }
    /** Requested movement at the nearby signal, not simply the next distant turn.
     * 0 means ambiguous/unsupported: hide rather than substitute a different light.
     * This does not establish that the car and SDK are referring to the same junction.
     */
    public static int desiredMovement(String turn,int turnMetres,int lightMetres) {
        if(lightMetres<0 || lightMetres>RANGE_METRES || turnMetres<0 || turn==null)return 0;
        return desiredMovementAtOffset(turn,turnMetres,lightMetres);
    }
    /** No range decision here: only compares distances in one reference. */
    public static int desiredMovementAtOffset(String turn,int turnMetres,int lightMetres) {
        if(lightMetres<0 || turnMetres<0 || turn==null)return 0;
        if("S".equals(turn))return 4;
        int movement;
        switch(turn) {
            case "L":case "SL":movement=1;break;
            case "R":case "SR":movement=2;break;
            default:return 0; // U-turn, arrival, absent guidance: no guessed shared phase.
        }
        long gap=(long)turnMetres-lightMetres;
        if(Math.abs(gap)<=45)return movement;
        // A preceding signal can be close to a turn too. Requiring the turn to
        // be over 300m away created a blind interval between nearby junctions.
        // Both route engines must still independently agree in CarNavSync.
        if(gap>80)return 4;
        return 0; // Signal may be beyond the turn or the two distances are inconclusive.
    }
    public static String directionLabel(int direction) {
        switch(direction) {
            case 1:return "← 左转灯";
            case 2:return "→ 右转灯";
            case 4:return "↑ 直行灯";
            default:return "方向未提供";
        }
    }
}
