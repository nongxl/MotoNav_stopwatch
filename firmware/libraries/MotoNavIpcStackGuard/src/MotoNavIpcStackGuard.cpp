#include "MotoNavIpcStackGuard.h"
#include "IpcStackPolicy.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_attr.h>

// Plain BSS: IPC tasks are created before application C++ constructors run.
static TaskHandle_t ipcHandles[2];
static uint32_t ipcBytes[2];

extern "C" BaseType_t __real_xTaskCreatePinnedToCore(
    TaskFunction_t code, const char * const name, const uint32_t depth,
    void * const argument, UBaseType_t priority,
    TaskHandle_t * const handle, const BaseType_t core);

extern "C" BaseType_t IRAM_ATTR __wrap_xTaskCreatePinnedToCore(
    TaskFunction_t code, const char * const name, const uint32_t depth,
    void * const argument, UBaseType_t priority,
    TaskHandle_t * const handle, const BaseType_t core) {
  const uint32_t bytes=MotoNavIpcStack::requestedBytes(
      name,depth,priority,configMAX_PRIORITIES,core,
      reinterpret_cast<uintptr_t>(argument));
  const BaseType_t result=__real_xTaskCreatePinnedToCore(
      code,name,bytes,argument,priority,handle,core);
  if (result==pdPASS && bytes!=depth && handle) {
    ipcHandles[core]=*handle;
    ipcBytes[core]=bytes;
  }
  return result;
}

// Keep the hook's real-symbol reference alive even if a builder ignores
// library.properties ldflags. Missing --wrap must fail at link time, never
// silently generate a firmware that still has the original 1 KiB IPC stacks.
static auto volatile requiredHook=&__wrap_xTaskCreatePinnedToCore;

MotoNavIpcStackStatus motoNavIpcStackStatus() {
  MotoNavIpcStackStatus status={};
  if (!requiredHook) return status;
  for (unsigned i=0;i<2;++i) {
    status.bytes[i]=ipcBytes[i];
    if (ipcHandles[i]) status.freeMinimum[i]=uxTaskGetStackHighWaterMark(ipcHandles[i]);
  }
  return status;
}
