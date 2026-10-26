$ErrorActionPreference='Stop'
$taskParent=Split-Path -Parent $PSScriptRoot
$taskJdk=Join-Path $taskParent '_tools/jdk/jdk-17.0.20.1+1'
$taskCache=Join-Path (Split-Path -Parent $taskParent) '.gradle/caches/modules-2/files-2.1/org.jetbrains.kotlin/kotlin-stdlib/2.0.21'
$taskStd=(Get-ChildItem -LiteralPath $taskCache -Filter '*.jar' -Recurse | Select-Object -First 1).FullName
$taskClasses=Join-Path $PSScriptRoot '.local/amap-only-test-classes'
$taskKotlin=Join-Path $PSScriptRoot 'app/build/tmp/kotlin-classes/release'
$taskJava=Join-Path $PSScriptRoot 'app/build/intermediates/javac/release/compileReleaseJavaWithJavac/classes'
New-Item -ItemType Directory -Force $taskClasses | Out-Null
$taskClasspath=@($taskClasses,$taskKotlin,$taskJava,$taskStd) -join ';'
& (Join-Path $taskJdk 'bin/javac.exe') -encoding UTF-8 -cp $taskClasspath -d $taskClasses (Join-Path $PSScriptRoot 'tests/jvm/android/graphics/Bitmap.java') (Join-Path $PSScriptRoot 'tests/AmapOnlyTest.java')
if($LASTEXITCODE -ne 0){throw 'AMap-only tests compile failed'}
& (Join-Path $taskJdk 'bin/java.exe') -cp $taskClasspath com.example.motonav.AmapOnlyTest
if($LASTEXITCODE -ne 0){throw 'AMap-only tests failed'}
