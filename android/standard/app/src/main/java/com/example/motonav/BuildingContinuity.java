package com.example.motonav;

import java.util.Arrays;

/** 换地图时按真实坐标保留已取到的楼块；普通图携带旧楼块不等于新楼块已完成。 */
public final class BuildingContinuity {
    private static final int SIZE=1200;
    private BuildingContinuity() {}
    public static final class Result {
        public final byte[] bytes;
        public final boolean complete;
        public final int retained;
        Result(byte[] bytes,boolean complete,int retained) {
            this.bytes=bytes;this.complete=complete;this.retained=retained;
        }
    }
    private static int u16(byte[] b,int p) { return (b[p]&255)|((b[p+1]&255)<<8); }
    private static int i32(byte[] b,int p) { return u16(b,p)|(u16(b,p+2)<<16); }
    private static boolean valid(byte[] b) {
        return b!=null && b.length>=17 && RasterPacket.valid(b,u16(b,0),i32(b,2),i32(b,6));
    }
    private static byte[] decode(byte[] b) {
        byte[] pixels=new byte[SIZE*SIZE];
        for(int pos=16,k=0;pos<b.length;) {
            int v=b[pos++]&255,run=(v>>>4)+1;
            if(run==16)run+=b[pos++]&255;
            Arrays.fill(pixels,k,k+run,(byte)(v&15));k+=run;
        }
        return pixels;
    }
    public static Result merge(byte[] incoming,byte[] known) {
        boolean complete=valid(incoming) && RasterPacket.isBuilding(incoming);
        Result original=new Result(incoming,complete,0);
        if(!valid(incoming) || !valid(known) || !RasterPacket.isBuilding(known) || u16(incoming,0)!=u16(known,0))return original;
        // 采样位于像素中心；使用长整数，兼容巡航中心的一米偏移和负坐标。
        long dx=(long)i32(incoming,2)-i32(known,2);
        long dy=(long)i32(known,6)-i32(incoming,6);
        long ox=Math.floorDiv(dx+1,2),oy=Math.floorDiv(dy+1,2);
        if(ox<=-SIZE || ox>=SIZE || oy<=-SIZE || oy>=SIZE)return original;
        byte[] current=decode(incoming),previous=decode(known);int retained=0;
        int xStart=(int)Math.max(0,-ox),xEnd=(int)Math.min(SIZE,SIZE-ox);
        int yStart=(int)Math.max(0,-oy),yEnd=(int)Math.min(SIZE,SIZE-oy);
        for(int y=yStart;y<yEnd;y++) {
            int a=y*SIZE+xStart,b=(int)((y+oy)*SIZE+xStart+ox);
            for(int x=xStart;x<xEnd;x++,a++,b++) {
                int ground=current[a]&255,roof=previous[b]&255;
                // 只补已确认的屋顶；新道路、水面及新取得的屋顶都优先。
                if((ground==0 || ground==2) && roof>=4 && roof<=7) {
                    current[a]=(byte)(4|(roof&1)|(ground&2));retained++;
                }
            }
        }
        if(retained==0)return original;
        byte[] packet=MapGeo.INSTANCE.encodeRaster(u16(incoming,0),i32(incoming,2),i32(incoming,6),SIZE,SIZE,2000|AmapBuildings.FLAG,current,null);
        if(!valid(packet))return original;
        return new Result(packet,complete,retained);
    }
}
