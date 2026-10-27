param(
    [Parameter(Mandatory=$true)][string]$KotlinStdlib,
    [string]$JavaPath = $env:JAVA_HOME
)
$ErrorActionPreference = 'Stop'
$java = if ($JavaPath) { Join-Path $JavaPath 'bin/java.exe' } else { 'java' }
$javac = if ($JavaPath) { Join-Path $JavaPath 'bin/javac.exe' } else { 'javac' }
$root = Split-Path -Parent $PSScriptRoot
$classes = Join-Path $root '.local/raster-test-classes'
$kotlin = Join-Path $root 'app/build/tmp/kotlin-classes/release'
$compiledJava = Join-Path $root 'app/build/intermediates/javac/release/compileReleaseJavaWithJavac/classes'
if (-not (Test-Path $kotlin)) { throw 'Build assembleRelease first.' }
if (-not (Test-Path $KotlinStdlib)) { throw 'Kotlin stdlib not found.' }
New-Item -ItemType Directory -Force $classes | Out-Null
$classpath = @($classes,$kotlin,$compiledJava,$KotlinStdlib) -join [IO.Path]::PathSeparator
& $javac -encoding UTF-8 -cp $classpath -d $classes (Join-Path $PSScriptRoot 'jvm/android/graphics/Bitmap.java') (Join-Path $PSScriptRoot 'AmapOnlyTest.java') (Join-Path $PSScriptRoot 'FourStyleRasterTest.java')
if ($LASTEXITCODE -ne 0) { throw 'Raster test compilation failed' }
& $java -cp $classpath com.example.motonav.AmapOnlyTest
if ($LASTEXITCODE -ne 0) { throw 'AMap tests failed' }
& $java -cp $classpath com.example.motonav.FourStyleRasterTest (Join-Path $root '.local/four-style-wire')
if ($LASTEXITCODE -ne 0) { throw 'Four-style tests failed' }
