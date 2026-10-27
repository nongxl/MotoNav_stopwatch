package com.example.motonav;

public class ArrivalPolicyTest {
    private static int checks;
    private static void check(boolean value,String message) {
        checks++; if(!value)throw new AssertionError(message);
    }
    /** Mirrors only the documented legacy protocol boundary: A,* starts the arrival effect. */
    private static class Screen {
        String direction="S"; int celebrations; boolean effect;
        void receive(String next) {
            if(next.equals("A") && !direction.equals("A")) { celebrations++; effect=true; }
            if(!next.equals("A"))effect=false;
            direction=next;
        }
    }
    private static class Trip {
        final ArrivalPolicy policy=new ArrivalPolicy();
        final Screen screen=new Screen();
        long token=policy.prepare(); int savedTrips, statistics;
        void start(boolean simulated) { check(policy.begin(token,simulated),"trip starts"); }
        void info(String direction,int metres) {
            if(policy.acceptsGuidance(token)) screen.receive(ArrivalPolicy.guidanceDirection(direction));
        }
        void arrive(long epoch) {
            if(policy.arrive(epoch)) {statistics++;savedTrips++;screen.receive("A");}
        }
        void emulatorEnd(long epoch) { if(policy.isSimulation(epoch))arrive(epoch); }
    }
    public static void main(String[] args) {
        Screen old=new Screen();old.receive("A");
        check(old.celebrations==1,"baseline reproduces premature arrival even if A carries 800 metres");
        Trip t=new Trip();
        check(t.policy.isOpen(t.token),"route planning can receive SDK callbacks");
        check(!t.policy.acceptsGuidance(t.token),"route planning cannot emit navigation or end a trip");
        check(!t.policy.arrive(t.token),"arrival callback while planning ignored");
        t.start(true);
        check(t.policy.isSimulation(t.token),"simulation mode recorded");
        t.info("L",1200);t.info("S",900);
        for(int distance:new int[]{800,750,700,600,100,1,0}) {
            t.info("A",distance);
            check(t.screen.celebrations==0,"destination-ahead at "+distance+" m is not arrival");
        }
        check(t.policy.acceptsGuidance(t.token),"predicted zero distance never ends trip");
        t.emulatorEnd(t.token);
        check(t.screen.celebrations==1 && t.screen.effect,"real emulator end triggers one celebration");
        check(t.savedTrips==1 && t.statistics==1,"one finalization and one statistics event");
        check(!t.policy.acceptsGuidance(t.token),"end state ignores queued guidance");
        t.info("R",100);t.info("A",0);t.info("S",0);
        check(t.screen.effect && t.screen.direction.equals("A"),"late updates cannot bounce screen back to navigation");
        t.arrive(t.token);t.emulatorEnd(t.token);
        check(t.savedTrips==1 && t.screen.celebrations==1,"arrival plus emulator-end duplicates are idempotent");
        check(!t.policy.begin(t.token,true),"late SDK onStart cannot reopen completed trip");
        long previous=t.token;
        t.token=t.policy.prepare();
        check(t.token!=previous && t.policy.isOpen(t.token),"explicit new route creates a fresh generation");
        check(!t.policy.isCurrent(previous),"old listener invalidated");
        t.start(false);
        t.policy.cancel(previous);
        check(t.policy.acceptsGuidance(t.token),"old page exit cannot cancel new navigation");
        t.arrive(previous);
        check(t.savedTrips==1,"old arrival cannot finish new navigation");
        check(!t.policy.begin(previous,true),"old start cannot alter current mode");
        check(!t.policy.isSimulation(t.token),"current real-navigation mode retained");
        t.emulatorEnd(t.token);
        check(t.savedTrips==1,"emulator-end callback cannot complete real navigation");
        t.info("L",100);
        check(t.screen.direction.equals("L"),"new navigation accepts normal directions");
        t.arrive(t.token);
        check(t.savedTrips==2 && t.screen.celebrations==2,"new genuine arrival works independently");

        Trip cancelled=new Trip();cancelled.start(true);
        cancelled.policy.cancel(cancelled.token); // This happens before SDK stopNavi.
        cancelled.emulatorEnd(cancelled.token);cancelled.arrive(cancelled.token);cancelled.info("A",0);
        check(cancelled.screen.celebrations==0,"manual stop synchronous emulator callback never celebrates");
        check(!cancelled.policy.isOpen(cancelled.token),"cancelled callback stream remains closed");
        check(!cancelled.policy.begin(cancelled.token,true),"resume callback cannot reopen cancelled trip");

        Trip quick=new Trip();quick.start(false);quick.arrive(quick.token);
        check(quick.savedTrips==1,"genuine arrival before first guidance is allowed after explicit start");
        Trip reroute=new Trip();reroute.start(true);
        check(!reroute.policy.begin(reroute.token,true),"reroute start callback does not start a duplicate session");
        check(reroute.policy.acceptsGuidance(reroute.token),"same-session rerouting still accepts guidance");
        for(String direction:new String[]{"S","L","R","SL","SR","U"})
            check(ArrivalPolicy.guidanceDirection(direction).equals(direction),"ordinary direction preserved: "+direction);
        System.out.println("ArrivalPolicyTest: "+checks+" checks passed");
    }
}
