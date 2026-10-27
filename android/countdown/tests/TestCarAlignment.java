import com.yuminaga.motonav.CarSignalAlignment;
import com.yuminaga.motonav.TrafficSignalPolicy;
public final class TestCarAlignment {
    static void check(boolean condition,String message){if(!condition)throw new AssertionError(message);}
    static boolean match(boolean sim,int type,long requestedAt,long at,long now,double endLat,double carLat){
        return CarSignalAlignment.matches(true,sim,type,requestedAt,at,now,30.52,104.02,endLat,104.02,30.53,104.01,carLat,104.01);
    }
    public static void main(String[] args){
        check(match(true,1,100,500,1000,30.52,30.53),"same simulation source");
        check(!match(true,0,100,500,1000,30.52,30.53),"real car must never feed SDK simulation");
        check(!match(false,1,100,500,1000,30.52,30.53),"sim car cannot qualify a real trip");
        check(!match(true,1,100,500,3500,30.52,30.53),"stale guide expires at 3 seconds");
        check(!match(true,1,600,500,1000,30.52,30.53),"old trip cannot qualify a new trip");
        check(!match(true,1,100,1500,1000,30.52,30.53),"future guide rejected");
        check(!match(true,1,100,500,1000,30.54,30.53),"other destination rejected");
        check(!match(true,1,100,500,1000,30.52,30.532),"same direction at different position rejected");
        check(!match(true,1,100,500,1000,30.52,Double.NaN),"missing position rejected");
        check(!CarSignalAlignment.coordinate(0,0),"empty coordinate");
        check(!CarSignalAlignment.coordinate(91,104),"invalid latitude");
        check("L".equals(CarSignalAlignment.turn(2)),"car left maneuver");
        check("R".equals(CarSignalAlignment.turn(3)),"car right maneuver");
        check(CarSignalAlignment.turn(8).isEmpty(),"unsupported u-turn hides");
        check(CarSignalAlignment.guideAgreement("甲路","乙路",2,180,2000,"甲路","乙路","L",190,2040),"real guides agree");
        check(!CarSignalAlignment.guideAgreement("甲路","乙路",9,180,2000,"甲路","乙路","L",190,2040),"straight car guide is not left");
        check(!CarSignalAlignment.guideAgreement("甲路","乙路",2,180,2000,"甲路","丙路","L",190,2040),"different next road");
        check(!CarSignalAlignment.guideAgreement("甲路","乙路",2,180,4000,"甲路","乙路","L",190,2040),"different route progress");
        check(!CarSignalAlignment.guideAgreement("甲路","乙路",2,380,2000,"甲路","乙路","L",190,2040),"different junction distance");
        check(!CarSignalAlignment.guideAgreement("无名道路","乙路",2,180,2000,"无名道路","乙路","L",190,2040),"unnamed roads not sufficient evidence");
        // Recorded left approach: UI 153m to turn / 134m to light,
        // car guidance lags at 184m. Mixing references invents a 50m gap.
        check(TrafficSignalPolicy.desiredMovement("L",184,134)==0,"reproduce false ambiguous movement");
        int translated=CarSignalAlignment.lightInCarReference(134,153,184);
        check(translated==165,"preserve 19m light-to-maneuver offset");
        check(TrafficSignalPolicy.desiredMovementAtOffset("L",184,translated)==1,"same junction remains left despite sampling skew");
        check(TrafficSignalPolicy.desiredMovement("L",285,300)==1,"SDK entry range");
        check(TrafficSignalPolicy.desiredMovementAtOffset("L",316,331)==1,"translated distance does not reapply SDK entry boundary");
        check(TrafficSignalPolicy.desiredMovement("L",316,331)==0,"normal SDK range remains 300m");
        check(CarSignalAlignment.lightInCarReference(-1,153,184)==-1,"missing light rejected");
        check(CarSignalAlignment.lightInCarReference(5,153,100)==-1,"passed light rejected");
        check("segment-distance".equals(CarSignalAlignment.guideDisagreement("甲路","乙路",2,230,2000,"甲路","乙路","L",190,2040)),"diagnostic names exact failed guard");
        check(!CarSignalAlignment.guideAgreement("甲路","乙路",2,85,7927,"甲路","乙路","L",35,7895),"advanced UI falsely disagrees by 50m");
        check(CarSignalAlignment.guideAgreement("甲路","乙路",2,85,7927,"甲路","乙路","L",53,7895),"raw guidance agrees within unchanged 35m bound");
        System.out.println("PASS: synchronized-source/mode/position/destination/epoch/TTL guards");
    }
}
