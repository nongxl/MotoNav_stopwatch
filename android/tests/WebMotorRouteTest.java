package com.example.motonav;

import java.nio.file.*;
import java.util.*;

public final class WebMotorRouteTest {
    static int checks;
    static void check(boolean good,String why) { checks++;if(!good)throw new AssertionError(why); }
    static void fails(Runnable action,String why) {
        try { action.run(); } catch(IllegalArgumentException|IllegalStateException e) {checks++;return;}
        throw new AssertionError(why);
    }
    public static void main(String[] args) throws Exception {
        MotorRequest request=new MotorRequest(new MotorPoint(30.533852,104.007799,"起点",""),
            new MotorPoint(30.077114,103.848417,"终点","B07290070A"),Collections.emptyList());
        List<WebMotorRoute> routes=WebMotorRoutes.INSTANCE.parse(Files.readString(Path.of(args[0])),request);
        check(!routes.isEmpty(),"real response routes");
        WebMotorRoute route=routes.get(0);
        check(route.getDistance()==61579,"actual distance");
        check(route.getSeconds()==5580,"actual duration");
        check(route.getLights()==76,"actual lights");
        check(route.getPoints().size()==865,"all 865 distinct points from the real response");
        check(route.getSteps().size()==11,"all 11 turn instructions");
        check(route.getSteps().get(0).getDir().equals("R"),"first right turn");
        MotorTrack track=route.tracker();
        long elapsed=1000; boolean arrived=false;
        for(double p=0;p<track.total;p+=20) {
            double[] pose=track.at(p);
            MotorTrack.Match m=track.update(pose[0],pose[1],5,20,elapsed);elapsed+=1000;
            check(m!=null,"real geometry tracking");
            check(Math.abs(m.progress-p)<25,"progress tracks actual geometry");
            if(track.total-p>50)check(!m.arrived,"no premature arrival on real route");
            arrived|=m.arrived;
        }
        double[] end=track.at(track.total);
        for(int i=0;i<4;i++){arrived|=track.update(end[0],end[1],5,0,elapsed).arrived;elapsed+=1000;}
        check(arrived,"real route can arrive");
        String minimal="{\"code\":1,\"data\":[{\"distance\":100,\"cost\":{\"duration\":10},\"steps\":[{\"polyline\":\"104,30;104,30.001\",\"navi\":{\"action\":\"到达目的地\"}}]}]}";
        check(WebMotorRoutes.INSTANCE.parse(minimal,request).get(0).getSteps().get(0).getDir().equals("S"),"arrival-ahead is guidance only");
        fails(()->WebMotorRoutes.INSTANCE.parse("<html>verification</html>",request),"verification must not produce route");
        fails(()->WebMotorRoutes.INSTANCE.parse("{\"code\":0,\"message\":\"blocked\"}",request),"error code");
        fails(()->WebMotorRoutes.INSTANCE.parse("{\"code\":1,\"data\":[]}",request),"empty data");
        fails(()->WebMotorRoutes.INSTANCE.parse(minimal.replace("104,30.001","NaN,30.001"),request),"invalid geometry");
        fails(()->WebMotorRoutes.INSTANCE.parse(minimal.replace("104,30.001","104,50"),request),"discontinuous geometry");
        check(route.getPoints().stream().allMatch(p->MotorTrack.valid(p[0],p[1])),"all lat/lng finite");
        String url=request.pageUrl();
        check(WebMotorRoutes.INSTANCE.request(url).equals(request),"page URL roundtrip");
        MotorRequest viaRequest=new MotorRequest(request.getStart(),request.getEnd(),
            List.of(new MotorPoint(30.3,104,"中途地点","")));
        check(WebMotorRoutes.INSTANCE.request(viaRequest.pageUrl()).equals(viaRequest),"explicit via preserved");
        check(WebMotorRoutes.INSTANCE.request(url+"&via%5B0%5D%5Blnglat%5D=104%2C30.3&via%5B0%5D%5Bname%5D=%E4%B8%AD%E9%80%94%E5%9C%B0%E7%82%B9").equals(viaRequest),"nested via preserved");
        fails(()->WebMotorRoutes.INSTANCE.request(url.replace("https:","http:")),"non-HTTPS rejected");
        fails(()->WebMotorRoutes.INSTANCE.request(url.replace("www.amap.com","www.amap.com.evil.example")),"foreign host rejected");
        fails(()->WebMotorRoutes.INSTANCE.request(url.replace("type=4","type=0")),"car mode rejected");
        fails(()->WebMotorRoutes.INSTANCE.request(url.replace("flat=30.533852","flat=NaN")),"invalid start coordinate rejected");
        fails(()->WebMotorRoutes.INSTANCE.request(url+"&vid=unresolved"),"unresolved via cannot be silently discarded");
        fails(()->WebMotorRoutes.INSTANCE.request(url+"&vlat=30.3%7C30.4&vlon=104"),"mismatched via arrays");
        // Every alternative uses its own full coordinate sequence, not the first route's geometry.
        for(WebMotorRoute alternate:routes) {
            MotorTrack next=alternate.tracker();
            check(next.total>50000,"alternative geometry is usable");
            check(alternate.getPoints().size()>2,"alternative has a route");
        }
        System.out.println("WebMotorRoute: "+checks+" checks passed; "+route.getPoints().size()+" points / "+route.getSteps().size()+" steps; geometry "+Math.round(track.total)+"m");
    }
}
