#pragma once
#include <stdint.h>

// Interpolate only toward a received distance. Never extrapolate vehicle motion.
struct TurnRingMotion {
  float from = 0, target = 0;
  uint32_t started = 0, duration = 0, lastPacket = 0, cadence = 1000;
  bool received = false;

  static float bounded(float value) {
    return value < 0 ? 0 : value > 1 ? 1 : value;
  }
  float value(uint32_t now) const {
    const uint32_t elapsed = now - started;
    if (!duration || elapsed >= duration) return target;
    return from + (target - from) * (float(elapsed) / duration);
  }
  void update(float shown, float next, bool reset, uint32_t now) {
    const uint32_t gap = now - lastPacket;
    if (received && gap >= 150 && gap <= 3000)
      cadence = gap < 200 ? 200 : gap > 2000 ? 2000 : gap;
    reset = reset || !received || gap > 3000;
    lastPacket = now;
    received = true;
    next = bounded(next);
    // Repeated packets must not restart an in-flight transition.
    if (!reset && next == target) return;
    // A substantial distance increase indicates a new approach/recalculation.
    if (next < target - 0.04f) reset = true;
    from = reset ? next : bounded(shown);
    target = next;
    started = now;
    duration = reset ? 0 : cadence;
  }
};
