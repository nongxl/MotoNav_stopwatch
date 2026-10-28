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

if($ProviderPath){$ProviderPath=Need-Path $ProviderPath 'Provider APK path'}

# Auto-detect Apktool
if(!$ApktoolPath){
    $commonJar = "$env:USERPROFILE\scoop\apps\apktool\current\apktool.jar"
    if(Test-Path $commonJar){$ApktoolPath = $commonJar}
}
$ApktoolPath=Need-Path $ApktoolPath 'apktool.jar path'
if(Test-Path -PathType Container $ApktoolPath){
    $subJar = Join-Path $ApktoolPath 'apktool.jar'
    if(Test-Path $subJar){$ApktoolPath = $subJar}
}

# Auto-detect Java Home
$JavaPath=Need-Path $JavaPath 'JDK root path (JAVA_HOME)'

# Auto-detect Android SDK
if(!$SdkPath){
    $commonSdk = "$env:USERPROFILE\AppData\Local\Android\Sdk"
    if(Test-Path $commonSdk){$SdkPath = $commonSdk}
}
$SdkPath=Need-Path $SdkPath 'Android SDK root path'

# Auto-detect Gradle
if(!$GradlePath){$GradlePath=Join-Path $project 'gradlew.bat'}
$GradlePath=Need-Path $GradlePath 'gradle.bat path'

# Auto-detect Zig
if(!$ZigPath){
    $cmd=Get-Command zig -ErrorAction SilentlyContinue
    if($cmd){$ZigPath=$cmd.Source}
    else {
        $commonZig = "$env:USERPROFILE\scoop\shims\zig.exe"
        if(Test-Path $commonZig){$ZigPath = $commonZig}
    }
}
$ZigPath=Need-Path $ZigPath 'Zig 0.13.0 exe path'

$flags=@((Join-Path $project 'build-countdown.py'),'--apktool',$ApktoolPath,
    '--java-home',$JavaPath,'--sdk',$SdkPath,'--gradle',$GradlePath,'--zig',$ZigPath)
if($ProviderPath){$flags+=@('--provider',$ProviderPath)}
if($Offline){$flags+='--offline'}

Write-Host "Running build-countdown with:" -ForegroundColor Cyan
Write-Host "  Apktool: $ApktoolPath"
Write-Host "  Java:    $JavaPath"
Write-Host "  SDK:     $SdkPath"
Write-Host "  Zig:     $ZigPath"
Write-Host "  Gradle:  $GradlePath"
Write-Host ""

& $PythonPath @flags
if($LASTEXITCODE -ne 0){throw 'Countdown build failed. Review the first error above.'}
Write-Host ('Output: '+(Join-Path $project 'output')) -ForegroundColor Green
