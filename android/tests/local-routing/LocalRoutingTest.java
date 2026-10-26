package com.example.motonav;
import com.amap.api.navi.AMapNavi;
import com.amap.api.navi.model.NaviLatLng;
import java.util.ArrayList;
public final class LocalRoutingTest {
    private static int checks;
    private static void check(boolean b,String s){checks++;if(!b)throw new AssertionError(s);}
    public static void main(String[] args){
        NaviLatLng from=new NaviLatLng(30.533852,104.007799),to=new NaviLatLng(30.54,104.01);
        ArrayList<NaviLatLng> via=new ArrayList<>(); AMapNavi n=new AMapNavi();
        check(LocalRouting.calculate(n,1,from,to,via,10),"bicycle starts");
        check(n.travel&&!n.multiple&&n.rides==1&&n.drives==0,"bicycle engine exclusively");
        check(n.from==from&&n.to==to,"origin and destination retained");
        n=new AMapNavi(); n.result=false;
        check(!LocalRouting.calculate(n,1,from,to,via,10),"permission/rejection propagated");
        check(n.drives==0,"no driving fallback on bicycle failure");
        n=new AMapNavi();check(LocalRouting.calculate(n,1,null,to,via,10)&&n.gps&&n.rides==1,"missing location starts SDK GPS");
        via.add(from);n=new AMapNavi();
        check(!LocalRouting.calculate(n,1,from,to,via,10)&&n.rides==0&&n.drives==0,"unsupported bicycle via never dropped silently");
        check(LocalRouting.calculate(n,0,from,to,via,10)&&n.drives==1&&!n.travel&&n.multiple,"drive with via preserved");
        n=new AMapNavi();check(LocalRouting.calculate(n,0,null,to,via,10)&&n.drives==1,"drive without origin retained");
        via.clear();n=new AMapNavi();
        check(!LocalRouting.calculate(n,9,from,to,via,10)&&n.drives==0,"unknown mode rejected");
        for(double[] p:new double[][]{{Double.NaN,104},{30,Double.POSITIVE_INFINITY},{91,104},{30,181}}){
            check(!LocalRouting.calculate(n,1,new NaviLatLng(p[0],p[1]),to,via,10),"invalid origin rejected");
            check(!LocalRouting.calculate(n,1,from,new NaviLatLng(p[0],p[1]),via,10),"invalid destination rejected");
        }
        check(n.rides==0&&n.drives==0,"invalid inputs never invoke routing");
        check(LocalRouting.normalize(1)==1&&LocalRouting.normalize(-1)==0,"stored mode migration");
        System.out.println("LocalRouting: "+checks+" checks passed");
    }
}
