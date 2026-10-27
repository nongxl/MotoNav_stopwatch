import com.example.motonav.QWeatherClient;
import org.json.*;
import java.time.*;

public class QWeatherClientTest {
  static int checks;
  static void check(boolean ok, String message) { checks++; if(!ok) throw new AssertionError(message); }
  static JSONObject object(String text) { return new JSONObject(text); }
  public static void main(String[] args) {
    var client = QWeatherClient.INSTANCE;
    check(client.normalizeHost("https://example.xy.qweatherapi.com/").equals("example.xy.qweatherapi.com"), "host normalization");
    for(String host: new String[]{"evil.test", "example.qweatherapi.com.evil.test", "https://name:secret@example.qweatherapi.com", "example.qweatherapi.com/path"}) {
      try { client.normalizeHost(host); throw new AssertionError("unsafe host accepted"); }
      catch(IllegalArgumentException expected) { checks++; }
    }
    var current = object("{\"condition\":{\"text\":\"小雨\",\"code\":\"305\"},\"temperature\":{\"value\":21.6},\"humidity\":0.73,\"wind\":{\"direction\":{\"degree\":45},\"scale\":3}}");
    String future = OffsetDateTime.now().plusHours(1).withMinute(0).withSecond(0).withNano(0).toString();
    var hour = object("{\"hours\":[{\"forecastTime\":\""+future+"\",\"condition\":{\"text\":\"小雨\"},\"temperature\":{\"value\":22},\"precipitation\":{\"probability\":0.67}}]}");
    var rain = object("{\"summary\":\"30分钟后雨停\",\"minutely\":[{\"precip\":\"0.15\"},{\"precip\":\"0\"},{}]}");
    var air = object("{\"indexes\":[{\"code\":\"us-epa\",\"name\":\"美国AQI\",\"aqi\":90},{\"code\":\"cn-mee\",\"name\":\"中国AQI\",\"aqi\":45,\"aqiDisplay\":\"45\",\"category\":\"优\"}]}");
    var alerts = object("{\"alerts\":[{\"headline\":\"已解除\",\"messageType\":{\"code\":\"cancel\"}},{\"headline\":\"过期\",\"expireTime\":\"2020-01-01T00:00Z\"},{\"headline\":\"大风橙色预警\",\"severity\":\"severe\",\"description\":\"阵风\",\"instruction\":\"注意防风\"},{\"headline\":\"高温\",\"severity\":\"moderate\"}]}");
    var result = client.parse(current,hour,rain,air,alerts,"","来源");
    check(result.getTemperature()==22, "rounded temperature");
    check(result.getHumidity().equals("湿度 73%"), "fractional humidity");
    check(result.getWind().equals("东北风 3级"), "wind direction");
    check(result.getHours().size()==1 && result.getHours().get(0).getProbability()==67, "hourly precipitation fraction");
    check(result.getRainBars().get(0)==15 && result.getRainBars().get(1)==0 && result.getRainBars().get(2)==-1, "rain: mm/5m and missing values");
    check(result.getAqi()==45 && result.getAir().contains("中国AQI"), "preserve AQI standard");
    check(result.getWarningCount()==2 && result.getWarningLevel()==3 && result.getWarningTitle().contains("大风"), "active alert lifecycle and severity");
    var absent = client.parse(current,null,null,null,null,"部分服务失败", "来源");
    check(absent.getWarningCount()==-1 && absent.getWarningTitle().contains("暂不可用"), "failure is not no-alert");
    check(absent.getAqi()==-1 && absent.getRainBars().isEmpty(), "missing optional services");
    var empty = client.parse(current,null,null,null,object("{\"metadata\":{\"zeroResult\":true}}"),"", "来源");
    check(empty.getWarningCount()==0, "confirmed zero result");
    var unknownHour = client.parse(current,object("{\"hours\":[{\"forecastTime\":\""+future+"\",\"temperature\":{\"value\":20}}]}"),null,null,null,"", "来源");
    check(unknownHour.getHours().get(0).getProbability()==-1, "missing probability is unknown");
    try { client.parse(object("{\"condition\":{\"text\":\"晴\"},\"temperature\":{\"value\":500}}"),null,null,null,null,"",""); throw new AssertionError("invalid temp accepted"); }
    catch(IllegalArgumentException expected) { checks++; }
    check(client.iconOf("雷阵雨",false)==5 && client.iconOf("雨夹雪",false)==8 && client.iconOf("晴",true)==10,"screen weather icons");
    if(args.length > 0) {
      try {
        var directory = java.nio.file.Path.of(args[0]);
        var actual = client.parse(
          object(java.nio.file.Files.readString(directory.resolve("current.json"))),
          object(java.nio.file.Files.readString(directory.resolve("hourly.json"))),
          object(java.nio.file.Files.readString(directory.resolve("rain.json"))),
          object(java.nio.file.Files.readString(directory.resolve("air.json"))),
          object(java.nio.file.Files.readString(directory.resolve("alerts.json"))), "", "QWeather");
        check(actual.getHours().size()==5, "live response: five future hours");
        check(actual.getAqi()>=0, "live response: AQI");
        check(!actual.getRainBars().isEmpty(), "live response: minutely chart");
        check(actual.getWarningCount()>=0, "live response: valid alert status");
        System.out.println("Live API payloads parsed successfully (public Beijing sample)");
      } catch(java.io.IOException error) { throw new RuntimeException(error); }
    }
    System.out.println("QWeather parser: "+checks+" checks passed");
  }
}
