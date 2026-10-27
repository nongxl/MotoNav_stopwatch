package com.yuminaga.motonav;

/** Per-view change detection; never throttles guidance, GPS or map camera animation. */
public final class MapPoseUpdates {
 private boolean positionKnown,headingKnown,cameraKnown;
 private long lat,lng,cameraLat,cameraLng;
 private int heading,cameraHeading;
 public boolean positionChanged(double latitude,double longitude){
  long a=Double.doubleToLongBits(latitude),b=Double.doubleToLongBits(longitude);
  if(positionKnown&&lat==a&&lng==b)return false;
  positionKnown=true;lat=a;lng=b;return true;
 }
 public boolean headingChanged(float bearing){
  int value=Float.floatToIntBits(bearing);
  if(headingKnown&&heading==value)return false;
  headingKnown=true;heading=value;return true;
 }
 public boolean cameraChanged(double latitude,double longitude,float bearing){
  long a=Double.doubleToLongBits(latitude),b=Double.doubleToLongBits(longitude);int c=Float.floatToIntBits(bearing);
  if(cameraKnown&&cameraLat==a&&cameraLng==b&&cameraHeading==c)return false;
  cameraKnown=true;cameraLat=a;cameraLng=b;cameraHeading=c;return true;
 }
 public void invalidateCamera(){cameraKnown=false;}
}
