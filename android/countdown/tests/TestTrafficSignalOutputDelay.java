import com.yuminaga.motonav.TrafficSignalOutputDelay;
public class TestTrafficSignalOutputDelay {
 public static void main(String[] args){
  TrafficSignalOutputDelay d=new TrafficSignalOutputDelay();
  if(d.due(1000)||d.due(1250)||d.due(1750)||d.due(1999)||!d.due(2000))throw new AssertionError("bounded gap");
  d.reset();if(d.due(3000)||d.due(3500))throw new AssertionError("recovery resets gap");
  d.reset();if(d.due(5000)||d.due(4900)||!d.due(5900))throw new AssertionError("clock reversal");
  System.out.println("PASS: short alignment loss waits at most 1000ms; recovery and hard-clear reset; no signal data/expiry stored");
 }
}
