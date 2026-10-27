package com.amap.api.navi.model;
public final class NaviLatLng {
    private final double lat,lng;
    public NaviLatLng(double lat,double lng){this.lat=lat;this.lng=lng;}
    public double getLatitude(){return lat;}
    public double getLongitude(){return lng;}
}
