package com.yuminaga.motonav;
public class NativeSignalGeometryTest {
 static void yes(boolean ok,String m){if(!ok)throw new AssertionError(m);}
 public static void main(String[] args){
  double step=1.0/111320;
  double[][] path={{30,104},{30+1000*step,104}};
  double[][] lamps={{30+300*step,104},{30+303*step,104},{30+650*step,104},{30+400*step,104+.002}};
  NativeSignalGeometry g=new NativeSignalGeometry(path,lamps);
  NativeSignalGeometry.Approach a=g.locate(30+270*step,104,3);
  yes(a.index==0&&a.metres>=29&&a.metres<=31,"near red signal distance");
  for(int i=0;i<75;i++)yes(g.locate(30+270*step,104,3).index==0,"stationary approach identity");
  a=g.locate(30+325*step,104,3);
  yes(a.index==1&&a.metres>=324&&a.metres<=326,"passed signal must advance; duplicate merged; off-route ignored");
  yes(g.locate(30+270*step,104,80).index==-1,"inaccurate GPS must not prove proximity");
  yes(g.locate(30+270*step,104+.003,3).index==-1,"off-route GPS rejected");
  System.out.println("Native signal geometry: proximity, stable stop, crossing, invalid GPS passed");
 }
}