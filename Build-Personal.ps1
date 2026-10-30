param([string]$JavaPath='', [string]$SdkPath='', [string]$GradlePath='', [switch]$Offline)
$ErrorActionPreference='Stop'
[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
$root=$PSScriptRoot
$project=Join-Path $root 'android/standard'
$tools=Join-Path $root '.tools'

function Download($url,$path) {
    if(-not(Test-Path -LiteralPath $path)) {
        Write-Host "Downloading $url"
        Invoke-WebRequest -UseBasicParsing -Uri $url -OutFile "$path.part"
        Move-Item -LiteralPath "$path.part" -Destination $path -Force
    }
}

# 1. Auto-detect Java Home
if(-not $JavaPath){
    if($env:JAVA_HOME -and (Test-Path (Join-Path $env:JAVA_HOME 'bin/java.exe'))){
        $JavaPath=$env:JAVA_HOME
    } else {
        $cmdJava=Get-Command java -ErrorAction SilentlyContinue
        if($cmdJava){
            $binDir=Split-Path $cmdJava.Source
            if(Test-Path (Join-Path $binDir 'keytool.exe')){
                $JavaPath=Split-Path $binDir
            }
        }
    }
}
if(-not $JavaPath){
    $jdk=Get-ChildItem -Path "$tools/jdk/*/bin/java.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if(-not $jdk){
        New-Item -ItemType Directory -Force $tools | Out-Null
        Download 'https://api.adoptium.net/v3/binary/latest/21/ga/windows/x64/jdk/hotspot/normal/eclipse' "$tools/jdk.zip"
        Expand-Archive -LiteralPath "$tools/jdk.zip" -DestinationPath "$tools/jdk" -Force
        $jdk=Get-ChildItem -Path "$tools/jdk/*/bin/java.exe" | Select-Object -First 1
    }
    $JavaPath=Split-Path (Split-Path $jdk.FullName)
}
$env:JAVA_HOME=$JavaPath
$env:PATH="$JavaPath\bin;"+$env:PATH

# 2. Auto-detect Android SDK
if(-not $SdkPath){
    $candidateProps=@(
        (Join-Path $project 'local.properties'),
        (Join-Path $root 'android/countdown/local.properties')
    )
    foreach($lp in $candidateProps){
        if(Test-Path $lp){
            $line=Get-Content $lp | Where-Object { $_ -match '^sdk\.dir\s*=\s*(.+)$' } | Select-Object -First 1
            if($line){
                $val=($line -split '=',2)[1].Trim()
                $val=$val.Replace('\: ',': ').Replace('\:',':').Replace('\/','/').Replace('\','/')
                if(Test-Path $val){$SdkPath=$val; break}
            }
        }
    }
    if(-not $SdkPath -and $env:ANDROID_HOME -and (Test-Path $env:ANDROID_HOME)){$SdkPath=$env:ANDROID_HOME}
    if(-not $SdkPath -and $env:ANDROID_SDK_ROOT -and (Test-Path $env:ANDROID_SDK_ROOT)){$SdkPath=$env:ANDROID_SDK_ROOT}
    if(-not $SdkPath){
        $defaultSdk=Join-Path $env:USERPROFILE 'AppData/Local/Android/Sdk'
        if(Test-Path $defaultSdk){$SdkPath=$defaultSdk}
    }
    if(-not $SdkPath){$SdkPath=Join-Path $tools 'android-sdk'}
}
$env:ANDROID_HOME=$SdkPath
$env:ANDROID_SDK_ROOT=$SdkPath

# 3. Check apksigner and build-tools
$apksigner=Get-ChildItem -Path "$SdkPath/build-tools/*/apksigner.bat" -ErrorAction SilentlyContinue | Sort-Object FullName -Descending | Select-Object -First 1
$hasPlatform=Test-Path "$SdkPath/platforms/android-35/android.jar"

if(-not $apksigner -or -not $hasPlatform) {
    New-Item -ItemType Directory -Force $tools | Out-Null
    $manager=Join-Path $SdkPath 'cmdline-tools/latest/bin/sdkmanager.bat'
    if(-not(Test-Path $manager)) {
        Download 'https://dl.google.com/android/repository/commandlinetools-win-15859902_latest.zip' "$tools/sdk-tools.zip"
        Expand-Archive -LiteralPath "$tools/sdk-tools.zip" -DestinationPath "$tools/sdk-unpacked" -Force
        New-Item -ItemType Directory -Force "$SdkPath/cmdline-tools/latest" | Out-Null
        Copy-Item -Path "$tools/sdk-unpacked/cmdline-tools/*" -Destination "$SdkPath/cmdline-tools/latest" -Recurse -Force
    }
    Write-Host 'Review the Android SDK licenses; enter y only if you accept.'
    & $manager "--sdk_root=$SdkPath" --licenses
    if($LASTEXITCODE -ne 0){throw 'SDK license setup failed.'}
    & $manager "--sdk_root=$SdkPath" 'platforms;android-35' 'build-tools;35.0.0' 'platform-tools'
    if($LASTEXITCODE -ne 0){throw 'SDK installation failed.'}
    $apksigner=Get-ChildItem -Path "$SdkPath/build-tools/*/apksigner.bat" | Sort-Object FullName -Descending | Select-Object -First 1
}

Write-Host "Building MotoNav Standard with:" -ForegroundColor Cyan
Write-Host "  Java:    $JavaPath"
Write-Host "  SDK:     $SdkPath"
Write-Host "  Signer:  $($apksigner.FullName)"
Write-Host ""

$identity=Join-Path $project 'personal.properties'
$signing=Join-Path $project 'keystore/keystore.properties'
$keyfile=Join-Path $project 'keystore/personal.jks'
New-Item -ItemType Directory -Force (Split-Path -Parent $keyfile) | Out-Null
if(-not(Test-Path $identity)) {
    if(Test-Path $keyfile){throw 'personal.properties missing. Restore it from your backup before continuing.'}
    $appId='com.motonav.u'+[Guid]::NewGuid().ToString('N')
    [IO.File]::WriteAllText($identity,"personalApplicationId=$appId`n")
}
$appId=(([IO.File]::ReadAllText($identity)).Trim() -split '=',2)[1].Trim()
if($appId -notmatch '^com\.motonav\.u[a-f0-9]{32}$'){throw 'Invalid personal application ID.'}
if(-not(Test-Path $signing)) {
    if(Test-Path $keyfile){throw 'Signing settings missing. Restore your backup.'}
    $password=[Guid]::NewGuid().ToString('N')+[Guid]::NewGuid().ToString('N')
    $env:MOTONAV_SIGN_PASSWORD=$password
    & "$JavaPath/bin/keytool.exe" -genkeypair -storetype JKS -keystore $keyfile -storepass:env MOTONAV_SIGN_PASSWORD -keypass:env MOTONAV_SIGN_PASSWORD -alias motonav -keyalg RSA -keysize 2048 -validity 10000 -dname 'CN=MotoNav Personal,O=MotoNav,C=CN'
    if($LASTEXITCODE -ne 0){throw 'Certificate generation failed.'}
    [IO.File]::WriteAllText($signing,"storeFile=personal.jks`nstorePassword=$password`nkeyAlias=motonav`nkeyPassword=$password`n")
}
if(-not(Test-Path $keyfile)){throw 'Signing certificate missing. Restore your backup; do not generate a replacement.'}
$props=ConvertFrom-StringData ([IO.File]::ReadAllText($signing))
$env:MOTONAV_SIGN_PASSWORD=$props.storePassword
$out=Join-Path $root 'output'
New-Item -ItemType Directory -Force $out | Out-Null
& "$JavaPath/bin/keytool.exe" -exportcert -keystore $keyfile -storepass:env MOTONAV_SIGN_PASSWORD -alias motonav -file "$out/certificate.der"
if($LASTEXITCODE -ne 0){throw 'Certificate export failed.'}
$fingerprint=(Get-FileHash -LiteralPath "$out/certificate.der" -Algorithm SHA1).Hash -replace '(..)(?!$)','$1:'
[IO.File]::WriteAllText("$out/Application-info.txt","Package: $appId`r`nSHA1: $fingerprint`r`n")
$escapedSdk=$SdkPath.Replace('\','/').Replace(':','\:')
[IO.File]::WriteAllText("$project/local.properties","sdk.dir=$escapedSdk`n")
if(-not $GradlePath){$GradlePath=Join-Path $project 'gradlew.bat'}
Push-Location -LiteralPath $project
try {
    $buildArgs=@('--no-daemon','--max-workers=1','-Dorg.gradle.jvmargs=-Xmx1536m -Dfile.encoding=UTF-8','-Pkotlin.compiler.execution.strategy=in-process',"-PpersonalApplicationId=$appId",'assembleRelease')
    if($Offline){$buildArgs=@('--offline')+$buildArgs}
    & $GradlePath @buildArgs
    if($LASTEXITCODE -ne 0){throw 'Build failed. See the messages above.'}
} finally {Pop-Location}
$apk=Join-Path $project 'app/build/outputs/apk/release/app-release.apk'
& $apksigner.FullName verify --verbose --print-certs $apk
if($LASTEXITCODE -ne 0){throw 'APK signature verification failed.'}
Copy-Item -LiteralPath $apk -Destination "$out/MotoNav-personal.apk" -Force
$env:MOTONAV_SIGN_PASSWORD=$null
Write-Host "Done: $out" -ForegroundColor Green
Write-Host "Package: $appId"
Write-Host "SHA1: $fingerprint"
Write-Host 'Back up android/standard/personal.properties and android/standard/keystore. Never share them.'
