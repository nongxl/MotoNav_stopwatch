#ifndef MOTONAV_IDLE_POWER_OFF_H
#define MOTONAV_IDLE_POWER_OFF_H

#include <stdint.h>
#include <stdbool.h>

/* Same timer is compiled into the firmware and the native C regression test. */
#define IDLE_POWER_OFF_MS UINT32_C(300000)
/* StopWatch：断连（no link）满 2 分钟 → 真关机（M5PM1 shutdown，PWR 键再开机）。 */
#define LINK_OFF_POWER_OFF_MS UINT32_C(120000)

typedef struct {
  uint32_t since;
  bool armed;
} IdlePowerOffTimer;

static inline void idlePowerOffReset(IdlePowerOffTimer *timer, uint32_t now) {
  timer->since = now;
  timer->armed = true;
}

static inline bool idlePowerOffNavigationActive(bool connected, bool inNav,
                                                bool cruise, bool arrived) {
  return connected && (cruise || (inNav && !arrived));
}

static inline bool idlePowerOffDue(IdlePowerOffTimer *timer, uint32_t now,
                                   bool navigating, bool interacting) {
  if (!timer->armed || navigating || interacting) {
    idlePowerOffReset(timer, now);
    return false;
  }
  /* Unsigned subtraction also handles the approximately 49-day millis wrap. */
  return (uint32_t)(now - timer->since) >= IDLE_POWER_OFF_MS;
}

/* StopWatch：链接断开连续 2 分钟即到期；有链接或忙（配对/来电/滑页）时续期。 */
static inline bool idleLinkOffDue(IdlePowerOffTimer *timer, uint32_t now,
                                  bool linked, bool busy) {
  if (!timer->armed || linked || busy) {
    idlePowerOffReset(timer, now);
    return false;
  }
  return (uint32_t)(now - timer->since) >= LINK_OFF_POWER_OFF_MS;
}

#endif
