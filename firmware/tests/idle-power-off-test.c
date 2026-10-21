#include <assert.h>
#include <stdio.h>
#include "../boards/175/MotoNav/IdlePowerOff.h"

int main(void) {
  IdlePowerOffTimer t = {0, false};
  assert(!idlePowerOffDue(&t, 0, false, false));
  assert(!idlePowerOffDue(&t, 299999, false, false));
  assert(idlePowerOffDue(&t, 300000, false, false));

  /* A connected phone sending only idle heartbeats must not prevent shutdown. */
  idlePowerOffReset(&t, 100);
  for (uint32_t now = 100; now < 300100; now += 1000)
    assert(!idlePowerOffDue(&t, now,
      idlePowerOffNavigationActive(true, false, false, false), false));
  assert(idlePowerOffDue(&t, 300100, false, false));

  /* Navigation can run longer than the timeout; stopping starts a fresh period. */
  idlePowerOffReset(&t, 0);
  for (uint32_t now = 0; now <= 1800000; now += 1000)
    assert(!idlePowerOffDue(&t, now,
      idlePowerOffNavigationActive(true, true, false, false), false));
  assert(!idlePowerOffDue(&t, 2099999, false, false));
  assert(idlePowerOffDue(&t, 2100000, false, false));

  /* Arrival stops navigation for the timer even when inNav stays true. */
  assert(!idlePowerOffNavigationActive(true, true, false, true));
  assert(idlePowerOffNavigationActive(true, true, true, true));
  assert(idlePowerOffNavigationActive(true, false, true, false));
  assert(!idlePowerOffNavigationActive(false, true, true, false));
  assert(!idlePowerOffNavigationActive(false, false, false, false));

  /* Starting another route at the exact deadline cancels shutdown. */
  idlePowerOffReset(&t, 0);
  assert(!idlePowerOffDue(&t, 300000, true, false));
  assert(!idlePowerOffDue(&t, 599999, false, false));
  assert(idlePowerOffDue(&t, 600000, false, false));

  /* Cruise, call/pairing, and local interaction all protect current use. */
  idlePowerOffReset(&t, 0);
  assert(!idlePowerOffDue(&t, 900000,
      idlePowerOffNavigationActive(true, true, true, false), false));
  assert(!idlePowerOffDue(&t, 1200000, false, true));
  assert(!idlePowerOffDue(&t, 1499999, false, false));
  assert(idlePowerOffDue(&t, 1500000, false, false));

  /* Reconnect with navigation resumes; idle reconnect does not reset anything. */
  idlePowerOffReset(&t, 0);
  assert(!idlePowerOffDue(&t, 299999,
      idlePowerOffNavigationActive(true, true, false, false), false));
  assert(!idlePowerOffDue(&t, 300000,
      idlePowerOffNavigationActive(true, false, false, false), false));
  assert(idlePowerOffDue(&t, 599999, false, false));

  /* A short navigation session within one receive batch must be accounted for. */
  idlePowerOffReset(&t, 0);
  idlePowerOffReset(&t, 299990); /* accepted direction packet */
  idlePowerOffReset(&t, 299995); /* stop/disconnect transition */
  assert(!idlePowerOffDue(&t, 599994, false, false));
  assert(idlePowerOffDue(&t, 599995, false, false));

  /* StopWatch: 2 minutes without a BLE link powers off; linked/busy renews. */
  idlePowerOffReset(&t, 0);
  assert(!idleLinkOffDue(&t, 119999, false, false));
  assert(idleLinkOffDue(&t, 120000, false, false));
  idlePowerOffReset(&t, 0);
  assert(!idleLinkOffDue(&t, 900000, true, false));   /* linked: never due, renews */
  assert(!idleLinkOffDue(&t, 900000, false, true));   /* busy: renews at 900000 */
  assert(!idleLinkOffDue(&t, 1019999, false, false));
  assert(idleLinkOffDue(&t, 1020000, false, false));
  idlePowerOffReset(&t, UINT32_C(0xffff0000));
  assert(!idleLinkOffDue(&t, (uint32_t)(UINT32_C(0xffff0000) + 119999), false, false));
  assert(idleLinkOffDue(&t, (uint32_t)(UINT32_C(0xffff0000) + 120000), false, false));

  /* millis wrap and reset at timestamp zero are both valid, without sentinels. */
  idlePowerOffReset(&t, UINT32_C(0xffff0000));
  assert(!idlePowerOffDue(&t, (uint32_t)(UINT32_C(0xffff0000) + 299999), false, false));
  assert(idlePowerOffDue(&t, (uint32_t)(UINT32_C(0xffff0000) + 300000), false, false));
  idlePowerOffReset(&t, 0);
  assert(!idlePowerOffDue(&t, 299999, false, false));
  assert(idlePowerOffDue(&t, 300000, false, false));

  puts("PASS: native production timer, idle/heartbeat/navigation/cruise/arrival/reconnect/interaction/deadline/wrap scenarios");
  return 0;
}
