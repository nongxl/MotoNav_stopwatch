$ErrorActionPreference='Stop'
$taskJdk=$env:JAVA_HOME
if(!$taskJdk){throw 'Please set JAVA_HOME to JDK 17 or newer.'}
$classes=Join-Path $PSScriptRoot '.local/bicycle-tests'
New-Item -ItemType Directory -Force $classes | Out-Null
$sources=@(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'tests/local-routing') -Recurse -Filter '*.java' | ForEach-Object FullName)
& (Join-Path $taskJdk 'bin/javac.exe') -encoding UTF-8 -d $classes @sources (Join-Path $PSScriptRoot 'app/src/main/java/com/example/motonav/LocalRouting.java')
if($LASTEXITCODE -ne 0){throw 'Bicycle routing tests compile failed'}
& (Join-Path $taskJdk 'bin/java.exe') -cp $classes com.example.motonav.LocalRoutingTest
if($LASTEXITCODE -ne 0){throw 'Bicycle routing tests failed'}
