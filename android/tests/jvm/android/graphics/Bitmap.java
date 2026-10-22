package android.graphics;
/** Test-only preview allocation adapter; production encoder runs unchanged. */
public final class Bitmap {
    public enum Config { ARGB_8888 }
    public final int[] pixels;
    private Bitmap(int[] input) { pixels=input.clone(); }
    public static Bitmap createBitmap(int[] input,int width,int height,Config config) {
        if(input.length != width*height) throw new IllegalArgumentException("Preview size");
        return new Bitmap(input);
    }
}
