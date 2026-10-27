import com.yuminaga.motonav.SharedRouteComparison;

public class SharedRouteComparisonTest {
    static void check(boolean v,String message){if(!v)throw new AssertionError(message);}
    public static void main(String[] args){
        double[][] original={{30,104},{30.01,104}};
        check(!SharedRouteComparison.compare(original,original).needsConfirmation(),"Identical route");
        double[][] dense={{30,104},{30.003,104},{30.008,104},{30.01,104}};
        check(!SharedRouteComparison.compare(original,dense).needsConfirmation(),"Polyline density must not change match");
        double[][] offset={{30,104.005},{30.01,104.005}};
        check(SharedRouteComparison.compare(original,offset).needsConfirmation(),"Parallel but different road");
        double[][] extra={{30,104},{30.005,104},{30.005,104.02},{30.005,104},{30.01,104}};
        SharedRouteComparison.Result result=SharedRouteComparison.compare(original,extra);
        check(result.needsConfirmation()&&result.coverage<.95,"Reverse check must detect extra detour when every original point remains covered");
        double[][] shortLine={{30,104},{30.000001,104}};
        check(!SharedRouteComparison.compare(shortLine,shortLine).needsConfirmation(),"Very short route");
        Thread.currentThread().interrupt();boolean cancelled=false;
        try{SharedRouteComparison.compare(original,extra);}catch(java.util.concurrent.CancellationException expected){cancelled=true;}
        finally{Thread.interrupted();}
        check(cancelled,"Obsolete route checks must stop on cancellation");
        System.out.println("Symmetric share-route comparison, density, detour, short route and cancellation passed");
    }
}
