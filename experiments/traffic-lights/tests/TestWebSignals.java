import com.yuminaga.motonav.*;
import java.util.*;
public class TestWebSignals {
    static void check(boolean ok,String why){if(!ok)throw new AssertionError(why);}
    static MotorTrack track(){double[][] p=new double[101][2];for(int i=0;i<p.length;i++)p[i]=new double[]{30+i*.0001,104};return new MotorTrack(p);}
    static String[] roads(MotorTrack t){String[] r=new String[t.points.length-1];Arrays.fill(r,"沿途路");return r;}
    static RouteSignalIndex.Node node(double lat,double lng,double back,String road){return new RouteSignalIndex.Node(lat,lng,back,lng,road);}
    static RouteSignalIndex make(MotorTrack t,RouteSignalIndex.Node... n){return new RouteSignalIndex(t,roads(t),Arrays.asList(n));}
    public static void main(String[] args){
        MotorTrack t=track();RouteSignalIndex.Node n=node(30.005,104,30.00475,"沿途路");
        RouteSignalIndex i=make(t,n);check(i.signals.size()==1,"actual marked on-route node accepted");
        check(i.distance(300)>=250&&i.distance(300)<=260,"distance follows selected route geometry");
        check(i.distance(200)==-1,"outside 300m hidden");
        check(i.next(557)==-1,"passed signal never reused");
        check(make(t,node(30.005,104.0005,30.00475,"沿途路")).signals.isEmpty(),"parallel road rejected");
        check(make(t,node(30.005,104,30.00525,"沿途路")).signals.isEmpty(),"opposite approach rejected");
        check(make(t,node(30.005,104,30.00475,"其他路")).signals.isEmpty(),"road mismatch rejected");
        check(make(t,node(30.005,104,30.00475,"")).signals.isEmpty(),"unnamed rejected");
        check(make(t,n,n).signals.size()==1,"duplicate SDK nodes deduplicated");
        check(make(t,n,node(30.0055,104,30.00525,"沿途路")).distance(400)==-1,"closely spaced junctions withheld");
        check(make(t).distance(400)==-1,"no metadata cannot synthesize a light from a turn");
        double[][] loop=new double[303][2];for(int k=0;k<101;k++){loop[k]=t.points[k];loop[101+k]=t.points[100-k];loop[202+k]=t.points[k];}
        MotorTrack repeated=new MotorTrack(loop);check(make(repeated,n).signals.isEmpty(),"ambiguous repeated same-direction passage rejected");
        System.out.println("PASS: genuine marked node projection, 300m range, passed/parallel/opposite/unnamed/nearby/repeated junction rejection");
    }
}
