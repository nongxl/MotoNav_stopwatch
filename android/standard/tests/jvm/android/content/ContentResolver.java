package android.content;
import java.io.*;
import android.net.Uri;
public class ContentResolver {
    public OutputStream openOutputStream(Uri uri,String mode) throws IOException { return new FileOutputStream(uri.path); }
}
