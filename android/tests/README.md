# Android 源码检查

以下检查使用模拟数据，不是道路安全认证。测试代码中的 Android I/O 适配器仅在 JVM 检查时使用，不会打包进 App。

## 不依赖 Android SDK 的逻辑检查

安装 JDK 17，将 JAVA_HOME 指向 JDK 根目录后，在仓库根目录运行：

```powershell
powershell -ExecutionPolicy Bypass -File .\android\tests\run.ps1
```

覆盖巡航静止计时、地图风格、区县文字格式，以及本次新增的到达生命周期（45 项）、地图加载与轨迹采样（34 项）。

## 轨迹和常用目的地持久化检查

先完成 Android release 构建，例如运行仓库根目录的一键打包。检查直接执行该构建的 Kotlin/Java 类，覆盖保存、读取、GPX 多段导出、收藏去重及异常中断恢复。

另需准备 Kotlin 标准库 2.0.21（可使用 Gradle 已下载缓存）和真实 JVM 版 `org.json:json:20240303`。Android SDK 自带的 org.json 是桩实现，不能替代该测试依赖；第三方依赖不随源码提交。

```powershell
powershell -ExecutionPolicy Bypass -File .\android\tests\run-persistence.ps1 `
  -SdkPath 'D:\Tools\android-sdk' `
  -KotlinStdlib 'D:\TestLibs\kotlin-stdlib-2.0.21.jar' `
  -JsonJar 'D:\TestLibs\json-20240303.jar'
```

路径改成自己机器上的位置。正常持久化检查 33 项；异常中断夹具会主动结束测试进程，再由单独进程完成恢复检查 7 项。生成的模拟记录保存在 android/.local 下，不会读取手机数据。

天气解析检查仍保留在 QWeatherClientTest.java，需要完成 App 构建及 Kotlin、org.json 运行库；它不在上述纯 Java 逻辑入口内。

## 8.3.23-webmotor.4 补充

`./android/tests/run.ps1` 使用 JAVA_HOME（或 PATH 中的 Java 17+），运行到达、巡航、风格、区县、轨迹采样和 MotorTrack 纯 Java 检查。无需设备、地图 Key 或网络。

网页解析测试先编译 `assembleRelease`，再运行：

```powershell
.\android\tests\run-webmotor.ps1 -KotlinStdlib '<kotlin-stdlib-2.0.21.jar路径>' -JsonJar '<org.json运行库jar路径>'
```

可从本机 Gradle 缓存找 Kotlin 2.0.21 运行库；JSON 测试使用 Maven 坐标 `org.json:json:20240303` 的 JVM 实现，Android SDK 的 org.json 空桩不能替代。脚本仅使用测试传入依赖，不访问高德。

公开 WebMotorRouteTest 全部使用人工合成路线，测试多方案、连续几何、转向、稳定到达、途经点、错误响应和 URL 校验。真实行程响应与用户坐标不随源码发布。

四风格及高德取图逻辑检查可执行 `./android/tests/run-raster.ps1 -KotlinStdlib '<kotlin-stdlib-2.0.21.jar路径>'`，需先编译 release。脚本使用 tests/jvm 图形适配器，输出只写到 .local。合成测试不代表真实禁摩限制或道路验证。
