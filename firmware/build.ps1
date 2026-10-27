param([ValidateSet('175','185b','stopwatch')][string]$Board='175',
      [ValidateSet(0,1,2)][int]$PanelVariant=0,
      [string]$CliPath='arduino-cli',[string]$ConfigFile='',
      [ValidateRange(1,32)][int]$Jobs=4)
$ErrorActionPreference='Stop'
if($Board -in @('175','stopwatch') -and $PanelVariant -ne 0){throw "$Board has only panel 0."}
$taskRoot=$PSScriptRoot
$taskBuild=Join-Path $taskRoot ".build/$Board"
$taskSketch=Join-Path $taskBuild 'MotoNav'
$taskOutput=Join-Path $taskBuild 'output'
New-Item -ItemType Directory -Force $taskSketch,$taskOutput | Out-Null
$taskSource=Join-Path $taskRoot "boards/$Board/MotoNav"
Get-ChildItem -LiteralPath $taskSource -File -Recurse | Where-Object Extension -in '.ino','.h','.cpp','.c' | ForEach-Object {
    $taskRelative=$_.FullName.Substring($taskSource.Length).TrimStart([char[]]'\/')
    $taskTarget=Join-Path $taskSketch $taskRelative
    New-Item -ItemType Directory -Force (Split-Path -Parent $taskTarget) | Out-Null
    Copy-Item -LiteralPath $_.FullName -Destination $taskTarget -Force
}
if($Board -eq '185b'){
    $taskPin=Join-Path $taskSketch 'pin_config.h'
    $taskText=[IO.File]::ReadAllText($taskPin)
    $taskText=[regex]::Replace($taskText,'#define LCD_INIT_VARIANT [0-9]+',"#define LCD_INIT_VARIANT $PanelVariant")
    [IO.File]::WriteAllText($taskPin,$taskText,[Text.UTF8Encoding]::new($false))
}
$taskArgs=@()
if($ConfigFile){$taskArgs+=@('--config-file',$ConfigFile)}
$taskHomePrefix=[Environment]::GetFolderPath('UserProfile').Replace('\','/')
$taskPrefixMap="-ffile-prefix-map=$taskHomePrefix=/builder"
$taskArgs+=@('compile','--jobs',"$Jobs",'--libraries',(Join-Path $taskRoot 'libraries'),
    '--build-path',(Join-Path $taskBuild 'build'),
    '--fqbn','esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,CDCOnBoot=cdc',
    '--build-property',"compiler.c.extra_flags=$taskPrefixMap",
    '--build-property',"compiler.cpp.extra_flags=$taskPrefixMap",
    '--output-dir',$taskOutput,$taskSketch)
if($Board -eq '175'){$taskArgs+=@('--build-property','compiler.c.elf.extra_flags=-Wl,--wrap=xTaskCreatePinnedToCore')}
& $CliPath @taskArgs
if($LASTEXITCODE -ne 0){throw "Arduino build failed ($LASTEXITCODE)"}
$taskDest=Join-Path $taskRoot "dist/$Board/panel$PanelVariant"
$taskLabel=switch($Board){'175'{'1.75'}'185b'{'1.85B'}'stopwatch'{'stopwatch'}}
$taskVersionSource=Join-Path $taskSketch 'src/core/FirmwareConfig.h'
if(!(Test-Path -LiteralPath $taskVersionSource)){$taskVersionSource=Join-Path $taskSketch 'MotoNav.ino'}
$taskVersion=[regex]::Match([IO.File]::ReadAllText($taskVersionSource),'^#define FW_VERSION "v([^\"]+)"',[Text.RegularExpressions.RegexOptions]::Multiline).Groups[1].Value
if(!$taskVersion){throw 'Cannot read firmware version'}
$taskName="MotoNav_universal_${taskLabel}_v${taskVersion}_panel$PanelVariant"
$taskApp=[IO.File]::ReadAllBytes((Join-Path $taskOutput 'MotoNav.ino.bin'))
$taskMerged=[IO.File]::ReadAllBytes((Join-Path $taskOutput 'MotoNav.ino.merged.bin'))
$taskLength=0x10000+$taskApp.Length
if($taskMerged.Length -lt $taskLength){throw 'Merged image is shorter than the application image.'}
$taskSegment=[byte[]]::new($taskApp.Length)
[Array]::Copy($taskMerged,0x10000,$taskSegment,0,$taskApp.Length)
$taskHasher=[Security.Cryptography.SHA256]::Create()
try {
    if([Convert]::ToBase64String($taskHasher.ComputeHash($taskSegment)) -ne [Convert]::ToBase64String($taskHasher.ComputeHash($taskApp))){throw 'Unexpected application offset in merged image.'}
} finally {$taskHasher.Dispose()}
# Core 3.3.10 exports a 16 MiB merged image. Drop only padding after the app;
# keep every byte of bootloader, partition table, boot data and the app itself.
$taskFull=[byte[]]::new($taskLength)
[Array]::Copy($taskMerged,0,$taskFull,0,$taskLength)
[IO.File]::WriteAllBytes((Join-Path $taskDest "${taskName}_app_0x10000.bin"),$taskApp)
[IO.File]::WriteAllBytes((Join-Path $taskDest "${taskName}_full_0x0.bin"),$taskFull)
# Integrity manifest consumed by the Mac flashing entry point; UTF-8 without BOM.
if($Board -in @('175','stopwatch')) {
    $taskBoardDesc=switch($Board){'175'{'Waveshare ESP32-S3-Touch-AMOLED-1.75'}'stopwatch'{'M5Stack Stopwatch'}}
    $taskManifestFiles=@()
    foreach($taskSuffix in @('app_0x10000','full_0x0')) {
        $taskFileName="${taskName}_${taskSuffix}.bin"
        $taskFilePath=Join-Path $taskDest $taskFileName
        $taskManifestFiles+=@{file=$taskFileName;bytes=(Get-Item -LiteralPath $taskFilePath).Length;sha256=(Get-FileHash -LiteralPath $taskFilePath -Algorithm SHA256).Hash.ToLowerInvariant()}
    }
    $taskManifest=@{firmware=$taskVersion;platform='universal';board=$taskBoardDesc;esp32Core='3.3.10';flashSize='16MB';partition='app3M_fat9M_16MB';verifiedApplicationOffset='0x10000';deviceTested=$false;files=$taskManifestFiles}
    [IO.File]::WriteAllText((Join-Path $taskDest "${taskName}_manifest.json"),($taskManifest | ConvertTo-Json -Depth 5),[Text.UTF8Encoding]::new($false))
}
Get-ChildItem -LiteralPath $taskDest -File | Get-FileHash -Algorithm SHA256 | Format-Table -AutoSize
