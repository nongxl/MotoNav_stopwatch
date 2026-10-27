$ErrorActionPreference='Stop'
$taskJdk=$env:JAVA_HOME
if(!$taskJdk){throw 'Please set JAVA_HOME to JDK 17 or newer.'}
$taskClasses=Join-Path $PSScriptRoot '.local/test-classes'
New-Item -ItemType Directory -Force $taskClasses | Out-Null
$taskSource=Join-Path $PSScriptRoot 'app/src/main/java/com/example/motonav'
& (Join-Path $taskJdk 'bin/javac.exe') -encoding UTF-8 -d $taskClasses (Join-Path $taskSource 'RideTrackMath.java') (Join-Path $taskSource 'MapLoadStatus.java') (Join-Path $PSScriptRoot 'tests/RideToolsTest.java')
if($LASTEXITCODE -ne 0){throw 'Ride tools test compilation failed'}
& (Join-Path $taskJdk 'bin/java.exe') -cp $taskClasses com.example.motonav.RideToolsTest
if($LASTEXITCODE -ne 0){throw 'Ride tools tests failed'}
