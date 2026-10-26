# 当前 1.75 通用固件：7.12.9-junction.11

[最新通用版、构建方式及验证范围](../docs/RELEASE-FIRMWARE-7.12.9-junction.11.md)。公开源码不含个人 Rebel 500 开机资源。1.85B 保持 7.12.3-185B。**最新成品：[v7.12.9-junction.11 Release](https://github.com/zyn950929/MotoNav/releases/tag/firmware-v7.12.9-junction.11)**。下方 7.12.1 下载是历史版本。

---

# 通用圆屏固件 7.12.1

适用微雪 ESP32-S3-Touch-AMOLED-1.75（466×466），Android 8.3.21 公开源码/分享打包版与 iOS 0.13.8 共用。源码位于 `boards/175/MotoNav`，本次融合不适用于 1.85B 或 M5Stack StopWatch。

本版以 7.12.0 为基础，保留 HID/AMS 音乐、歌词及旋转支持，合入 7.8.13 的五分钟无导航关机和关机崩溃重启修复。导航、巡航中不关机；普通心跳、天气和音乐更新不重置空闲计时。详细兼容范围及实测限制见 [更新说明](../docs/RELEASE-FIRMWARE-7.12.1.md)。

## 下载与安装

从 [firmware-v7.12.1 Release](https://github.com/zyn950929/MotoNav/releases/tag/firmware-v7.12.1) 下载成品。

Windows：完整解压 `MotoNav-1.75-v7.12.1-Universal-Flasher.zip`，双击“一键烧录.bat”，核对板型和 COM 号。默认升级保留设置；完整刷写会整片擦除。

Mac：下载 `MotoNav-universal-firmware-7.12.1.zip`，解压到仓库根目录，保留 `firmware/dist/175/panel0`。已安装 Arduino CLI / ESP32 Core 3.3.10 和 esptool 后，运行根目录 `./一键烧录固件.command --check` 检查，再双击或执行烧录入口。默认写应用到 `0x10000`，要求既有 16 MB / app3M_fat9M_16MB 分区；首次完整安装按原入口提示确认。

## 编译

安装 Arduino CLI 和 Espressif ESP32 Arduino Core 3.3.10：

```sh
python3 firmware/build.py
```

Windows 也可执行 `./firmware/build.ps1 -Board 175`，用 `-CliPath`、`-ConfigFile` 指定工具。镜像与大小/哈希清单位于 `firmware/dist/175/panel0`。清单的 `deviceTested: false` 保持保守标记，具体已完成和未完成的硬件验证以上述更新说明为准。

## 测试

```sh
for test in firmware/tests/*Test.cpp; do g++ -std=c++17 "$test" -o /tmp/motonav-test && /tmp/motonav-test || exit 1; done
cc firmware/tests/idle-power-off-test.c -o /tmp/motonav-idle && /tmp/motonav-idle
python3 -m unittest discover -s firmware/tests -p 'test_flash_macos.py'
```

`windows-flasher/` 保存独立包的脚本源码。`releases/7.12.1/` 保存本次已校验包的发布输入分片，发布流程校验并组装原 ZIP；日常下载请使用 Release 成品。

## 7.12.3 本次更新

四种独立地图风格及 1.75 外部供电保护详见 [7.12.3 说明](../docs/RELEASE-FIRMWARE-7.12.3.md)。Android 配套版本为 8.3.23-webmotor.4。iOS 源码/IPA 本次不变，既有 BLE 协议保留；不表示新增 Android 功能已在 iOS 实现。

`boards/185b` 提供 v7.12.3-185B 实验源码，PowerShell 构建可选 `-Board 185b -PanelVariant 0/1/2`；1.75 的外接电源保护未移植过去。不同板型禁止混刷。当前仅源码更新，历史 Release 下载不代表 7.12.3。
