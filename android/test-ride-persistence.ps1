# Compile the test-only Android I/O adapters, then execute the release's actual Kotlin classes.
$ErrorActionPreference='Stop'
$taskParent=Split-Path -Parent $PSScriptRoot
$taskJdk=Join-Path $taskParent '_tools/jdk/jdk-17.0.20.1+1'
$taskAndroid=Join-Path $taskParent '_tools/android-sdk/platforms/android-35/android.jar'
$taskCache=Join-Path (Split-Path -Parent $taskParent) '.gradle/caches/modules-2/files-2.1/org.jetbrains.kotlin/kotlin-stdlib/2.0.21'
$taskStd=(Get-ChildItem -LiteralPath $taskCache -Filter '*.jar' -Recurse | Select-Object -First 1).FullName
$taskJson=Join-Path $PSScriptRoot '.test-libs/json.jar'
$taskClasses=Join-Path $PSScriptRoot '.local/persistence-test-classes'
$taskKotlin=Join-Path $PSScriptRoot 'app/build/tmp/kotlin-classes/release'
$taskJava=Join-Path $PSScriptRoot 'app/build/intermediates/javac/release/compileReleaseJavaWithJavac/classes'
New-Item -ItemType Directory -Force $taskClasses | Out-Null
$taskClasspath=@($taskClasses,$taskKotlin,$taskJava,$taskStd,$taskJson,$taskAndroid) -join ';'
$taskAdapters=@(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'tests/jvm') -Filter '*.java' -Recurse | ForEach-Object FullName)
& (Join-Path $taskJdk 'bin/javac.exe') -encoding UTF-8 -cp $taskClasspath -d $taskClasses @taskAdapters (Join-Path $PSScriptRoot 'tests/RidePersistenceTest.java')
if($LASTEXITCODE -ne 0){throw 'Persistence tests compile failed'}
$taskRun=Join-Path $PSScriptRoot ('.local/ride-tests-'+[guid]::NewGuid().ToString('N'))
& (Join-Path $taskJdk 'bin/java.exe') -cp $taskClasspath com.example.motonav.RidePersistenceTest normal (Join-Path $taskRun 'normal')
if($LASTEXITCODE -ne 0){throw 'Persistence tests failed'}
& (Join-Path $taskJdk 'bin/java.exe') -cp $taskClasspath com.example.motonav.RidePersistenceTest seed-crash (Join-Path $taskRun 'crash')
if($LASTEXITCODE -ne 0){throw 'Crash fixture failed'}
& (Join-Path $taskJdk 'bin/java.exe') -cp $taskClasspath com.example.motonav.RidePersistenceTest recover (Join-Path $taskRun 'crash')
if($LASTEXITCODE -ne 0){throw 'Crash recovery tests failed'}
