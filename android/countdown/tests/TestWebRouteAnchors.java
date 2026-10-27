import com.yuminaga.motonav.WebRouteAnchors;
import com.yuminaga.motonav.RouteAnchorSelector;
import com.yuminaga.motonav.CarSignalAlignment;
import java.util.Arrays;

/** Synthetic road only: no recorded personal itinerary. */
public final class TestWebRouteAnchors {
    static void check(boolean v,String why){if(!v)throw new AssertionError(why);}
    public static void main(String[] args){
        double step=20.0/111320,longitudeStep=step/Math.cos(Math.toRadians(30));
        double[][] road=new double[301][2];
        for(int i=0;i<=300;i++){
            if(i<=100)road[i]=new double[]{30+i*step,104};
            else if(i<=200)road[i]=new double[]{30+100*step,104+(i-100)*longitudeStep};
            else road[i]=new double[]{30+(i-100)*step,104+100*longitudeStep};
        }
        int[] indices=RouteAnchorSelector.select(road,null);
        double[][] plain=WebRouteAnchors.select(road,null,null);
        check(plain.length==indices.length,"same count without maneuvers");
        for(int i=0;i<plain.length;i++)check(Arrays.equals(plain[i],road[indices[i]]),"no-turn route unchanged");
        double[][] moved=WebRouteAnchors.select(road,null,new int[]{100,200});
        check(moved.length==plain.length,"keep anchor count");
        boolean changed=false;
        for(int i=0;i<moved.length;i++){
            if(!Arrays.equals(moved[i],plain[i])){
                changed=true;
                double nearest=Math.min(CarSignalAlignment.metres(moved[i][0],moved[i][1],road[100][0],road[100][1]),
                    CarSignalAlignment.metres(moved[i][0],moved[i][1],road[200][0],road[200][1]));
                check(nearest>80,"move generated anchors clear of the intersection");
            }
        }
        check(changed,"exercise near-turn generated anchor adjustment");
        double[][] fixed=WebRouteAnchors.select(road,new int[]{100},new int[]{100,200});
        check(Arrays.stream(fixed).anyMatch(x->Arrays.equals(x,road[100])),"never move explicit user waypoint");
        check(WebRouteAnchors.select(null,null,null).length==0,"invalid input remains safe");
        System.out.println("Synthetic web-anchor turn avoidance and explicit-waypoint preservation passed");
    }
}
