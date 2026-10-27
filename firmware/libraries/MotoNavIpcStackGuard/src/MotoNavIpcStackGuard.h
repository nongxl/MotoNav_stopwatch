#pragma once
#include <stdint.h>

struct MotoNavIpcStackStatus {
  uint32_t bytes[2];
  uint32_t freeMinimum[2];
};
// A snapshot, read only after BLE initialization in setup(). No heap allocation.
MotoNavIpcStackStatus motoNavIpcStackStatus();
