# MotoNav 读秒版 8.3.26-single.31

这是当前读秒版的完整自主源码与构建工具，和 [普通版 8.3.26](../standard/README.md) 分开维护。手机只安装一个最终 APK，使用现有的单引擎导航流程。

**红绿灯读秒属于实验功能，不保证一直有效，也不保证每个路口、每个方向都有数据。** 上游服务、版本、权限、账号、定位、路口覆盖、网络与系统后台限制变化都可能使其缺失或失效。缺数据时不补造读秒；必须以现场交通信号为准。

## 这一版包含什么

- 省电优化基于 **[@nongxl](https://github.com/nongxl)** 的 **[PR #9](https://github.com/zyn950929/MotoNav/pull/9)**（[原始提交](https://github.com/zyn950929/MotoNav/commit/a57019d5f161625b4da6c6fdafea62dfe4bcd503)）：蓝牙扫描分级与重连退避、按需持有唤醒锁、独立 GPS 观察降频，以及地图镜头、车标朝向和预览修正。
- 本项目在贡献基础上补入巡航和长途导航唤醒保护、选路／到达释放、停车定位更新、认证后重置退避及切换跟随模式刷新。未完成实际耗电量测，具体说明见 [single.31 更新记录](../../docs/RELEASE-ANDROID-8.3.26-single.31.md)。
- 修复网页摩托车后台路线已规划、导航未实际开始而缺少读秒：明确启动所选路线，连续检查真实导航状态；未启动时有限重试，运行后确实停止时最多恢复两次。正常停车和引导回调短暂停顿不会触发重启，主动结束导航后撤销监测。
- 驾车选路及 60／100／120 km/h 模拟速度、网页摩托车多方案规划、自行车与分享入口。
- 摩托车／驾车最多三个手动途经点，支持搜索加油站、排序、替换与删除；网页摩托车历史保存途经点顺序，支持收藏与历史重选。
- 导航中返回设置与恢复、锁屏恢复保护、蓝牙连接及地图更新处理、主页时间同步。
- 最新网页锚点修正，避免自动途经点紧靠转弯时干扰路口方向核对；保留方向、位置和数据时效检查。
- 小屏右侧竖排路口图标和秒数；缺少右转数据时用灰色图标／灰色实心圆表示，不把它当成已确认的绿灯。

## 依赖与公开范围

除常规 Android SDK 外，完整包还需要兼容数据组件。现在完整构建入口会**自动下载 `provider.apk` 到 `android/countdown/inputs/provider.apk`，并校验大小与 SHA256**；已有匹配文件会直接复用，新手不用另外找 APK 或填写路径。

组件下载：[provider.apk](https://github.com/zyn950929/MotoNav/releases/download/countdown-provider-9.5.13/provider.apk) · [组件发布页](https://github.com/zyn950929/MotoNav/releases/tag/countdown-provider-9.5.13)。约 148 MiB，超过 GitHub 普通 Git 文件大小限制，因此保存在此仓库的 Release 附件，代码目录保留下载清单 `provider.json`。**它只是电脑构建输入，不要当作最终 MotoNav 安装包。**

Git 代码目录包含本项目代码、原生路径读取代码、资源、构建工具和对指定输入的最小集成增量；不包含完整反编译源码、维护者个人 Key 或签名。第三方组件单独作为构建依赖提供，不归入本项目自主代码许可。

输入仅支持已验证的配套 9.5.13 APK，其 SHA256 为：

```text
f0fe164279a6d60bd83d926a32d47f335f044faf9c0d37564b49655e95db0141
```

输入不符会在修改前拒绝。该文件为用户提供的配套 9.5.13 补丁版本，未随附完整的补丁作者和许可信息；本项目不将第三方组件或补丁声明为自主开源代码，发布构建依赖不变更其权利归属及适用条款。高德 SDK 与相关服务适用其自身条款。普通版不下载或使用这份额外组件。

## 构建完整 APK（Windows）

准备 Python 3、JDK 17、Android SDK 35 / build-tools 35.0.0、Gradle 8.10.2、apktool 2.12.1 和 Zig 0.13.0。Zig 用于从 `native-route/route_bridge.cpp` 编译本项目的路径读取库，Git 不提交本地生成的库或下载缓存。

根目录执行 `一键打包-读秒版.bat`，依提示提供本机工具路径；也可在本目录运行：

```powershell
python build-countdown.py --apktool 'D:\Tools\apktool.jar' --gradle 'D:\Tools\gradle-8.10.2\bin\gradle.bat' --sdk 'D:\Android\Sdk' --java-home 'D:\Tools\jdk-17' --zig 'D:\Tools\zig-0.13.0\zig.exe'
```

首次联网时自动下载组件，下载完成前不会开始集成。下载失败会重试三次并提示直接下载地址；也可手动把同名文件放到 `inputs/provider.apk`。需要使用其他保存位置时，Python 入口可加 `--provider 'D:\Inputs\provider.apk'`，根目录 PowerShell 入口可加 `-ProviderPath`；仍必须通过相同校验。

只有组件和其他依赖**全部已缓存**时才加 `--offline`；离线缺文件会明确报错，不偷偷联网。脚本校验输入、应用增量、构建路径读取库、编译自主源码、检查资源与重复类，并用本机新生成的开发签名签署最终包。单独预下载及校验组件可运行 `python android/countdown/provider_download.py`（仓库根目录）。

## 让 WorkBuddy 帮忙打包（新手推荐）

安装并打开 WorkBuddy，选择一个本地工作文件夹，把下面整段发送给它；无需提前手动下载组件：

从安装工具、申请 Key 到烧录圆屏的完整步骤，见 [安卓小白 PDF 教程](../../docs/MotoNav-Android-WorkBuddy-Guide.pdf)。

```text
请从 https://github.com/zyn950929/MotoNav 的 main 分支获取最新源码，打包读秒版 8.3.26-single.31，工程在 android/countdown；已有旧源码时先更新，不要回到旧版本。
请准备需要的 Python 3、JDK 17、Android SDK 35/build-tools 35.0.0、Gradle 8.10.2、apktool 2.12.1 和 Zig 0.13.0；下载慢时换可信国内镜像。
使用根目录“一键打包-读秒版.bat”或 Build-Countdown.ps1 完成完整构建，让脚本自动从仓库 Release 获取并校验 provider.apk，不要只交付 Gradle 中间 APK。
最终包名保持 com.autonavi.amapauto，生成并保留我的本机签名；已有签名就沿用。请检查最终安装包及签名，告诉我 APK 完整路径、包名和最终签名证书的十六进制 SHA1（不是 APK 文件哈希），保存 Application-info.txt，并帮我打开成品文件夹。
不要修改功能，不要上传我的签名、Key 或登录资料。
```

## 成品与升级资料

成品是 **`output/MotoNav-8.3.26-single.31-countdown.apk`**。`app/build/outputs/` 中的 Gradle APK 只是中间文件，不能当完整读秒版安装。Android Studio 可以打开本目录编辑和编译自主源码；完整包仍由上面的入口生成。

最终包名固定为 **`com.autonavi.amapauto`**，实际证书的十六进制 SHA1 会打印并保存到 `output/Application-info.txt`；用这两项申请自己的高德 Android Key。最终包系统版本字段保留兼容组件要求，MotoNav 内显示 `8.3.26-single.31`。私下备份并沿用 `.local/debug.keystore`，不要上传或群发。已安装同包名但不同签名的软件不能直接覆盖，先自行备份数据再决定安装方式。

`inputs/`、`car-code/`、`car-resources/`、`moto-code/`、`framework/`、`.local/`、`output/` 都是本机构建产物，已经忽略，不属于公开源码。构建过程中不安装手机、不发布仓库。

## 安装后首次使用

先按提示允许附近设备／蓝牙、定位及必要通知权限，并完成额外数据组件的首次授权与初始化；这是本人需要阅读确认的步骤，编译工具不会替你接受条款。随后在 MotoNav 连接圆屏，在“圆屏显示”中检查读秒开关。

如果地图正常而读秒没有数据，先核对初始化是否完成、定位是否新鲜、后台限制和路口覆盖。普通室内模拟不保证产生真实灯秒数；维护者的虚拟 GPS 测试也不等于所有用户在任意地点点模拟都能收到数据。

## 验证与限制

JDK 17 配置好后：`python tests/run.py`。公开回归使用合成道路，不包含维护者的日常行程或设备录制日志。

single.31 已完成自主源码构建、完整集成与19组策略回归；实际功耗尚未量测。

此前 single.30 已在手机检查网页摩托车普通启动、锁屏启动、后台实际停止后的自动恢复及主动结束后的清理；测试期间收到了真实灯色和秒数。启动问题本轮未稳定复现原来的触发条件，不能把防护和恢复测试写成原故障已稳定复现，也不等于修复后的实际道路复测通过。新增原生状态与恢复回归，原有方向和路口检查保留。

此前 single.29 已验证途经点、加油站、历史重选与去重；真实灯源配合虚拟 GPS 完成三次红灯等待后对应绿灯起步，后续一个路口缺少匹配数据而结束。完整构建和签名的本轮结果及边界见 [验证记录](../../docs/VERIFY-2026-10-27.md)。
