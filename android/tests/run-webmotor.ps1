param(
    [Parameter(Mandatory=$true)][string]$KotlinStdlib,
    [Parameter(Mandatory=$true)][string]$JsonJar,
    [string]$JavaPath = $env:JAVA_HOME
)
$ErrorActionPreference = 'Stop'
$java = if ($JavaPath) { Join-Path $JavaPath 'bin/java.exe' } else { 'java' }
$javac = if ($JavaPath) { Join-Path $JavaPath 'bin/javac.exe' } else { 'javac' }
$root = Split-Path -Parent $PSScriptRoot
$classes = Join-Path $root '.local/webmotor-test-classes'
$kotlin = Join-Path $root 'app/build/tmp/kotlin-classes/release'
$compiledJava = Join-Path $root 'app/build/intermediates/javac/release/compileReleaseJavaWithJavac/classes'
if (-not (Test-Path $kotlin)) { throw 'Build assembleRelease first.' }
foreach ($file in @($KotlinStdlib, $JsonJar)) { if (-not (Test-Path $file)) { throw "Missing test dependency: $file" } }
New-Item -ItemType Directory -Force $classes | Out-Null
$classpath = @($classes,$kotlin,$compiledJava,$KotlinStdlib,$JsonJar) -join [IO.Path]::PathSeparator
& $javac -encoding UTF-8 -cp $classpath -d $classes (Join-Path $PSScriptRoot 'jvm/android/net/Uri.java') (Join-Path $PSScriptRoot 'WebMotorRouteTest.java')
if ($LASTEXITCODE -ne 0) { throw 'WebMotor test compilation failed' }
& $java -cp $classpath com.example.motonav.WebMotorRouteTest
if ($LASTEXITCODE -ne 0) { throw 'WebMotor tests failed' }
