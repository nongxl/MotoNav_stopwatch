# MotoNav

Android / iOS 手机配合 ESP32-S3 圆屏的导航项目。当前 Android App **8.3.23-webmotor.4（网页摩托车实验版）**，微雪 1.75 固件 **7.12.3**。iOS 继续沿用 **0.13.8**，本次不改 iOS 源码、构建流程或 IPA，也不发布 fresh 工程。

iOS 工程已整合到 [ios/](ios/README.md)，包含 Xcode 工程、分享扩展、构建说明和功能对照。iOS 已移植高德截图底图、建筑处理和沿线离线地图；播放器支持和后台取图能力与 Android 存在差异，详见该说明。

**本项目借助 AI 辅助开发，由个人进行需求设计、设备验证和迭代。仅供个人非商业使用与学习交流，禁止未经许可的商业使用及转售。**

![MotoNav 圆屏摩托车导航模拟效果](docs/media/mockups/hero-simulation.png)

**上图为 AI 模拟效果，非实机截图。** 手机负责定位、路线与数据处理，圆屏展示导航、天气和音乐控制信息。

## 本次源码更新（2026-10-21）

- **Android 8.3.23-webmotor.4**：五个独立页面，首页靠前的巡航入口；新增高德网页摩托车规划、原生路线选择/导航/模拟、偏航后沿用摩托车方式重算。详见 [Android 更新与使用说明](docs/RELEASE-ANDROID-8.3.23-webmotor.md)。
- **地图统一高德**：移除 Android 的天地图与 OSM 调用，仅保留罪恶都市、原版、极简黑白、龙珠雷达四种独立风格。原版使用高德 SDK 初始地图截图；圆屏图像受调色板和传输分辨率限制。极简黑白保留道路网和导航线，去掉建筑、绿地、水面与文字。
- **固件 7.12.3**：配套四风格的原色显示与协议处理；1.75 合入插电时不自动关机、拔电后重新计满五分钟的处理，保留 iOS HID/AMS/歌词/旋转协议。详见 [固件更新与兼容说明](docs/RELEASE-FIRMWARE-7.12.3.md)。
- 本次是源码发布，**不新增 APK、IPA、刷机 ZIP 或固件二进制 Release**。已有历史 Release 保留原版本。1.85B 的对应移植源码一并放在原有目录，属于实验适配，不能刷入 1.75 镜像。

网页摩托车功能使用高德网页当前返回的路线数据，并非已获得正式摩托车导航 SDK 权限。网页接口、登录或验证变化可能导致不可用；本项目不保证完整避开禁摩区，也不提供车道、电子眼、限速牌或红绿灯倒计时。当前修订已完成编译和自动检查，尚未完成实车道路、锁屏导航与新五页面全流程验证。详情及验证边界见更新说明。

## 圆屏固件 7.12.1（2026-10-18，历史版本）

融合 GitHub 7.12.0 的 iOS HID/AMS、歌词和旋转支持，以及 7.8.13 的五分钟无导航关机与防重启修复。修复了旧 7.8.12 在关机时释放蓝牙对象、被迟到断连事件访问后崩溃重启的问题；建议使用该版本的 1.75 用户升级。

[固件 7.12.1 下载与更新说明](https://github.com/zyn950929/MotoNav/releases/tag/firmware-v7.12.1) · [详细更新说明](docs/RELEASE-FIRMWARE-7.12.1.md)

Windows 下载独立的一键烧录 ZIP，完整解压后双击“一键烧录.bat”；默认升级保留设置，首次完整刷写会整片擦除。仅适用于微雪 1.75，不适用于 1.85B 或 M5Stack StopWatch。

已对照 Android 8.3.21 公开源码/分享打包版与 iOS 0.13.8 核对协议，504 项音乐/旋转检查、关机计时和烧录工具检查通过。7.12.1 在实物上完成启动与接 USB 五分钟关机观察；7.8.13 的纯电池五分钟关机已由用户确认。融合版的纯电池、两端手机交替连接和道路测试仍待验证。旧个人直装 Android 8.3.21 缺少切换 iPhone 后复位音乐模式的处理，公开源码/分享包已包含该处理。

## iOS 0.13.8 到达动画修复（2026-10-18）

修复 iOS 将“前方终点”当成真正到达、导致圆屏提前播放到达动画的问题；同时限制重复结束、手动退出和迟到回调。维护者认为这是一个比较重大的 bug，建议旧版用户更新。详见 [iOS 0.13.8 更新说明](docs/RELEASE-IOS-0.13.8.md)。新版 IPA 由 GitHub 的 macOS 构建流程生成，构建成功后出现在 Releases；仍需用自己的 Apple 账号签名安装。

## Android 8.3.21 重要更新（2026-10-18）

**维护者说明：我认为这是一个比较重大的 bug。** 旧版在距离终点约七八百米、仍处于导航时，小屏可能提前显示“已到达”的结算画面，随后又回到导航，容易误导使用者。8.3.21 修复了这个问题：前方终点提示继续作为正常导航，只有本次导航真正结束时才发送一次到达结算。已使用旧版的 Android 用户建议更新。

这次也同步公开 8.3.19 之后新增的**地图加载状态提示、骑行轨迹记录与 GPX 导出、常用目的地**。入口、使用方法、验证范围及限制见 [Android 8.3.21 更新说明](docs/RELEASE-ANDROID-8.3.21.md)。本次更新 Android 源码与说明，iOS 和圆屏固件版本沿用仓库现有版本。

## 交通安全与使用风险

**使用本套程序时，请遵守交通法规。使用本套程序，默认使用者已知悉并自愿承担使用过程中可能带来的交通事故及其他风险。** 行驶中请勿操作手机或圆屏，设置与查看记录请先安全停车；始终以实际道路、交通信号、标志和现场情况为准，不得仅凭本程序作驾驶判断。

本项目仍可能存在定位、路线、地图、通信或显示错误，不保证导航信息准确、及时、完整。上述风险提示不排除适用法律规定的责任，也不意味着能免除依法不能免除的责任。

## 装到车上是什么样

<img src="docs/media/photos/installation.jpg" width="560" alt="MotoNav 圆屏安装实拍，车辆停放状态">

用户提供的停车安装实拍。小圆屏可固定在车把附近，具体支架、供电和安装方式由使用者按车辆情况安排。

## 实机画面

| 导航与路线 | 音乐控制 | 降水与空气 |
| :---: | :---: | :---: |
| <img src="docs/media/photos/navigation.jpg" width="260" alt="桌面导航实拍"> | <img src="docs/media/photos/music.jpg" width="260" alt="音乐控制实拍"> | <img src="docs/media/photos/rain-air.jpg" width="260" alt="降水与空气质量实拍"> |
| 地图、转向距离、车速及行程环；桌面演示 | 查看曲目，控制上一首、播放/暂停、下一首 | 查看短时降水趋势及空气质量 |

| 天气主页 | 天气预警 | 启动就绪 |
| :---: | :---: | :---: |
| <img src="docs/media/photos/weather-privacy-edited.png" width="260" alt="天气照片经 AI 替换区域名称"> | <img src="docs/media/photos/alerts.jpg" width="260" alt="暂无有效天气预警实拍"> | <img src="docs/media/photos/ready-privacy-edited.png" width="260" alt="启动照片经 AI 替换设备后缀"> |
| 实况与未来五小时预报；**照片经 AI 隐私处理，地名已替换** | 拍摄时显示暂无有效预警 | **照片经 AI 隐私处理，设备后缀已替换** |

天气照片中的数据仅代表拍摄时的显示内容。经过隐私处理的两张图片不是原始照片，界面细节可能有变化。

## 动态演示

点击预览可打开对应 MP4。以下均为实机拍摄的桌面演示，视频已静音。

| 导航画面 | 路线与行程环 | 龙珠雷达风格 |
| :---: | :---: | :---: |
| [![导航预览](docs/media/videos/navigation-demo.gif)](docs/media/videos/navigation-demo.mp4) | [![路线预览](docs/media/videos/route-progress-demo.gif)](docs/media/videos/route-progress-demo.mp4) | [![雷达预览](docs/media/videos/radar-demo.gif)](docs/media/videos/radar-demo.mp4) |

## 模拟效果展示

![地图导航与龙珠雷达模拟效果](docs/media/mockups/navigation-simulation.png)

两种导航视觉的概念展示，**AI 模拟效果，非实机截图**。

![天气、降水空气及预警模拟效果](docs/media/mockups/weather-simulation.png)

天气页面概念展示，**AI 模拟效果；数据虚构，非实时预报**。实际界面、数据可用性和硬件外观以当前软件及设备为准。

[查看完整素材说明](docs/MEDIA.md) · [打开离线展示页](docs/showcase.html)

## 支持的硬件

1. **M5Stack Stopwatch（本分支新增适配并独立维护）**
   - **硬件规格**：ESP32-S3，1.43 英寸 466×466 圆形 AMOLED（CO5300 QSPI）、CST820B 触控、M5PM1 电源管理、M5IOE1 扩展。
   - **源码位置**：`firmware/boards/stopwatch/MotoNav`
   - **适配特性**：
     - **原生极速图形驱动**：复用同款 CO5300 QSPI 硬件 DMA 刷屏管线，466×466 原生点对点无损高帧率渲染。
     - **物理按键 A/B 联动换页**：骑行戴手套时操作更灵敏——面对屏幕左侧 A 键切到【音乐】（从左滑入）、右侧 B 键切到【天气】（从右滑入）；在天气/音乐页按任意按键平滑返回主页（带 250ms 防抖与动画防干扰）。
     - **物理按键深睡与唤醒**：无操作自动关机后，按任意物理按键（EXT1 唤醒源）即可重新唤醒开机。
     - **电量平稳管理**：基于 M5PM1 硬件采样并引入 7:1 软件低通滤波，告别电量跳动，原生支持充电闪电动画与低电提醒。
     - **通信与配对握手优化**：解决 Bluedroid 下与 Android App 的 BLE 配对认证握手死锁，保证开机秒连接。
     - **构建与分发闭环**：支持 PlatformIO 一键编译，并提供 `build_and_merge.bat` 一键自动生成适用于 **M5Burner** 从 `0x0000` 烧录的出厂全量固件。
2. **Waveshare ESP32-S3-Touch-AMOLED-1.75（上游原生支持）**
   - 466×466 圆屏，固件位于 `firmware/boards/175/MotoNav`。

---

## M5Stack Stopwatch 编译、烧录与 M5Burner 发布包生成

### 1. 本地快速编译与调试烧录
```bash
# 编译固件
pio run -e stopwatch

# 连接设备一键烧录
pio run -e stopwatch -t upload

# 串口监控（115200 波特率）
pio run -e stopwatch -t monitor
```

### 2. 一键打包 M5Burner / 出厂全量烧录固件
参照主流出厂固件发布规范，本项目提供了全自动合并工具，自动将 bootloader、partitions、boot_app0 与主程序固件整合为一个可从 `0x0000` 完整刷入的单镜像：

- **Windows 用户**：直接双击根目录下的 **`build_and_merge.bat`**，即可一键全自动编译并合并出厂固件。
- **命令行用户**：
  ```bash
  # 编译并生成合并固件
  python tools/merge_firmware.py --env stopwatch --build
  ```
- **生成产物**位于 `release/` 目录：
  - **`release/MotoNav_stopwatch_factory_0x0000.bin`**：**M5Burner 专用出厂发布包**。可直接在 M5Burner 自定义固件（Custom Burn）中选择，设置烧录地址 `0x0000`，或直接发布给其他 Stopwatch 用户一键线刷。
  - **`release/MotoNav_stopwatch_app_ota_0x10000.bin`**：仅包含应用层主程序，供 OTA 或保留数据升级使用。
- **本地出厂刷机验证**：连接设备后双击 **`flash_factory.bat`** 即可直接将合并固件写入设备。

---

## 分支维护与上游同步说明

- **分支隔离原则**：本仓库作为个人适配版本，所有针对 M5Stack Stopwatch 的代码、脚本与文档改动**仅在本仓库维护并提交**，不向上游仓库发起 Pull Request，绝不影响或干扰上游主项目的开发规划。
- **获取上游更新**：当上游发布了新的 Android / iOS App 或协议更新时，可通过仓库内提供的脚本一键拉取并 rebase：
  ```powershell
  # 在项目根目录下执行 PowerShell 脚本
  .\scripts\sync-upstream.ps1
  ```
  该脚本会自动同步上游 `main` 分支最新提交，并将本分支的改动优雅 rebase 到最新代码之上。

---

## 功能

地图、路线、转向提醒、车速、行程环、巡航、音乐控制和品牌开机动画；保留摩托车、驾车、自行车的路线分享处理。高德网页无法还原原路线时，按界面提示处理，不保证所有分享链接都能还原。

iPhone 新增圆屏 BLE HID 媒体按键，可向当前播放器发送播放/暂停和上一首/下一首，面向 QQ 音乐等播放器使用。导航与媒体按键共用同一 BLE 设备；HID 不读取歌名或确认播放状态。需要系统 HID 配对，QQ 音乐响应及锁屏行为尚待实机验证，详见 [配对与使用说明](docs/BLE-HID.md)。

天气改用和风天气：实况、未来五小时预报、短时降水、空气质量和官方预警。圆屏天气页轻点循环显示主页、降水/空气、预警摘要；手机可查看完整详情及来源。仍按查看天气触发查询，并使用缓存，不在导航后台持续轮询天气。

## iPhone 安装

从 [GitHub Releases](https://github.com/zyn950929/MotoNav/releases) 下载 `MotoNav-0.13.8-unsigned.ipa`，使用自己的 Apple 账号通过 Sideloadly 签名安装。详见 [安装与续签教程](docs/INSTALL-IOS.md)。

## Windows 一键打包

1. 下载仓库 ZIP 并完整解压到较短的本地路径。
2. 双击根目录“一键打包.bat”。不需要 Android Studio。
3. 首次需要联网下载 JDK、Android SDK、Gradle 和依赖；SDK 许可由官方工具提示，阅读后自行决定是否接受。
4. 成功后 output/MotoNav-personal.apk 是安装包，output/Application-info.txt 包含生成的包名和签名 SHA1。

首次打包自动生成专属应用包名与签名。源码命名空间 com.example.motonav 与实际安装包 applicationId 可以不同。

**更新前必须备份 android/personal.properties 和整个 android/keystore 文件夹。新版解压后先复制回这两项，再打包，才能沿用包名、证书和地图绑定信息。不要把生成后的个人工程或证书公开上传。**

本仓库不包含编译工具、依赖缓存、APK、个人签名或地图天气凭据。国内网络访问部分官方下载站点可能需要额外的网络配置。

已有环境可执行：

```powershell
.\Build-Personal.ps1 -JavaPath 'D:\Tools\jdk-17' -SdkPath 'D:\Tools\android-sdk' -GradlePath 'D:\Tools\gradle\bin\gradle.bat'
```

已缓存所需依赖时可追加 -Offline。

## 地图与天气配置

安装后进入底部“地图 Key”页面配置：

- 高德：Android 平台 SDK Key，绑定实际安装包的包名和发布证书 SHA1。地图、搜索、算路和导航仍依赖高德。
- Android 底图、卫星和沿线离线地图均使用高德 SDK；无需天地图 Key，也没有 OSM 备用取图。龙珠雷达不请求底图。
- 和风：在“和风天气 → 配置”填入自己账号的 API Host 与 API Key，启用天气预报、分钟降水、空气质量和天气预警。凭据不内置。

服务权限、使用范围、请求额度、署名及计费以各服务商现行规则为准。不同用户重新签名并不自动获得服务授权。天气与预警可能延迟，不替代官方最新发布。

## Mac 双击烧录

本机已安装 Arduino CLI / ESP32 Core 3.3.10 和 esptool 5.3.0。连接微雪 1.75 英寸圆屏的 USB 数据线，在 Finder 中双击根目录 [一键烧录固件.command](一键烧录固件.command)。脚本校验生成镜像的 SHA-256 和应用偏移，只有一个 USB 串口时自动选择，多个串口时要求选择。

默认写入 `0x10000` 的应用升级镜像，不执行整片擦除，适用于已有相同 16 MB / app3M_fat9M_16MB 分区的 MotoNav。首次完整安装可运行 `./一键烧录固件.command --full`，需输入 INSTALL 确认。脚本只支持本仓库的 1.75 英寸板型。

`./一键烧录固件.command --check` 仅检查工具、固件和串口，不写入设备。固件镜像与清单位于 `firmware/dist/175/panel0/`；源码仓库不提交这些生成文件，缺少时需先构建并生成清单。HID 配对步骤见 [说明](docs/BLE-HID.md)。

## 固件构建

iOS 与 Android 共用一份圆屏固件，源码位于 `firmware/boards/175/MotoNav`，第三方库位于 `firmware/libraries`。手机端 APK 与 IPA 分开发布，详见 [通用固件说明](firmware/README.md)。

使用 Arduino CLI，安装 Espressif ESP32 Arduino Core 3.3.10，然后执行：

```powershell
.\firmware\build.ps1 -Board 175
```

可用 -CliPath、-ConfigFile 指定自己的 Arduino CLI 和配置。输出在 firmware/dist 中。已有相同板型及分区的 MotoNav 使用 app_0x10000 升级镜像；首次安装使用 full_0x0 完整镜像。完整刷写可能覆盖原程序和设置，请先备份。

## 检查与验证范围

android/tests/run.ps1 可运行到达判定、地图加载与轨迹采样、巡航计时、地图风格、区县格式和网页摩托车定位跟踪检查；run-webmotor.ps1 另测合成路线解析、途经点和异常数据。轨迹与常用目的地的持久化检查见 android/tests/README.md。天气解析测试见 android/tests/QWeatherClientTest.java，需要完成 App 编译、Kotlin 运行库及 org.json 测试运行库。

天气实现原版已完成 App、1.75 英寸板型固件的编译，以及接口示例数据解析检查；公开命名空间导出已构建验证。上方实拍来自用户提供的设备记录，软件检查与展示素材不代表独立路测或全场景验证通过。

## 依赖与许可

licenses 和各第三方库目录保留原许可及版权声明。高德 SDK 通过构建依赖获取，地图/天气数据按服务商规则使用。品牌标识属于各自权利人。

项目自有代码及原创素材按 [非商业使用条款](LICENSE.md) 提供，不采用 MIT/Apache 等允许商业使用的许可。禁止未经许可倒卖源码、固件、安装包，或以付费代刷、打包销售、硬件捆绑销售等方式获利。第三方组件、地图和天气服务仍适用各自的许可及规则。

## 致谢与衍生声明

本项目在 `avatar` 分支中的伴侣模式（Companion Mode）动态表情引擎、关键帧插值与体感交互算法，衍生并移植自优秀的开源项目 **[m5stack-stopwatch-avatar](https://github.com/Trentct/m5stack-stopwatch-avatar)**（作者：[Trentct](https://github.com/Trentct)）。

- **项目链接**：[m5stack-stopwatch-avatar](https://github.com/Trentct/m5stack-stopwatch-avatar) (`https://github.com/Trentct/m5stack-stopwatch-avatar.git`)
- **致谢与说明**：衷心感谢原作者 Trentct 为 M5Stack StopWatch 圆屏设计并实现的精彩表情角色 KK。其纯程序化实时矢量绘制、微动注视算法、六轴姿态跟随与多级震动反馈为本项目带来了极具温度的交互体验。本项目在此基础上适配了 MotoNav 的双缓冲图形管线，实现了从主程序界面滑入伴侣模式、长按 AB 键安全返回主界面、待机与休眠智能调度以及角色扩展接口。



圆屏 v7.10.0 与 iOS v0.11.0 增加 iPhone AMS 歌曲信息：歌名、歌手、专辑、状态及进度，保留已实测可用的网易云 HID 媒体按键。详见 [媒体控制与歌曲信息](docs/BLE-HID.md)。

圆屏 v7.10.1 修复 iOS 无级旋转：正确解析 `#R,最近四档,精确角度` 的第二项，支持 0–359°，兼容旧四档 `#R,0~3` 和安卓版 `#D角度`。已安装的 iOS App 无需重装，停止拖动 1.5 秒后固件保存角度，触摸坐标继续按画面角度换算。

圆屏 v7.11.0 合并上述无级旋转修复并增加可选逐句歌词支持。配套 iOS v0.12.0 的媒体控制中“显示逐句歌词”默认关闭，默认布局是上方歌名、下方歌手；开启后使用 LRCLIB 匹配同步歌词，匹配不到时保留默认布局。仅升级固件不会给旧版手机 App 增加歌词开关。

圆屏 v7.12.0 增加歌词 OFF 清理及歌手栏刷新请求，并在 HID 未就绪时使用 iPhone AMS 媒体命令作为备用控制。iOS v0.12.1 增加独立歌词开关回执与重试；详见上述媒体协议说明。

iOS v0.13.7 默认使用高德建筑提取；修复 Retina 截图、相机更新时序与地图覆盖问题，并修复 App 重连错过认证通知、始终不查询圆屏建筑能力的问题。更新已安装到新手机，正常模拟导航已确认建筑地图生成和圆屏 K 回执，并通过固件解码器验证实际发送包。2026-10-17 的 120 km/h 前台模拟收到 42 次完整地图回执，用户确认此轮未再出现底图空白；地图传输延迟仍存在，详见 ios/README.md。

[本次实际验证结果与未覆盖范围](docs/VERIFY-2026-10-21.md)
