package android.os;
public class Handler {
    public Handler(Looper looper) {}
    public boolean post(Runnable r) { r.run(); return true; }
}
