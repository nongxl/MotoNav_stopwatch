param(
    [Parameter(Mandatory=$true)][string]$SdkPath,
    [Parameter(Mandatory=$true)][string]$KotlinStdlib,
    [Parameter(Mandatory=$true)][string]$JsonJar
)
$ErrorActionPreference='Stop'
$taskJava=if($env:JAVA_HOME){Join-Path $env:JAVA_HOME 'bin/java.exe'}else{'java'}
$taskJavac=if($env:JAVA_HOME){Join-Path $env:JAVA_HOME 'bin/javac.exe'}else{'javac'}
$taskRoot=Split-Path -Parent $PSScriptRoot
$taskClasses=Join-Path $taskRoot '.local/persistence-test-classes'
$taskKotlin=Join-Path $taskRoot 'app/build/tmp/kotlin-classes/release'
$taskJavaClasses=Join-Path $taskRoot 'app/build/intermediates/javac/release/compileReleaseJavaWithJavac/classes'
$taskAndroid=Join-Path $SdkPath 'platforms/android-35/android.jar'
foreach($taskItem in @($taskKotlin,$taskJavaClasses,$taskAndroid,$KotlinStdlib,$JsonJar)){
    if(!(Test-Path -LiteralPath $taskItem)){throw "Missing dependency or release build output: $taskItem"}
}
New-Item -ItemType Directory -Force $taskClasses | Out-Null
$taskClasspath=@($taskClasses,$taskKotlin,$taskJavaClasses,$KotlinStdlib,$JsonJar,$taskAndroid) -join [IO.Path]::PathSeparator
$taskAdapters=@(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'jvm') -Filter '*.java' -Recurse | ForEach-Object FullName)
& $taskJavac -encoding UTF-8 -cp $taskClasspath -d $taskClasses @taskAdapters (Join-Path $PSScriptRoot 'RidePersistenceTest.java')
if($LASTEXITCODE -ne 0){throw 'Persistence tests compile failed'}
$taskRun=Join-Path $taskRoot ('.local/ride-tests-'+[guid]::NewGuid().ToString('N'))
foreach($taskMode in @('normal','seed-crash','recover')){
    $taskDir=if($taskMode -eq 'normal'){'normal'}else{'crash'}
    & $taskJava -cp $taskClasspath com.example.motonav.RidePersistenceTest $taskMode (Join-Path $taskRun $taskDir)
    if($LASTEXITCODE -ne 0){throw "Persistence test failed: $taskMode"}
}
