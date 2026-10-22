package com.example.motonav;

import java.util.*;

/** Synthetic routes only: no real trip, account cookies or service requests. */
public final class WebMotorRouteTest {
    static int checks;
    static void check(boolean good, String why) { checks++; if (!good) throw new AssertionError(why); }
    static void fails(Runnable action, String why) {
        try { action.run(); } catch (IllegalArgumentException | IllegalStateException e) { checks++; return; }
        throw new AssertionError(why);
    }
    public static void main(String[] args) {
        MotorRequest request = new MotorRequest(new MotorPoint(30,104,"起点",""),
            new MotorPoint(30.002,104.001,"终点",""), Collections.emptyList());
        String path = "{\"distance\":333,\"cost\":{\"duration\":90,\"traffic_lights\":2},\"steps\":["
            + "{\"polyline\":\"104,30;104,30.001;104,30.002\",\"step_distance\":222,\"navi\":{\"action\":\"右转\"}},"
            + "{\"polyline\":\"104,30.002;104.001,30.002\",\"step_distance\":111,\"navi\":{\"action\":\"到达目的地\"}}]}";
        String response = "{\"code\":1,\"data\":[" + path + "," + path.replace("104.001", "104.002") + "]}";
        List<WebMotorRoute> routes = WebMotorRoutes.INSTANCE.parse(response,request);
        check(routes.size()==2,"all alternatives parsed");
        WebMotorRoute route=routes.get(0);
        check(route.getPoints().size()==4,"shared boundary deduplicated");
        check(route.getSteps().size()==2,"all turn steps retained");
        check(route.getDistance()==333 && route.getSeconds()==90 && route.getLights()==2,"metadata");
        check(route.getSteps().get(0).getDir().equals("R"),"right turn");
        check(route.getSteps().get(1).getDir().equals("S"),"destination ahead is guidance only");
        check(routes.get(1).getPoints().get(3)[1]==104.002,"alternative owns its geometry");
        for (WebMotorRoute candidate : routes) {
            MotorTrack track=candidate.tracker();
            long time=1000;
            for (double p=0;p<track.total-40;p+=10) {
                double[] pose=track.at(p);
                MotorTrack.Match match=track.update(pose[0],pose[1],5,10,time);time+=1000;
                check(Math.abs(match.progress-p)<20,"full geometry tracking");
                check(!match.arrived,"no early completion");
            }
            double[] end=track.at(track.total);
            boolean arrived=false;
            for(int i=0;i<4;i++) { arrived |= track.update(end[0],end[1],5,0,time).arrived;time+=1000; }
            check(arrived,"stable terminal fixes arrive");
        }
        fails(()->WebMotorRoutes.INSTANCE.parse("<html>verification</html>",request),"verification page rejected");
        fails(()->WebMotorRoutes.INSTANCE.parse("{\"code\":0,\"message\":\"blocked\"}",request),"service error rejected");
        fails(()->WebMotorRoutes.INSTANCE.parse("{\"code\":1,\"data\":[]}",request),"empty route rejected");
        fails(()->WebMotorRoutes.INSTANCE.parse(response.replace("104.001,30.002","NaN,30.002"),request),"invalid coordinate rejected");
        fails(()->WebMotorRoutes.INSTANCE.parse(response.replace("104.001,30.002","104,50"),request),"discontinuous geometry rejected");
        String url=request.pageUrl();
        check(WebMotorRoutes.INSTANCE.request(url).equals(request),"URL round trip");
        MotorRequest via=new MotorRequest(request.getStart(),request.getEnd(),List.of(new MotorPoint(30.001,104,"途经点","")));
        check(WebMotorRoutes.INSTANCE.request(via.pageUrl()).equals(via),"explicit vias preserved");
        check(WebMotorRoutes.INSTANCE.request(url+"&via%5B0%5D%5Blnglat%5D=104%2C30.001&via%5B0%5D%5Bname%5D=%E9%80%94%E7%BB%8F%E7%82%B9").equals(via),"nested vias preserved");
        fails(()->WebMotorRoutes.INSTANCE.request(url.replace("https:","http:")),"HTTPS required");
        fails(()->WebMotorRoutes.INSTANCE.request(url.replace("www.amap.com","www.amap.com.evil.example")),"foreign host rejected");
        fails(()->WebMotorRoutes.INSTANCE.request(url.replace("type=4","type=0")),"car mode rejected");
        fails(()->WebMotorRoutes.INSTANCE.request(url.replace("flat=30.0","flat=NaN")),"invalid start rejected");
        fails(()->WebMotorRoutes.INSTANCE.request(url+"&vid=unresolved"),"ID-only vias cannot disappear");
        fails(()->WebMotorRoutes.INSTANCE.request(url+"&vlat=30.001%7C30.002&vlon=104"),"mismatched vias rejected");
        System.out.println("WebMotorRoute synthetic fixtures: "+checks+" checks passed");
    }
}
