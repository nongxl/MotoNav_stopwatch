package com.yuminaga.motonav;

/** 只提取高德建筑层开关差分得到的轮廓，不从普通地面颜色猜测或虚构房屋。 */
public final class AmapBuildings {
    public static final int FLAG=0x4000;
    public static final int ROOF=4, OUTLINE=5;
    private AmapBuildings() {}
    public static boolean marked(int color) {
        int r=(color>>>16)&255,g=(color>>>8)&255,b=color&255;
        return r>=140 && b>=140 && g*2<Math.min(r,b);
    }
    public static final class Result {
        public final byte[] pixels;
        public final int buildings, roofPixels;
        Result(byte[] pixels,int buildings,int roofPixels){this.pixels=pixels;this.buildings=buildings;this.roofPixels=roofPixels;}
    }
    public static Result apply(int[] raw,byte[] base,int w,int h) {
        if(w<=0 || h<=0 || (long)w*h!=raw.length || raw.length!=base.length)throw new IllegalArgumentException("楼块图尺寸不一致");
        byte[] mask=new byte[raw.length];int marked=0;
        for(int i=0;i<raw.length;i++)if(marked(raw[i])){mask[i]=1;marked++;}
        if(marked>raw.length/2)throw new IllegalArgumentException("建筑图层覆盖异常，保留当前地图");
        int[] component=new int[raw.length];int buildings=0;
        for(int i=0;i<mask.length;i++){
            if(mask[i]!=1)continue;
            int head=0,tail=1,minX=i%w,maxX=minX,minY=i/w,maxY=minY;component[0]=i;mask[i]=2;
            while(head<tail){
                int p=component[head++],x=p%w,y=p/w;
                minX=Math.min(minX,x);maxX=Math.max(maxX,x);minY=Math.min(minY,y);maxY=Math.max(maxY,y);
                if(x>0 && mask[p-1]==1){mask[p-1]=2;component[tail++]=p-1;}
                if(x+1<w && mask[p+1]==1){mask[p+1]=2;component[tail++]=p+1;}
                if(y>0 && mask[p-w]==1){mask[p-w]=2;component[tail++]=p-w;}
                if(y+1<h && mask[p+w]==1){mask[p+w]=2;component[tail++]=p+w;}
            }
            int bw=maxX-minX+1,bh=maxY-minY+1;
            boolean valid=tail>=6 && tail<=raw.length/8 && bw>=2 && bh>=2 && (long)tail*8>=(long)bw*bh;
            if(valid)buildings++;
            for(int j=0;j<tail;j++)mask[component[j]]=(byte)(valid?3:4);
        }
        byte[] out=base.clone();int pixels=0;
        for(int y=0;y<h;y++)for(int x=0;x<w;x++){
            int i=y*w+x;
            // 道路和水面优先；保留屋顶下原本是地面还是绿地，供旧固件无损回退。
            if(mask[i]!=3 || (base[i]!=0 && base[i]!=2))continue;
            boolean edge=x==0 || x+1==w || y==0 || y+1==h || mask[i-1]!=3 || mask[i+1]!=3 || mask[i-w]!=3 || mask[i+w]!=3;
            out[i]=(byte)((edge?OUTLINE:ROOF)|(base[i]&2));pixels++;
        }
        return new Result(out,buildings,pixels);
    }
}
