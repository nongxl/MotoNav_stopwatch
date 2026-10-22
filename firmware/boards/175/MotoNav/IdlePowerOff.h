#ifndef MOTONAV_IDLE_POWER_OFF_H
#define MOTONAV_IDLE_POWER_OFF_H

#include <stdint.h>
#include <stdbool.h>

/* Same timer is compiled into the firmware and the native C regression test. */
#define IDLE_POWER_OFF_MS UINT32_C(300000)

typedef struct {
  uint32_t since;
  bool armed;
} IdlePowerOffTimer;

typedef struct {
  uint32_t checkedAt;
  bool sampled;
  bool batteryOnly;
} IdlePowerSupply;

static inline void idlePowerSupplyUpdate(IdlePowerSupply *supply, uint32_t now,
                                         int axpStatus1) {
  supply->checkedAt = now;
  supply->sampled = true;
  /* STATUS1 bit 5: VBUS-good. Full batteries still have this bit set on USB. */
  supply->batteryOnly = axpStatus1 >= 0 && axpStatus1 <= 255 &&
                        (axpStatus1 & 0x20) == 0;
}

static inline bool idlePowerSupplyAllowsOff(const IdlePowerSupply *supply,
                                           uint32_t now) {
  return supply->sampled && supply->batteryOnly &&
         (uint32_t)(now - supply->checkedAt) <= 1500;
}

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

static inline bool idlePowerOffDueOnBattery(IdlePowerOffTimer *timer, uint32_t now,
                                           bool navigating, bool interacting,
                                           const IdlePowerSupply *supply) {
  return idlePowerOffDue(timer, now, navigating,
                        interacting || !idlePowerSupplyAllowsOff(supply, now));
}

#endif
