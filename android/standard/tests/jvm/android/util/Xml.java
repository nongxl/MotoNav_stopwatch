package android.util;
import org.xmlpull.v1.XmlSerializer;
import javax.xml.stream.*;
import java.lang.reflect.*;
import java.io.*;
/** JVM XML writer behind the same serializer calls as Android. */
public class Xml {
    public static XmlSerializer newSerializer() {
        return (XmlSerializer) Proxy.newProxyInstance(XmlSerializer.class.getClassLoader(),new Class[]{XmlSerializer.class},new InvocationHandler() {
            XMLStreamWriter out;
            public Object invoke(Object proxy,Method method,Object[] a) throws Exception {
                switch(method.getName()) {
                    case "setOutput": out=XMLOutputFactory.newFactory().createXMLStreamWriter((OutputStream)a[0],(String)a[1]); break;
                    case "startDocument": out.writeStartDocument((String)a[0],"1.0"); break;
                    case "startTag": out.writeStartElement((String)a[1]); break;
                    case "attribute": out.writeAttribute((String)a[1],(String)a[2]); break;
                    case "text": out.writeCharacters((String)a[0]); break;
                    case "endTag": out.writeEndElement(); break;
                    case "endDocument": out.writeEndDocument(); break;
                    case "flush": out.flush(); break;
                    default: throw new UnsupportedOperationException(method.getName());
                }
                return method.getReturnType()==XmlSerializer.class ? proxy : null;
            }
        });
    }
}
