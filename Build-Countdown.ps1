param([string]$ProviderPath='',[string]$ApktoolPath='',[string]$JavaPath=$env:JAVA_HOME,
      [string]$SdkPath=$env:ANDROID_SDK_ROOT,[string]$GradlePath='',
      [string]$ZigPath='',[string]$PythonPath='python',[switch]$Offline)
$ErrorActionPreference='Stop'
$project=Join-Path $PSScriptRoot 'android/countdown'
function Need-Path([string]$Value,[string]$Prompt) {
    if(!$Value){$Value=Read-Host $Prompt}
    $Value=$Value.Trim().Trim('"')
    if(!(Test-Path -LiteralPath $Value)){throw "Path does not exist: $Value"}
    return [IO.Path]::GetFullPath($Value)
}
if($ProviderPath){$ProviderPath=Need-Path $ProviderPath '兼容数据组件 APK 路径'}
$ApktoolPath=Need-Path $ApktoolPath 'apktool.jar 路径'
$JavaPath=Need-Path $JavaPath 'JDK 17 根目录'
$SdkPath=Need-Path $SdkPath 'Android SDK 根目录'
if(!$GradlePath){$GradlePath=Join-Path $project 'gradlew.bat'}
$GradlePath=Need-Path $GradlePath 'Gradle 8.10.2 的 gradle.bat 路径'
if(!$ZigPath){$cmd=Get-Command zig -ErrorAction SilentlyContinue;if($cmd){$ZigPath=$cmd.Source}}
$ZigPath=Need-Path $ZigPath 'Zig 0.13.0 的 zig.exe 路径'
$flags=@((Join-Path $project 'build-countdown.py'),'--apktool',$ApktoolPath,
    '--java-home',$JavaPath,'--sdk',$SdkPath,'--gradle',$GradlePath,'--zig',$ZigPath)
if($ProviderPath){$flags+=@('--provider',$ProviderPath)}
if($Offline){$flags+='--offline'}
& $PythonPath @flags
if($LASTEXITCODE -ne 0){throw 'Countdown build failed. Review the first error above.'}
Write-Host ('Output: '+(Join-Path $project 'output')) -ForegroundColor Green
