import com.yuminaga.motonav.*;
import java.util.*;
public final class TestRouteRoadIndex {
 static void check(boolean ok,String s){if(!ok)throw new AssertionError(s);}
 static MotorTrack line(double lng,boolean reverse){double[][] a=new double[201][2];for(int i=0;i<a.length;i++)a[i]=new double[]{30+(reverse?200-i:i)*.0001,lng};return new MotorTrack(a);}
 static String[] names(MotorTrack t){String[] names=new String[t.points.length-1];for(int i=0;i<names.length;i++)names[i]=i<100?"示例路一段":"示例路二段";return names;}
 public static void main(String[] args){
  MotorTrack web=line(104,false),source=line(104,false);String[] coarse=new String[200];Arrays.fill(coarse,"示例路一段");
  RouteRoadIndex index=new RouteRoadIndex(source,names(source),web);
  check(index.roadAt(500).equals("示例路一段"),"first section");
  check(index.roadAt(1500).equals("示例路二段"),"coarse web label resolved from matching corridor");
  check(new RouteRoadIndex(line(104.00012,false),names(source),web).roadAt(1500).isEmpty(),"parallel road 12m away rejected");
  check(new RouteRoadIndex(line(104,true),names(source),web).roadAt(1500).isEmpty(),"opposite approach rejected");
  double[][] crossing={{30.0135,103.999},{30.0135,104.001}};
  check(new RouteRoadIndex(new MotorTrack(crossing),new String[]{"交叉路"},web).roadAt(1500).isEmpty(),"crossing point cannot prove same road");
  double[][] loop=new double[603][2];for(int i=0;i<201;i++){loop[i]=web.points[i];loop[201+i]=web.points[200-i];loop[402+i]=web.points[i];}
  String[] loopRoads=new String[602];Arrays.fill(loopRoads,"重复道路");
  check(new RouteRoadIndex(new MotorTrack(loop),loopRoads,web).roadAt(1500).isEmpty(),"repeated same-direction passage ambiguous");
  RouteSignalIndex.Node node=new RouteSignalIndex.Node(30.0135,104,30.01325,104,"示例路二段");
  check(new RouteSignalIndex(web,coarse,Arrays.asList(node)).signals.isEmpty(),"old rule reproduces missing section light");
  check(new RouteSignalIndex(web,coarse,Arrays.asList(node),Arrays.asList(index)).signals.size()==1,"verified section light restored");
  RouteSignalIndex.Node wrong=new RouteSignalIndex.Node(30.0135,104,30.01325,104,"其他路");
  check(new RouteSignalIndex(web,coarse,Arrays.asList(wrong),Arrays.asList(index)).signals.isEmpty(),"different source road remains rejected");
  RouteSignalIndex.Node shifted=new RouteSignalIndex.Node(30.005,104.00012,30.00475,104.00012,"示例路一段");
  RouteSignalIndex.Signal marker=new RouteSignalIndex(web,coarse,Arrays.asList(shifted)).signals.get(0);
  double[] projected=web.at(marker.progress);
  check(MotorTrack.distance(marker.lat,marker.lng,projected[0],projected[1])<.001,"marker and route distance share the exact projected coordinate");
  check(Math.abs(marker.lng-104)<1e-9,"source metadata lateral offset is not drawn beside selected road");
  check(Math.abs(marker.lat-30.005)<1e-8,"no invented forward shift toward a junction");
  RouteSignalIndex.Node exact=RouteSignalIndex.sdkPoint(source,names(source),30.0135,104);
  check(exact!=null && exact.road.equals("示例路二段"),"authoritative SDK coordinate takes fine road name");
  check(Math.abs(exact.lat-30.0135)<1e-9,"SDK coordinate is retained, not replaced by link endpoint");
  check(RouteSignalIndex.sdkPoint(source,names(source),30.0135,104.001)==null,"off-road explicit light rejected");
  check(RouteSignalIndex.sdkPoint(new MotorTrack(loop),loopRoads,30.0135,104)==null,"ambiguous explicit light rejected");
  System.out.println("PASS: coarse labels, 40m corridor, parallel/opposite/crossing/repeated rejection, marker projection");
 }
}
