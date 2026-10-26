import com.yuminaga.motonav.RouteAnchorSelector;
import com.yuminaga.motonav.CarSignalAlignment;
import java.util.Arrays;

public class TestRouteAnchors {
    static void check(boolean ok,String why){if(!ok)throw new AssertionError(why);}
    static double[][] road(int n){
        double[][] p=new double[n][2];
        for(int i=0;i<n;i++){p[i][0]=30+i*.0001;p[i][1]=104;}
        return p;
    }
    static void invariants(double[][] p,int[] result){
        check(result.length<=3,"car supports only three vias");
        int previous=0;
        for(int i:result){check(i>previous && i<p.length-1,"ordered internal route points");previous=i;}
    }
    public static void main(String[] args){
        double[][] straight=road(1001);
        int[] s=RouteAnchorSelector.select(straight,null);
        invariants(straight,s);check(s.length==3,"long straight subdivided");
        check(s[0]>200 && s[0]<300 && s[1]>450 && s[1]<550,"straight anchors spread");
        double[][] detour=road(1001);
        for(int i=50;i<150;i++)detour[i][1]+= .012*(1-Math.abs(i-100)/50.0);
        int[] d=RouteAnchorSelector.select(detour,null);invariants(detour,d);
        check(Arrays.stream(d).anyMatch(i->i>=85&&i<=115),"narrow major detour retained instead of quarter samples");
        int[] explicit=RouteAnchorSelector.select(straight,new int[]{100,500,900,700,-1,10000});
        check(Arrays.equals(explicit,new int[]{100,500,900}),"explicit priorities bounded to three");
        int[] duplicate=RouteAnchorSelector.select(straight,new int[]{1,200,201,200,999});
        invariants(straight,duplicate);check(Arrays.stream(duplicate).filter(i->i>=200&&i<=201).count()==1,"near duplicate vias removed");
        check(RouteAnchorSelector.select(road(10),null).length==0,"short route no artificial vias");
        check(RouteAnchorSelector.select(new double[][]{{30,104},{Double.NaN,104}},null).length==0,"nonfinite rejected");
        check(RouteAnchorSelector.select(new double[][]{{30,104},null},null).length==0,"malformed rejected");
        double[][] sparse={{30,104},{30.01,104.01},{30.02,104},{30.03,104.01},{30.04,104}};
        invariants(sparse,RouteAnchorSelector.select(sparse,null));
        long t=System.nanoTime();double[][] large=road(150000);
        invariants(large,RouteAnchorSelector.select(large,null));
        System.out.println("PASS: detours, ordered geometry, explicit vias, short/invalid/sparse routes, 150000 points ("+((System.nanoTime()-t)/1000000)+"ms)");
    }
}
