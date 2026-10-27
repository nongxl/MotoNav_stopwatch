$ErrorActionPreference='Stop'
$taskJdk=$env:JAVA_HOME
if(!$taskJdk){throw 'Please set JAVA_HOME to JDK 17 or newer.'}
$taskClasses=Join-Path $PSScriptRoot '.test-classes'
New-Item -ItemType Directory -Force $taskClasses | Out-Null
& (Join-Path $taskJdk 'bin/javac.exe') -encoding UTF-8 -d $taskClasses (Join-Path $PSScriptRoot 'app/src/main/java/com/example/motonav/CruiseIdleTimer.java') (Join-Path $PSScriptRoot 'tests/CruiseIdleTimerTest.java')
if($LASTEXITCODE -ne 0){throw 'Test compilation failed'}
& (Join-Path $taskJdk 'bin/java.exe') -cp $taskClasses com.example.motonav.CruiseIdleTimerTest
if($LASTEXITCODE -ne 0){throw 'Cruise inactivity tests failed'}
