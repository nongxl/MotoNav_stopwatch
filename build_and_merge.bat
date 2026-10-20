@echo off
chcp 65001 >nul
echo ======================================================================
echo  MotoNav Stopwatch: 固件编译并合并生成 M5Burner 发布包
echo ======================================================================

set PIO_EXE="%USERPROFILE%\.platformio\penv\Scripts\platformio.exe"
if exist %PIO_EXE% (
    set "CMD_PIO=%PIO_EXE%"
) else (
    set "CMD_PIO=pio"
)

set PY_EXE="%USERPROFILE%\.platformio\penv\Scripts\python.exe"
if not exist %PY_EXE% (
    set "PY_EXE=python"
)

echo [1/2] 正在编译 M5Stack Stopwatch 固件...
%CMD_PIO% run -e stopwatch
if %ERRORLEVEL% NEQ 0 (
    echo [错误] 编译失败，请检查编译日志！
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo [2/2] 正在合并出厂全量固件 (.bin) 供 M5Burner / Web 刷机发布...
%PY_EXE% tools\merge_firmware.py --env stopwatch
if %ERRORLEVEL% NEQ 0 (
    echo [错误] 合并固件失败！
    pause
    exit /b %ERRORLEVEL%
)

echo.
echo ======================================================================
echo  [完成] 固件已保存至 release/ 目录：
echo  - MotoNav_stopwatch_factory_0x0000.bin  (可直接发布在 M5Burner 从 0x0000 烧录)
echo  - MotoNav_stopwatch_app_ota_0x10000.bin (仅主程序，用于 OTA / 0x10000 烧录)
echo ======================================================================
pause
