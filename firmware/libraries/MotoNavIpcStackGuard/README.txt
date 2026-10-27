MotoNav IPC startup stack guard

Keep this entire Arduino library with the firmware source. Build using firmware/build.ps1.
The build script explicitly adds -Wl,--wrap=xTaskCreatePinnedToCore. The Arduino CLI build on this machine did not apply library.properties ldflags for this source-only library; do not rely on that metadata alone.
The retained requiredHook reference makes missing linker configuration fail with an undefined __real_xTaskCreatePinnedToCore symbol rather than silently omit the fix.

Scope: ESP-IDF's ipc0 / ipc1 task names, matching CPU index, argument and maximum priority only; a minimum 4096 bytes. Larger requested stacks and all other tasks are passed through unchanged. No scheduler priority, affinity, watchdog or Bluetooth behavior changes.
Original fault: ESP32 Arduino 3.3.10, default 1024-byte IPC stack, real ipc0 stack overflow during BLE controller interrupt allocation. The diagnosis is summarized in repository verification notes; private device memory and logs are not published.
Runtime status after BLE setup prints allocated sizes and minimum remaining stack bytes. The patch uses plain static storage, no app-side heap allocation and no C++ dynamic initialization at the early IPC creation stage.
