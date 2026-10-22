param([string]$JavaPath = $env:JAVA_HOME)
$ErrorActionPreference = 'Stop'
$java = if ($JavaPath) { Join-Path $JavaPath 'bin/java.exe' } else { 'java' }
$javac = if ($JavaPath) { Join-Path $JavaPath 'bin/javac.exe' } else { 'javac' }
$root = Split-Path -Parent $PSScriptRoot
$classes = Join-Path $root '.local/test-classes'
$src = Join-Path $root 'app/src/main/java/com/example/motonav'
New-Item -ItemType Directory -Force $classes | Out-Null
$cases = @{
    CruiseIdleTimerTest = @('CruiseIdleTimer.java')
    MapStylePolicyTest = @('MapStylePolicy.java')
    WeatherAreaTest = @('WeatherArea.java')
    ArrivalPolicyTest = @('ArrivalPolicy.java')
    RideToolsTest = @('RideTrackMath.java', 'MapLoadStatus.java')
    MotorTrackTest = @('MotorTrack.java')
}
foreach ($test in $cases.Keys | Sort-Object) {
    $sources = @($cases[$test] | ForEach-Object { Join-Path $src $_ })
    & $javac -encoding UTF-8 -d $classes @sources (Join-Path $PSScriptRoot "$test.java")
    if ($LASTEXITCODE -ne 0) { throw "$test compile failed" }
    & $java -cp $classes "com.example.motonav.$test"
    if ($LASTEXITCODE -ne 0) { throw "$test failed" }
}
