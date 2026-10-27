package com.yuminaga.motonav;

/** Move only generated web-route vias beyond a nearby maneuver. A via placed on
 * a turn can replace the provider's turn guidance with an arrive-at-via event.
 * Points remain on the chosen geometry; explicit user vias are never moved. */
public final class WebRouteAnchors {
 private WebRouteAnchors(){}
 public static double[][] select(double[][] points,int[] explicit,int[] turns){
  int[] indices=RouteAnchorSelector.select(points,explicit);
  if(indices.length==0)return new double[0][];
  double[] cumulative=new double[points.length];
  for(int i=1;i<points.length;i++)cumulative[i]=cumulative[i-1]+CarSignalAlignment.metres(points[i-1][0],points[i-1][1],points[i][0],points[i][1]);
  double spacing=Math.min(150,cumulative[cumulative.length-1]/8);
  double[][] result=new double[indices.length][];
  for(int k=0;k<indices.length;k++){
   int index=indices[k];double original=cumulative[index],target=original;
   boolean fixed=false;
   if(explicit!=null)for(int e:explicit)if(e==index)fixed=true;
   if(!fixed&&turns!=null){
    int nearby=-1;double best=Double.POSITIVE_INFINITY;
    for(int t:turns)if(t>0&&t<points.length-1){
     double offset=original-cumulative[t];
     if(offset>=-120&&offset<=60&&Math.abs(offset)<best){best=Math.abs(offset);nearby=t;}
    }
    if(nearby>=0){
     double turn=cumulative[nearby],nextTurn=cumulative[cumulative.length-1];
     for(int t:turns)if(t>nearby&&t<points.length&&cumulative[t]<nextTurn)nextTurn=cumulative[t];
     double proposed=turn+Math.min(100,(nextTurn-turn)/2);
     double upper=(k+1<indices.length?cumulative[indices[k+1]]:cumulative[cumulative.length-1])-spacing;
     if(nextTurn-turn>=80&&proposed>original&&proposed-original<=240&&proposed<=upper)target=proposed;
    }
   }
   result[k]=target==original?new double[]{points[index][0],points[index][1]}:at(points,cumulative,target);
  }
  return result;
 }
 private static double[] at(double[][] points,double[] cumulative,double target){
  int i=1;while(i<cumulative.length-1&&cumulative[i]<target)i++;
  double length=cumulative[i]-cumulative[i-1];double f=length>0?(target-cumulative[i-1])/length:0;
  f=Math.max(0,Math.min(1,f));
  return new double[]{points[i-1][0]+f*(points[i][0]-points[i-1][0]),points[i-1][1]+f*(points[i][1]-points[i-1][1])};
 }
}
