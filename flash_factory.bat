@echo off
chcp 65001 >nul
echo ======================================================================
echo  MotoNav Stopwatch: 出厂固件一键烧录工具 (Offset: 0x0000)
echo ======================================================================

set ESPTOOL="D:\GreenPrograms\M5Burner-v3-beta-win-x64\packages\tool\esptool.exe"
if not exist %ESPTOOL% (
    echo [警告] 未检测到 %ESPTOOL%，尝试使用系统 esptool...
    set ESPTOOL=esptool.exe
)

set BIN_FILE="release\MotoNav_stopwatch_factory_0x0000.bin"
if not exist %BIN_FILE% (
    echo [提示] 未检测到合并固件，正在自动生成...
    "%USERPROFILE%\.platformio\penv\Scripts\python.exe" tools\merge_firmware.py --env stopwatch
)

set PORT=COM8
set /p USER_PORT="请输入设备串口号 [默认 COM8，直接回车即用]: "
if not "%USER_PORT%"=="" set PORT=%USER_PORT%

echo.
echo 正在使用 %ESPTOOL% 烧录至 %PORT% (0x0000)...
%ESPTOOL% --chip esp32s3 --port %PORT% --baud 921600 write_flash 0x0000 %BIN_FILE%

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ======================================================================
    echo  [成功] 固件烧录完毕！设备已自动重启进入导航系统！
    echo ======================================================================
) else (
    echo.
    echo [错误] 烧录失败，请检查串口是否被其他终端/监视器占用！
)
pause
