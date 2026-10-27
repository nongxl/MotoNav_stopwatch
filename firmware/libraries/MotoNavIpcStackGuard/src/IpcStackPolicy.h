#pragma once
#include <stdint.h>

namespace MotoNavIpcStack {
constexpr uint32_t MinimumBytes = 4096;
// Match ESP-IDF's own IPC tasks, not any similarly named application task.
__attribute__((always_inline)) inline uint32_t requestedBytes(const char *name, uint32_t bytes,
                               unsigned priority, unsigned maxPriority,
                               int core, uintptr_t argument) {
  if (name && name[0]=='i' && name[1]=='p' && name[2]=='c' &&
      (core==0 || core==1) && name[3]=='0'+core && name[4]=='\0' &&
      priority+1==maxPriority && argument==static_cast<uintptr_t>(core) &&
      bytes<MinimumBytes) return MinimumBytes;
  return bytes;
}
}
