param([Parameter(Mandatory)][string]$ZigPath,[Parameter(Mandatory)][string]$JavaHome)
$ErrorActionPreference='Stop'
$taskRoot=$PSScriptRoot
$taskZig=Split-Path -Parent $ZigPath
$taskOutput=Join-Path $taskRoot '..\app\src\main\jniLibs\arm64-v8a'
New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
$env:ZIG_GLOBAL_CACHE_DIR=Join-Path $taskRoot '.cache'
$taskStubs=Join-Path $taskRoot 'link-stubs'
New-Item -ItemType Directory -Path $taskStubs -Force | Out-Null
foreach($taskLib in @('dl','c')){
 $taskFlags=@('c++','-target','aarch64-linux-android','-shared','-nostdlib','-fPIC','-fno-builtin',('-Wl,-soname,lib'+$taskLib+'.so'))
 if($taskLib -eq 'dl'){$taskFlags+='-DSTUB_DL'}
 & $ZigPath @taskFlags (Join-Path $taskRoot 'link-stub.cpp') -o (Join-Path $taskStubs ('lib'+$taskLib+'.so'))
 if($LASTEXITCODE -ne 0){throw 'Android system dependency stub build failed'}
}
& $ZigPath c++ -target aarch64-linux-android -std=c++17 -O2 -fPIC -shared -nostdlib -nostdinc -nostdinc++ -fno-exceptions -fno-rtti -fno-stack-protector '-Wl,-soname,libmotonav_route.so' '-Wl,--no-as-needed' (Join-Path $taskStubs 'libdl.so') (Join-Path $taskStubs 'libc.so') -I (Join-Path $taskRoot 'include') -I (Join-Path $JavaHome 'include') -isystem (Join-Path $taskZig 'lib\include') (Join-Path $taskRoot 'route_bridge.cpp') -o (Join-Path $taskOutput 'libmotonav_route.so')
if($LASTEXITCODE -ne 0){throw 'Native route reader build failed'}
