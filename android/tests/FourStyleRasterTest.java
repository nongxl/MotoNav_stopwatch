package com.example.motonav;
import java.util.Arrays;
import java.util.concurrent.CancellationException;
public final class FourStyleRasterTest {
    static int checks;
    static void check(boolean yes,String label){if(!yes)throw new AssertionError(label);checks++;}
    static int u16(byte[] b,int n){return (b[n]&255)|((b[n+1]&255)<<8);}
    static int[] decode(byte[] b){
        int size=u16(b,10)*u16(b,12),p=RasterPacket.hasPalette(b)?48:16,count=0;
        int[] out=new int[size];
        while(p<b.length){int v=b[p++]&255,n=(v>>>4)+1;if(n==16)n+=b[p++]&255;
            check(n<=size-count,"Run fits packet");
            Arrays.fill(out,count,count+n,RasterPacket.hasPalette(b)?u16(b,16+2*(v&15)):v&15);count+=n;
        }
        check(count==size,"Full packet coverage");return out;
    }
    public static void main(String[] args) throws Exception {
        int n=1200;int[] raw=new int[n*n];Arrays.fill(raw,0xffeeeeee);
        for(int y=100;y<500;y++)for(int x=100;x<400;x++)raw[y*n+x]=0xffbbbbbb;
        for(int y=100;y<500;y++)for(int x=700;x<1000;x++)raw[y*n+x]=0xffbbdd99;
        for(int y=700;y<1000;y++)for(int x=700;x<1000;x++)raw[y*n+x]=0xff99ccff;
        for(int y=0;y<n;y++)for(int x=580;x<620;x++)raw[y*n+x]=0xffffffff;
        for(int y=580;y<620;y++)for(int x=0;x<n;x++)raw[y*n+x]=0xffffcc66;
        int[] clean=raw.clone();
        for(int y=200;y<205;y++)for(int x=200;x<280;x++)raw[y*n+x]=0xff303030;
        MapGeo.Frame frame=new MapGeo.Frame(30,104,77);
        byte[] original=OriginalRaster.INSTANCE.encode(frame,-1200,1200,n,raw,()->false);
        check(RasterPacket.valid(original,77,-1200,1200),"Original full-size palette packet accepted");
        check(RasterPacket.hasPalette(original)&&!RasterPacket.isBuilding(original),"Original carries SDK colors, not Vice materials");
        int[] colors=decode(original);
        check(colors[202*n+220]!=colors[220*n+220],"Original text-like pixels preserved");
        check(colors[800*n+800]!=colors[200*n+800],"Original retains water and park colors");
        check(Arrays.equals(original,OriginalRaster.INSTANCE.encode(frame,-1200,1200,n,raw,()->false)),"Deterministic original packet");
        byte[] roads=MinimalRoadRaster.INSTANCE.indices(clean,n);
        check(roads[300*n+600]==1&&roads[600*n+300]==1,"White and arterial roads retained");
        check(roads[200*n+200]==0&&roads[200*n+800]==0&&roads[800*n+800]==0,"No buildings, parks or water");
        for(byte b:roads)if(b!=0&&b!=1)throw new AssertionError("Material leaked into minimalist mask");
        byte[] mono=MapGeo.INSTANCE.encodeRaster(77,-1200,1200,n,n,2000,roads,null);
        check(RasterPacket.valid(mono,77,-1200,1200)&&!RasterPacket.hasPalette(mono),"Binary minimalist packet");
        check(Arrays.equals(roads,toBytes(decode(mono))),"Minimalist mask survives compression");
        check(!Arrays.equals(original,mono),"Independent payloads");
        byte[] damaged=original.clone();damaged[15]|=0x40;
        check(!RasterPacket.valid(damaged,77,-1200,1200),"Palette plus buildings rejected");
        check(!RasterPacket.valid(Arrays.copyOf(original,original.length-1),77,-1200,1200),"Truncation rejected");
        check(!RasterPacket.valid(original,78,-1200,1200),"Wrong coordinate frame rejected");
        if(args.length>0){
            java.nio.file.Path dir=java.nio.file.Path.of(args[0]);java.nio.file.Files.createDirectories(dir);
            java.nio.file.Files.write(dir.resolve("original.bin"),original);
            java.nio.file.Files.write(dir.resolve("minimal.bin"),mono);
            for(String style:new String[]{"original","minimal"}){
                int[] image=style.equals("original")?colors:decode(mono);
                byte[] expected=new byte[image.length*2];
                for(int i=0;i<image.length;i++){expected[2*i]=(byte)image[i];expected[2*i+1]=(byte)(image[i]>>>8);}
                java.nio.file.Files.write(dir.resolve(style+"-expected.rgb565"),expected);
            }
        }
        try{OriginalMapColors.quantize(raw,()->true);throw new AssertionError("Cancel ignored");}catch(CancellationException expected){checks++;}
        System.out.println("PASS: "+checks+" checks; original colors/text, binary road mask, packet round trip and corrupt data");
    }
    static byte[] toBytes(int[] a){byte[] b=new byte[a.length];for(int i=0;i<a.length;i++)b[i]=(byte)a[i];return b;}
}
