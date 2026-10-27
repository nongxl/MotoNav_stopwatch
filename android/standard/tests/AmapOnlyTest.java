package com.example.motonav;

import java.util.Arrays;
import java.util.concurrent.CancellationException;
import java.util.concurrent.atomic.AtomicInteger;

public final class AmapOnlyTest {
    static int checks;
    static void check(boolean value,String label) { if(!value)throw new AssertionError(label);checks++; }
    static int u16(byte[] b,int n){return (b[n]&255)|((b[n+1]&255)<<8);}
    static int i32(byte[] b,int n){return u16(b,n)|(u16(b,n+2)<<16);}
    static int[] decode(byte[] bytes) {
        check(u16(bytes,0)==77,"Coordinate frame");
        check(i32(bytes,2)==-700 && i32(bytes,6)==700,"Upper left in GCJ frame");
        check(u16(bytes,10)==400 && u16(bytes,12)==400,"Existing satellite resolution");
        check(u16(bytes,14)==(3500|0x8000),"Firmware satellite palette flag and 3.5m sampling");
        int[] colors=new int[16],image=new int[160000];
        for(int k=0;k<16;k++)colors[k]=u16(bytes,16+2*k);
        int p=48,count=0;
        while(p<bytes.length) {
            int item=bytes[p++]&255,len=(item>>>4)+1;
            if(len==16)len+=bytes[p++]&255;
            check(count+len<=image.length,"RLE bounds");
            Arrays.fill(image,count,count+len,colors[item&15]);count+=len;
        }
        check(count==160000,"Complete satellite packet");return image;
    }
    public static void main(String[] args) {
        for(int old=-10;old<100;old++) {
            check(MapProviderPolicy.normalize(old)==3,"Every old provider migrates to AMap");
            for(int theme=0;theme<=11;theme++)
                check(MapProviderPolicy.usesAmap(old,theme)==(theme!=11),"Only radar disables SDK capture");
        }
        MapGeo.Frame f=new MapGeo.Frame(30.0,104.0,77);
        int[] raw=new int[160000];
        int[] rgb={0xffff0000,0xff00ff00,0xff0000ff,0xffffff00};
        for(int y=0;y<400;y++)for(int x=0;x<400;x++)raw[y*400+x]=rgb[(y>=200?2:0)+(x>=200?1:0)];
        int[] original=raw.clone();
        byte[] first=SatelliteRaster.INSTANCE.encode(f,0,0,700,raw,()->false).getBytes();
        int[] image=decode(first);
        check(image[0]==0xf800 && image[399]==0x07e0 && image[399*400]==0x001f && image[159999]==0xffe0,"Color/coordinate orientation");
        check(Arrays.equals(original,raw),"SDK pixels remain unchanged");
        check(Arrays.equals(first,SatelliteRaster.INSTANCE.encode(f,0,0,700,raw,()->false).getBytes()),"Deterministic palette");
        try {SatelliteRaster.INSTANCE.encode(f,0,0,700,new int[3],()->false);throw new AssertionError("Bad size accepted");}
        catch(IllegalArgumentException expected){checks++;}
        try {SatelliteRaster.INSTANCE.encode(f,0,0,700,raw,()->true);throw new AssertionError("Cancellation ignored");}
        catch(CancellationException expected){checks++;}
        AtomicInteger calls=new AtomicInteger();
        try {SatelliteRaster.INSTANCE.encode(f,0,0,700,raw,()->calls.incrementAndGet()>2);throw new AssertionError("Processing cancellation ignored");}
        catch(CancellationException expected){checks++;}
        System.out.println("PASS: "+checks+" checks; provider migration, satellite encoding/orientation/determinism/cancellation and existing firmware protocol");
    }
}
