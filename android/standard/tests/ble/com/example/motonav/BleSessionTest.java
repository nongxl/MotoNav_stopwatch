package com.example.motonav;

import static com.example.motonav.BleSession.Phase.*;

public final class BleSessionTest {
    private static int checks;
    private static void check(boolean pass, String name) {
        if (!pass) throw new AssertionError(name);
        checks++;
    }
    public static void main(String[] args) {
        BleSession<Object> session = new BleSession<>();
        Object first = new Object(), second = new Object();
        session.begin(first, 100);
        check(session.owns(first), "active client owns callbacks");
        check(!session.owns(second), "different client on same device is ignored");
        check(!session.expired(15099), "connect has bounded grace period");
        check(session.expired(15100), "connect expires even with no Android callback");
        check(!session.move(second, CONNECTING, SERVICES, 200, 12000), "old client cannot start discovery");
        check(session.move(first, CONNECTING, SERVICES, 200, 12000), "successful connection starts discovery");
        check(!session.move(first, CONNECTING, SERVICES, 9000, 12000), "duplicate callback cannot extend deadline");
        check(session.expired(12200), "discovery cannot hang indefinitely");
        check(session.move(first, SERVICES, NOTIFY, 13000, 12000), "subscribe before MTU negotiation");
        check(!session.move(first, MTU, READY, 13000, 0), "MTU callback cannot skip notification subscription");
        check(session.expired(25000), "missing CCCD callback expires");
        session.clear(first);
        check(!session.owns(first) && !session.expired(Long.MAX_VALUE), "closed client has no active deadline");
        session.begin(second, 30000);
        session.clear(first);
        check(session.owns(second), "late disconnect cannot clear replacement client");
        check(!session.move(first, CONNECTING, READY, 31000, 0), "late callback cannot ready replacement client");
        check(session.move(second, CONNECTING, SERVICES, 31000, 12000), "replacement can discover services");
        check(session.move(second, SERVICES, NOTIFY, 32000, 120000), "first pairing gets time to enter code");
        check(!session.expired(151999), "pairing dialog is not cut off after 12 seconds");
        check(session.expired(152000), "pairing still has a finite timeout");
        check(session.move(second, NOTIFY, MTU, 40000, 3000), "successful CCCD starts MTU");
        check(session.expired(43000), "missing MTU callback can use small-packet fallback");
        check(session.move(second, MTU, AUTH, 43000, 20000), "MTU fallback still requires authentication");
        check(session.expired(63000), "authentication cannot wait forever");
        check(session.move(second, AUTH, READY, 64000, 0), "confirmation completes handshake");
        check(!session.expired(Long.MAX_VALUE), "healthy ready link has no handshake timeout");
        check(BleSession.confirms("U,1", false), "explicit successful authentication");
        check(!BleSession.confirms("U,0", true), "negative authentication is never success");
        check(!BleSession.confirms("U,garbage", true), "malformed authentication is never success");
        check(!BleSession.confirms("CAP,BLD,1", false), "unsolicited capability is not a query response");
        check(BleSession.confirms("CAP,BLD,1", true), "authenticated query recovers missed initial U reply");
        check(!BleSession.confirms("CAP,HID,1", true), "other capability is not confirmation");
        check(!BleSession.confirms("S,12,60,15", true), "stats do not bypass confirmation");
        System.out.println("BLE session: " + checks + " checks passed");
    }
}
