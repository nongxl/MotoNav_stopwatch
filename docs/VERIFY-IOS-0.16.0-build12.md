# iOS 0.16.0 Build12 与 pages.5 验证记录

记录日期：2026-10-31。以下结果按具体构建与测试区分，不将编译/桌面回归当作实体道路验证。

| 项目 | 已完成的检查 | 证据 |
| --- | --- | --- |
| iOS Build12 | 主应用和分享扩展 arm64 Release 编译；版本 0.16.0/12、图标 1024² 不透明 RGB、编译后 AppIcon 索引、ZIP 完整性与已知个人配置扫描 | [构建 41196444092](https://github.com/zyn950929/MotoNav/actions/runs/41196444092)，[源码 68147b5](https://github.com/zyn950929/MotoNav/tree/68147b592baf83909790f79528602a3ce9858cfc) |
| Build11 功能回归 | 自行车路径、到达、历史、Cookie 隔离、时间、媒体元数据、风格分类、地图交接、BLE 流控与确认、记录导出 | [构建 41194675772](https://github.com/zyn950929/MotoNav/actions/runs/41194675772)；Build12 Swift 与 Build11 逐文件相同 |
| 楼块连续性 | 实际配套 RasterDecoder 桌面测试：复现旧闪失，连续六次基础图替换、1200² 像素/600 米移动、道路水面优先、关闭后不复活、坐标/分辨率/风格/调色板隔离、坏包保留 | [可运行测试](../ios/tests/building-continuity/main.cpp)及[同一构建日志](https://github.com/zyn950929/MotoNav/actions/runs/41194675772)；测试 fixture 与 pages.5 相应源码一致 |
| 导出窗口 | Build8 原生 iPhone 模拟器首次打开分享/文件保存页；文件绑定及旧记录恢复回归 | [构建 41186574034](https://github.com/zyn950929/MotoNav/actions/runs/41186574034)；导出组件随后保持相同 |
| 1.75 pages.5 | ESP32 Core 3.3.10 完整编译，应用/完整镜像、0x10000 偏移、与 pages.4 相同分区；独立烧录包 VerifyOnly 通过，未访问串口 | [固件源码和说明](RELEASE-FIRMWARE-7.12.10-pages.5.md) |

用户曾确认 Build9 底图衔接改善，Build10 楼块能出现但反复消失。Build11 的后续“还是一样”反馈，用户随后明确尚未升级固件，因此不能视为 pages.5 实测结果。

以下尚未完成：Build11/12 与 pages.5 的实体组合、安卓手机配合 pages.5 的实机复测、iPhone 长时间锁屏、实际道路、功耗/温度/续航、ESP32 新收图合并耗时。桌面日志中的毫秒值是宿主机计时；延迟回放的可写窗口为模型，均不代表该手机或 ESP32 的实测性能。

记录导出默认关闭，仅用户手动触发；不自动上传。本次仅同步源码、合成测试及数值结论，不发布用户原始日志、绝对位置、个人 Key、Cookie、服务 Host 或签名文件。

