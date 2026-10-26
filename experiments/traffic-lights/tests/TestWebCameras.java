import com.yuminaga.motonav.*;
import java.util.*;
public class TestWebCameras {
    static void check(boolean b,String why){if(!b)throw new AssertionError(why);}
    static MotorTrack track(double lng,boolean reverse){
        double[][] p=new double[101][2];for(int i=0;i<101;i++)p[i]=new double[]{30+(reverse?100-i:i)*.0001,lng};return new MotorTrack(p);
    }
    static String[] roads(MotorTrack t){String[] s=new String[t.points.length-1];Arrays.fill(s,"沿途路");return s;}
    public static void main(String[] args){
        MotorTrack source=track(104,false);RouteCameraIndex.Camera c=new RouteCameraIndex.Camera(30.005,104,0,60);
        List<RouteCameraIndex.Marker> good=RouteCameraIndex.match(source,roads(source),source,roads(source),Arrays.asList(c,c));
        check(good.size()==1 && good.get(0).speed==60 && good.get(0).type==0,"real metadata type/speed retained and duplicates removed");
        check(good.get(0).progress>550 && good.get(0).progress<560,"camera distance belongs to selected web route");
        MotorTrack reverse=track(104,true),parallel=track(104.0005,false);
        check(RouteCameraIndex.match(source,roads(source),reverse,roads(reverse),Arrays.asList(c)).isEmpty(),"opposite web approach rejected");
        check(RouteCameraIndex.match(source,roads(source),parallel,roads(parallel),Arrays.asList(c)).isEmpty(),"parallel road rejected");
        String[] unnamed=roads(source);Arrays.fill(unnamed,"");
        check(RouteCameraIndex.match(source,unnamed,source,roads(source),Arrays.asList(c)).isEmpty(),"missing source road rejected");
        check(RouteCameraIndex.match(source,roads(source),source,roads(source),Arrays.asList(new RouteCameraIndex.Camera(30.005,104.002,0,60))).isEmpty(),"camera off reference route rejected");
        check(RouteCameraIndex.match(source,roads(source),source,roads(source),Arrays.asList(new RouteCameraIndex.Camera(Double.NaN,104,0,60))).isEmpty(),"nonfinite rejected");
        double[][] loop=new double[303][2];for(int k=0;k<101;k++){loop[k]=source.points[k];loop[101+k]=source.points[100-k];loop[202+k]=source.points[k];}
        MotorTrack repeated=new MotorTrack(loop);
        check(RouteCameraIndex.match(repeated,roads(repeated),source,roads(source),Arrays.asList(c)).isEmpty(),"ambiguous source passage rejected");
        System.out.println("PASS: camera metadata projection, preserved type/speed, duplicate/opposite/parallel/unnamed/off-route/repeated rejection");
    }
}
