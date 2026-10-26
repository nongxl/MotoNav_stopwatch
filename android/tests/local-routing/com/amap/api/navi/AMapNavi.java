package com.amap.api.navi;
import com.amap.api.navi.model.NaviLatLng;
import java.util.List;
public final class AMapNavi {
    public boolean travel,multiple,result=true,gps;
    public int rides,drives;
    public NaviLatLng from,to;
    public void setIsNaviTravelView(boolean b){travel=b;}
    public void setMultipleRouteNaviMode(boolean b){multiple=b;}
    public boolean startGPS(){gps=true;return true;}
    public boolean calculateRideRoute(NaviLatLng a,NaviLatLng b){from=a;to=b;rides++;return result;}
    public boolean calculateRideRoute(NaviLatLng b){if(!gps)throw new AssertionError("GPS not started");to=b;rides++;return result;}
    public boolean calculateDriveRoute(List<NaviLatLng> a,List<NaviLatLng> b,List<NaviLatLng> v,int s){from=a.get(0);to=b.get(0);drives++;return result;}
    public boolean calculateDriveRoute(List<NaviLatLng> b,List<NaviLatLng> v,int s){to=b.get(0);drives++;return result;}
}
