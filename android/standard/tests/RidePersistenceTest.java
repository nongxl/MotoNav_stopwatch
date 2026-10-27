package com.example.motonav;

import android.content.Context;
import android.net.Uri;
import kotlin.Unit;
import java.io.*;
import java.nio.file.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;
import javax.xml.parsers.*;
import org.w3c.dom.*;

/** Runs the APK's compiled Kotlin classes against small JVM Android I/O adapters. */
public class RidePersistenceTest {
    private static final RideRecorder R=RideRecorder.INSTANCE;
    private static int checks;
    private static long clock=1792296000000L;
    private static void check(boolean b,String s) { checks++; if(!b) throw new AssertionError(s); }
    private static void await(CountDownLatch latch) throws Exception {
        if(!latch.await(20,TimeUnit.SECONDS)) {
            Thread.getAllStackTraces().forEach((t,s)->{System.err.println(t.getName());for(StackTraceElement frame:s)System.err.println("  "+frame);});
            throw new AssertionError("worker callback timed out");
        }
    }
    private static List<RideSummary> history() throws Exception {
        AtomicReference<List<RideSummary>> result=new AtomicReference<>(); AtomicReference<String> error=new AtomicReference<>();
        CountDownLatch latch=new CountDownLatch(1);
        R.history((list,e)->{result.set(list);error.set(e);latch.countDown();return Unit.INSTANCE;}); await(latch);
        if(error.get()!=null) throw new AssertionError(error.get()); return result.get();
    }
    private static void fix(double lat,double lng,long after) { clock+=after; R.onFix(lat,lng,clock,clock,8,35); }
    private static String delete(String id) throws Exception {
        AtomicReference<String> error=new AtomicReference<>(); CountDownLatch l=new CountDownLatch(1);
        R.delete(id,e->{error.set(e);l.countDown();return Unit.INSTANCE;}); await(l); return error.get();
    }
    private static String export(String id,File file) throws Exception {
        AtomicReference<String> error=new AtomicReference<>(); CountDownLatch l=new CountDownLatch(1);
        R.export(id,new Uri(file.toString()),e->{error.set(e);l.countDown();return Unit.INSTANCE;}); await(l); return error.get();
    }
    private static List<RidePoint> preview(String id) throws Exception {
        AtomicReference<List<RidePoint>> points=new AtomicReference<>(); AtomicReference<String> error=new AtomicReference<>(); CountDownLatch l=new CountDownLatch(1);
        R.preview(id,(p,e)->{points.set(p);error.set(e);l.countDown();return Unit.INSTANCE;}); await(l);
        if(error.get()!=null) throw new AssertionError(error.get()); return points.get();
    }
    public static void main(String[] args) {
        try { run(args); System.out.println("RidePersistenceTest "+args[0]+": "+checks+" checks passed"); System.exit(0); }
        catch(Throwable e) { e.printStackTrace(); System.exit(1); }
    }
    private static void run(String[] args) throws Exception {
        File root=new File(args[1]); Context context=new Context(root); R.init(context); history();
        if(args[0].equals("seed-crash")) {
            R.start("巡航","异常退出测试",false); fix(31.2304,121.4737,0); fix(31.2307,121.4737,3000);
            List<RideSummary> list=history(); check(list.size()==1 && list.get(0).getActive(),"active track before crash");
            File lineFile=new File(context.getNoBackupFilesDir(),"ride_tracks/"+list.get(0).getId()+".jsonl");
            Files.writeString(lineFile.toPath(),"{\"lat\":31.",StandardOpenOption.APPEND);
            System.out.println("seed-crash: real point file flushed; intentionally terminating without stop");
            Runtime.getRuntime().halt(0);
        }
        if(args[0].equals("recover")) {
            List<RideSummary> list=history(); check(list.size()==1,"interrupted track survives restart");
            RideSummary s=list.get(0);
            check(!s.getActive() && s.getInterrupted(),"recovery finalizes interrupted track");
            check(s.getPoints()==2,"recovers flushed point beyond old metadata; ignores truncated last line");
            check(s.getMeters()>30 && s.getMeters()<40,"recovery restores distance");
            check(preview(s.getId()).size()==2,"truncated JSON does not prevent preview");
            check(export(s.getId(),new File(root,"recovered.gpx"))==null,"recovered track exports");
            check(R.getLive()==null,"recovery never silently resumes tracking");
            return;
        }
        check(history().isEmpty(),"fresh install has no tracks");
        check(R.getEnabled(),"recording defaults enabled");
        R.start("导航","模拟路线",true); fix(31,121,0); R.stop(); check(history().isEmpty(),"simulation creates no ride");
        R.onFix(31,121,clock,clock,8,80); check(history().isEmpty(),"idle GPS creates no ride");
        R.start("导航","湖畔 & <桥>",false);
        R.onFix(0,0,clock,clock,8,20); check(history().isEmpty(),"invalid fixes create no empty session");
        clock+=2000;
        R.onFix(31.2304,121.4737,clock/1000,clock,8,35); // SDK 10 engines can return Unix seconds.
        fix(31.2307,121.4737,3000);
        fix(31.2310,121.4737,180000); // GPS gap: distance must not bridge this.
        RideSummary active=history().get(0);
        check(active.getStart()>1000000000000L,"old SDK seconds are stored as UTC milliseconds");
        check(active.getActive() && active.getPoints()==3,"real navigation records accepted fixes");
        check(active.getMeters()>30 && active.getMeters()<40,"GPS gap is excluded from distance");
        check(delete(active.getId())!=null,"cannot delete track still being recorded");
        check(history().size()==1,"failed delete preserves active data");
        List<RidePoint> preview=preview(active.getId());
        check(preview.size()==3 && preview.get(0).getSegment() && preview.get(2).getSegment(),"preview preserves GPS segments");
        R.stop(); RideSummary done=history().get(0);
        check(!done.getActive() && !done.getInterrupted(),"manual stop finalizes metadata");
        File output=new File(root,"track.gpx"); check(export(done.getId(),output)==null,"GPX writes successfully");
        DocumentBuilderFactory factory=DocumentBuilderFactory.newInstance(); factory.setNamespaceAware(true);
        Document doc=factory.newDocumentBuilder().parse(output);
        String ns="http://www.topografix.com/GPX/1/1";
        check(doc.getDocumentElement().getNamespaceURI().equals(ns),"valid GPX namespace");
        check(doc.getElementsByTagNameNS(ns,"name").item(0).getTextContent().equals("湖畔 & <桥>"),"XML names safely escaped");
        check(doc.getElementsByTagNameNS(ns,"trkseg").getLength()==2,"GPX keeps signal outage as two segments");
        NodeList pts=doc.getElementsByTagNameNS(ns,"trkpt"); check(pts.getLength()==3,"GPX keeps all points");
        Element pt=(Element)pts.item(0);
        double lat=Double.parseDouble(pt.getAttribute("lat")), lng=Double.parseDouble(pt.getAttribute("lon"));
        double[] gcj=MapGeo.INSTANCE.wgsToGcj(lat,lng);
        check(RideTrackMath.distance(gcj[0],gcj[1],31.2304,121.4737)<1,"GPX converts GCJ-02 to WGS84 within one metre");
        check(doc.getElementsByTagNameNS(ns,"time").item(0).getTextContent().endsWith("Z"),"GPX timestamps are UTC");
        check(export(done.getId(),new File(root,"missing/track.gpx"))!=null,"export storage failure reported");
        check(delete("../../outside")!=null,"unsafe record ID rejected");
        R.setEnabled(false); R.start("巡航","关闭开关",false); fix(31.24,121.47,3000);
        check(history().size()==1,"disabled recording creates no ride");
        check(!new Context(root).getSharedPreferences("motonav",0).getBoolean("record_rides_v1",true),"recording switch persisted");
        R.setEnabled(true); fix(31.24,121.47,3000); R.stop();
        check(history().size()==2,"enabling during cruise starts a fresh ride");
        String cruise=history().stream().filter(s->s.getMode().equals("巡航")).findFirst().get().getId();
        check(delete(cruise)==null && history().size()==1,"completed ride can be deleted");
        check(!new File(context.getNoBackupFilesDir(),"ride_tracks/"+cruise+".jsonl").exists(),"delete removes point file");
        // A fresh Context reloads prefs from disk, independent of object instances.
        FavoritePlaces f=FavoritePlaces.INSTANCE;
        f.save(context,"家","A",31.23,121.47); f.save(context,"公司","B",31.25,121.48);
        check(f.load(new Context(root)).size()==2,"favorites persist to disk");
        String home=f.find(context,31.23,121.47).getId();
        f.save(context,"我的家","A",31.230001,121.47);
        check(f.load(context).size()==2 && f.find(context,31.23,121.47).getId().equals(home),"nearby duplicate updates same identity");
        check(f.find(context,31.23,121.47).getName().equals("我的家"),"favorite rename persisted");
        boolean rejected=false; try {f.save(context,"", "",31,121);} catch(IllegalArgumentException e){rejected=true;}
        check(rejected && f.load(context).size()==2,"empty favorite name rejected without damaging list");
        f.remove(context,home); check(f.load(context).size()==1,"favorite delete independent of history");
        for(int i=0;i<49;i++) f.save(context,"地点"+i,"",30+i*0.01,120);
        rejected=false;try{f.save(context,"第51个","",29,119);}catch(IllegalArgumentException e){rejected=true;}
        check(rejected && f.load(context).size()==50,"favorites cap protects existing 50 entries");
        check(history().size()==1,"favorite changes leave ride history intact");
    }
}
