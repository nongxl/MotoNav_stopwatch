import com.yuminaga.motonav.ProviderStartup;
public final class ProviderStartupChoiceTest {
    static void check(boolean v,String label){if(!v)throw new AssertionError(label);}
    public static void main(String[] args){
        ProviderStartup s=new ProviderStartup();s.begin(1000,true);
        check(s.next(1000,false,false)==ProviderStartup.WARM,"warm");
        check(s.next(2000,true,false)==ProviderStartup.PLAN,"plan");s.calculated();
        check(s.next(3000,true,false)==ProviderStartup.NONE,"no automatic start");
        check(s.next(190000,true,false)==ProviderStartup.NONE,"user may take longer than 90 seconds");
        check(s.choosing()&&!s.acceptsGuide()&&s.starts()==0,"no navigation during preview");
        s.confirm(200000);check(!s.choosing(),"confirmed");
        check(s.next(200000,true,false)==ProviderStartup.START,"start selected path");
        s.next(200500,true,true);check(s.running(),"actual guide acknowledgement");
        check(s.next(600000,true,false)==ProviderStartup.NONE,"no restart when stationary");
        s.begin(1000);check(s.next(1000,false,false)==ProviderStartup.WARM,"legacy warm");
        check(s.next(2000,true,false)==ProviderStartup.PLAN,"legacy plan");s.calculated();
        check(s.next(3000,true,false)==ProviderStartup.START,"legacy auto start retained");
        s.begin(1000,true);s.next(1000,false,false);s.next(2000,true,false);s.calculated();s.planFailed();
        check(!s.calculatedRoute(),"failed calculation clears ready");
        check(s.next(3000,true,false)==ProviderStartup.PLAN,"retry calculation");
        s.stop();check(!s.choosing()&&s.next(4000,true,true)==ProviderStartup.NONE,"cancel");
        System.out.println("ProviderStartup choice and existing automatic startup passed");
    }
}
