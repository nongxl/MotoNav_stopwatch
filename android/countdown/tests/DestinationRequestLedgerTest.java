import com.yuminaga.motonav.DestinationRequestLedger;

public final class DestinationRequestLedgerTest {
    private static void check(boolean ok,String message) { if(!ok)throw new AssertionError(message); }
    public static void main(String[] args) {
        DestinationRequestLedger g=new DestinationRequestLedger();
        check(g.claim("selected-A"),"Initial destination must plan");
        check(!g.claim("selected-A"),"Replayed launch Intent must not ask to replan");
        DestinationRequestLedger recreated=new DestinationRequestLedger();
        recreated.restore(g.snapshot());
        check(!recreated.claim("selected-A"),"Saved activity state must suppress original launch request");
        check(recreated.claim("selected-B"),"A deliberate new destination must still work");
        check(recreated.claim("selected-A-again-new-command"),"User may deliberately select the same place again");
        check(!recreated.claim("selected-B"),"Cancelling a request must not resurrect it on resume");
        recreated.restore(g.snapshot());
        check(!recreated.claim("selected-B"),"Merging saved state must retain newer receipts");
        check(!recreated.claim(null)&&!recreated.claim(""),"Invalid IDs must not enter ledger");
        for(int i=0;i<300;i++)recreated.claim("new-"+i);
        check(recreated.snapshot().length==128,"Bounded command memory");
        System.out.println("Destination command replay, restoration, deliberate reselection and bounds passed");
    }
}
