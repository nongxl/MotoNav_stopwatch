package com.example.motonav;

/** 同一相机位置只切换高德建筑层，以实际画面差分取得轮廓，避免依赖地面色或楼顶配色。 */
public final class BuildingDifference {
    private BuildingDifference() {}
    public static void mark(int[] without,int[] with) {
        if(without.length!=with.length)throw new IllegalArgumentException("楼块取图尺寸发生变化");
        for(int i=0;i<with.length;i++) {
            int a=without[i],b=with[i];
            if((a>>>24)<250 || (b>>>24)<250)throw new IllegalArgumentException("楼块图层仍有透明区域");
            int d=Math.max(Math.abs((a>>>16&255)-(b>>>16&255)),Math.max(Math.abs((a>>>8&255)-(b>>>8&255)),Math.abs((a&255)-(b&255))));
            with[i]=d>=4?0xffff00ff:0xfff8f9fa;
        }
    }
}
