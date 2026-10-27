import com.yuminaga.motonav.SingleEngineLifecycle;

public class SingleEngineLifecycleTest {
    static void check(boolean value,String why){if(!value)throw new AssertionError(why);}
    public static void main(String[] args){
        SingleEngineLifecycle s=new SingleEngineLifecycle();s.reset();
        check(!s.event(39),"Arrival before acknowledged navigation must be ignored");
        check(s.begin(false),"Start real trip");
        check(!s.event(15)&&s.running(),"Destination-ahead icon must not finish the trip");
        s.event(307);check(s.rerouting(),"Yaw starts rerouting");
        s.event(305);check(!s.rerouting()&&s.running(),"Successful reroute continues same trip");
        s.event(307);s.event(306);check(!s.rerouting()&&s.running(),"Failed reroute is not arrival");
        check(s.event(39)&&s.arrived(),"Actual terminal event completes trip");
        check(!s.event(39)&&!s.running(),"Arrival is one-shot");
        check(!s.begin(false),"Late engine heartbeat must not reopen completed trip");
        s.reset();check(s.begin(true),"New simulated trip can begin");
        s.event(308);check(!s.arrived()&&!s.running(),"Manual exit is not arrival");
        check(!s.event(39),"Late terminal event after cancel ignored");
        s.reset();s.begin(false);s.event(9);check(!s.arrived()&&!s.running(),"Stopped engine is not arrival");
        System.out.println("Single engine lifecycle regression checks passed");
    }
}
