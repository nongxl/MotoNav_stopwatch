package android.util;
import java.io.*;
import java.nio.file.*;
/** Exercise real file bytes; Android's platform AtomicFile is used on the phone. */
public class AtomicFile {
    private final File base, next;
    public AtomicFile(File f) { base=f; next=new File(f+".new"); }
    public FileInputStream openRead() throws IOException { return new FileInputStream(base); }
    public FileOutputStream startWrite() throws IOException { return new FileOutputStream(next); }
    public void finishWrite(FileOutputStream s) throws IOException { s.close(); Files.move(next.toPath(),base.toPath(),StandardCopyOption.REPLACE_EXISTING); }
    public void failWrite(FileOutputStream s) throws IOException { s.close(); next.delete(); }
}
