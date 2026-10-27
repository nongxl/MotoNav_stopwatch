package com.yuminaga.motonav;
public class MapPoseUpdatesTest {
 public static void main(String[] args){
  MapPoseUpdates p=new MapPoseUpdates();int positions=0,headings=0,cameras=0;
  for(int i=0;i<100000;i++){
   if(p.positionChanged(30.5,104.0))positions++;
   if(p.headingChanged(90f))headings++;
   if(p.cameraChanged(30.5,104.0,90f))cameras++;
  }
  check(positions==1&&headings==1&&cameras==1);
  check(!p.positionChanged(30.5,104.0));check(p.headingChanged(91f));check(p.cameraChanged(30.5,104.0,91f));
  check(p.positionChanged(30.50000001,104.0));check(!p.headingChanged(91f));check(p.cameraChanged(30.50000001,104.0,91f));
  p.invalidateCamera();check(p.cameraChanged(30.50000001,104.0,91f));check(!p.positionChanged(30.50000001,104.0));
  // Preserve the prior string-key distinction between signed zero and actual tiny changes.
  check(p.positionChanged(0.0,0.0));check(p.positionChanged(-0.0,0.0));check(!p.positionChanged(-0.0,0.0));
  System.out.println("Map pose change detection passed; 100000 stationary samples yield one property update; movement/rotation/follow reset preserved");
 }
 static void check(boolean condition){if(!condition)throw new AssertionError();}
}
