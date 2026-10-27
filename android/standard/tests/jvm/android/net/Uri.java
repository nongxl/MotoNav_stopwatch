package android.net;

// Test-only JVM adapter; production uses Android's Uri implementation.
import java.net.*;
import java.nio.charset.StandardCharsets;
import java.util.*;

public final class Uri {
    private final String value;
    private Uri(String value) { this.value=value; }
    public static Uri parse(String value) { return new Uri(value); }
    private URI uri() { return URI.create(value.replace("[","%5B").replace("]","%5D").replace("|","%7C")); }
    public String getScheme(){return uri().getScheme();}
    public String getHost(){return uri().getHost();}
    public String getPath(){return uri().getPath();}
    public String getQueryParameter(String key) {
        String q=uri().getRawQuery();if(q==null)return null;
        for(String pair:q.split("&")) {
            String[] p=pair.split("=",2);
            if(URLDecoder.decode(p[0],StandardCharsets.UTF_8).equals(key))
                return p.length==2?URLDecoder.decode(p[1],StandardCharsets.UTF_8):"";
        }
        return null;
    }
    public Builder buildUpon(){return new Builder(value);}
    @Override public String toString(){return value;}
    public static final class Builder {
        private String text;
        Builder(String value){text=value;}
        public Builder appendQueryParameter(String key,String value) {
            text+=(text.contains("?")?"&":"?")+URLEncoder.encode(key,StandardCharsets.UTF_8)+"="+URLEncoder.encode(value,StandardCharsets.UTF_8);
            return this;
        }
        public Uri build(){return Uri.parse(text);}
    }
}
