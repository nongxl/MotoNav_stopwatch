package android.content;
import java.io.*;
import java.nio.file.*;
import java.util.*;
import org.json.*;
/** Minimal file-backed Android boundary for JVM persistence tests, never packaged in the APK. */
public class Context {
    private final File root;
    public Context(File root) { this.root=root; root.mkdirs(); }
    public Context getApplicationContext() { return this; }
    public File getNoBackupFilesDir() { File f=new File(root,"no_backup"); f.mkdirs(); return f; }
    public ContentResolver getContentResolver() { return new ContentResolver(); }
    public SharedPreferences getSharedPreferences(String name,int mode) {
        File f=new File(root,name+".prefs");
        return new SharedPreferences() {
            JSONObject read() {
                try { return new JSONObject(Files.readString(f.toPath())); }
                catch(Exception e) { return new JSONObject(); }
            }
            public boolean getBoolean(String k,boolean v) { return read().optBoolean(k,v); }
            public String getString(String k,String v) { return read().optString(k,v); }
            public Editor edit() { return new Editor() {
                JSONObject o=read();
                public Editor putString(String k,String v) { try {o.put(k,v);} catch(Exception e){throw new RuntimeException(e);} return this; }
                public Editor putBoolean(String k,boolean v) { try {o.put(k,v);} catch(Exception e){throw new RuntimeException(e);} return this; }
                public boolean commit() { try {Files.writeString(f.toPath(),o.toString()); return true;} catch(IOException e) {return false;} }
            }; }
        };
    }
}
