package com.yuminaga.motonav;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.function.BooleanSupplier;
import java.util.concurrent.CancellationException;

/** Weighted median-cut of the actual SDK pixels. No material classification, blur or theme palette. */
public final class OriginalMapColors {
    public static final class Result {
        public final byte[] indices;
        public final int[] palette;
        Result(byte[] indices,int[] palette){this.indices=indices;this.palette=palette;}
    }
    private static void cancelled(BooleanSupplier stop) {
        if(Thread.currentThread().isInterrupted() || stop.getAsBoolean())throw new CancellationException();
    }
    private static int key(int c){return ((c>>>19)&31)<<10 | ((c>>>11)&31)<<5 | ((c>>>3)&31);}
    private static int component(int k,int axis){return (k >>> (10-axis*5))&31;}
    private static final class Box {
        Integer[] keys; int axis,range,total;
        Box(Integer[] keys,int[] hist){this.keys=keys;
            for(int a=0;a<3;a++){
                int lo=31,hi=0;
                for(int k:keys){int v=component(k,a);lo=Math.min(lo,v);hi=Math.max(hi,v);}
                if(hi-lo>range){range=hi-lo;axis=a;}
            }
            for(int k:keys)total+=hist[k];
        }
        double score(){return keys.length>1 ? range*Math.sqrt(total) : -1;}
    }
    public static Result quantize(int[] raw,BooleanSupplier stop){
        if(raw.length==0)throw new IllegalArgumentException("Empty SDK image");
        int[] hist=new int[32768];
        for(int i=0;i<raw.length;i++){if((i&16383)==0)cancelled(stop);hist[key(raw[i])]++;}
        ArrayList<Integer> used=new ArrayList<>();
        for(int k=0;k<hist.length;k++)if(hist[k]!=0)used.add(k);
        ArrayList<Box> boxes=new ArrayList<>();boxes.add(new Box(used.toArray(new Integer[0]),hist));
        while(boxes.size()<16){
            cancelled(stop);
            Box best=boxes.stream().max(Comparator.comparingDouble(Box::score)).get();
            if(best.keys.length<2)break;
            int axis=best.axis;Arrays.sort(best.keys,Comparator.comparingInt(k->component(k,axis)));
            int cumulative=0,cut=1;
            for(int i=0;i<best.keys.length-1;i++){cumulative+=hist[best.keys[i]];cut=i+1;if(cumulative>=best.total/2)break;}
            boxes.remove(best);
            boxes.add(new Box(Arrays.copyOfRange(best.keys,0,cut),hist));
            boxes.add(new Box(Arrays.copyOfRange(best.keys,cut,best.keys.length),hist));
        }
        int[] palette=new int[16];byte[] lookup=new byte[32768];
        for(int i=0;i<boxes.size();i++){
            Box box=boxes.get(i);long r=0,g=0,b=0;
            for(int k:box.keys){long count=hist[k];r+=component(k,0)*count;g+=component(k,1)*count;b+=component(k,2)*count;lookup[k]=(byte)i;}
            int rr=(int)((r+box.total/2)/box.total),gg=(int)((g+box.total/2)/box.total),bb=(int)((b+box.total/2)/box.total);
            palette[i]=(rr<<11)|(((gg<<1)|(gg>>>4))<<5)|bb;
        }
        for(int i=boxes.size();i<16;i++)palette[i]=palette[0];
        byte[] indices=new byte[raw.length];
        for(int i=0;i<raw.length;i++){if((i&16383)==0)cancelled(stop);indices[i]=lookup[key(raw[i])];}
        cancelled(stop);return new Result(indices,palette);
    }
}
