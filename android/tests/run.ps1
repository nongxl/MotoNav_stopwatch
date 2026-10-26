$ErrorActionPreference='Stop'
$taskJava=if($env:JAVA_HOME){Join-Path $env:JAVA_HOME 'bin/java.exe'}else{'java'}
$taskJavac=if($env:JAVA_HOME){Join-Path $env:JAVA_HOME 'bin/javac.exe'}else{'javac'}
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskClasses=Join-Path $taskRoot '.local/test-classes'
New-Item -ItemType Directory -Force $taskClasses | Out-Null
& $taskJavac -encoding UTF-8 -d $taskClasses (Join-Path $taskRoot 'app/src/main/java/com/example/motonav/CruiseIdleTimer.java') (Join-Path $PSScriptRoot 'CruiseIdleTimerTest.java')
if($LASTEXITCODE -ne 0){throw 'Test compile failed'}
& $taskJava -cp $taskClasses com.example.motonav.CruiseIdleTimerTest
if($LASTEXITCODE -ne 0){throw 'Tests failed'}

& $taskJavac -encoding UTF-8 -d $taskClasses (Join-Path $taskRoot 'app/src/main/java/com/example/motonav/MapStylePolicy.java') (Join-Path $PSScriptRoot 'MapStylePolicyTest.java')
if($LASTEXITCODE -ne 0){throw 'Style test compile failed'}
& $taskJava -cp $taskClasses com.example.motonav.MapStylePolicyTest
if($LASTEXITCODE -ne 0){throw 'Style policy tests failed'}
