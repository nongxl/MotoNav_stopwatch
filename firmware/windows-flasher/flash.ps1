param([switch]$VerifyOnly)
$ErrorActionPreference='Stop'
$taskRoot=$PSScriptRoot
$taskEsp=Join-Path $taskRoot 'tools\esptool.exe'

function Get-PackageSha256([string]$Path) {
    $taskHasher=[Security.Cryptography.SHA256]::Create()
    $taskStream=[IO.File]::OpenRead($Path)
    try{return [BitConverter]::ToString($taskHasher.ComputeHash($taskStream)).Replace('-','')}
    finally{$taskStream.Dispose();$taskHasher.Dispose()}
}

function Resolve-PackageFile($taskEntry,[bool]$Full) {
    $taskPath=[IO.Path]::GetFullPath((Join-Path $taskRoot $taskEntry.path))
    $taskPrefix=[IO.Path]::GetFullPath((Join-Path $taskRoot 'firmware')).TrimEnd('\')+'\'
    if(-not $taskPath.StartsWith($taskPrefix,[StringComparison]::OrdinalIgnoreCase)){throw '固件路径超出本包firmware目录。'}
    if(-not(Test-Path -LiteralPath $taskPath -PathType Leaf)){throw '缺少固件，请完整解压ZIP后重试。'}
    if((Get-Item -LiteralPath $taskPath).Length -ne $taskEntry.bytes -or
       (Get-PackageSha256 $taskPath) -ne $taskEntry.sha256){throw '固件大小或校验码不符，已停止。'}
    $taskData=[IO.File]::ReadAllBytes($taskPath)
    $taskOffset=if($Full){0x10000}else{0}
    if($taskData.Length -le $taskOffset+24 -or $taskData[$taskOffset] -ne 0xE9 -or
       [BitConverter]::ToUInt16($taskData,$taskOffset+12) -ne 9){throw '不是有效的ESP32-S3应用镜像。'}
    if($Full -and ($taskData[0] -ne 0xE9 -or $taskData[0x8000] -ne 0xAA -or $taskData[0x8001] -ne 0x50)){throw '完整包缺少启动程序或分区表。'}
    return $taskPath
}

function Invoke-Burn([string]$Baud,[bool]$Manual=$false) {
    # Recheck immediately before each attempt; the upgrade never erases NVS.
    $taskSelected=if($taskFull){$taskPanel.full}else{$taskPanel.app}
    Resolve-PackageFile $taskSelected $taskFull | Out-Null
    $taskBefore=if($Manual){'no-reset'}else{'default-reset'}
    $taskArgs=@('--chip','esp32s3','--port',$taskPort,'--baud',$Baud,
        '--before',$taskBefore,'--after','watchdog-reset','write-flash')
    if($taskFull){$taskArgs+='--erase-all'}
    $taskArgs+=@($taskAddress,$taskFile)
    & $taskEsp @taskArgs | Out-Host
    return ($LASTEXITCODE -eq 0)
}

try {
    $taskManifest=Get-Content -LiteralPath (Join-Path $taskRoot 'firmware-manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    if($taskManifest.schema -ne 1 -or $taskManifest.boards.Count -ne 1 -or $taskManifest.boards[0].id -ne '175'){throw '本包仅支持微雪1.75。'}
    $taskBoard=$taskManifest.boards[0]
    $taskVersion=$taskBoard.versions[0]
    if($taskVersion.version -ne '7.12.9-junction.11'){throw '固件清单版本不匹配。'}
    $taskPanel=$taskVersion.panels[0]
    $taskApp=Resolve-PackageFile $taskPanel.app $false
    $taskFullFile=Resolve-PackageFile $taskPanel.full $true
    if(-not(Test-Path -LiteralPath $taskEsp -PathType Leaf)){throw '缺少tools\esptool.exe，请完整解压ZIP。'}
    Write-Host ''
    Write-Host 'MotoNav 1.75 通用固件 7.12.9-junction.11 一键烧录包' -ForegroundColor Magenta
    Write-Host '通用品牌开机；连续转弯环；安卓/iOS通用协议' -ForegroundColor Cyan
    if($VerifyOnly){
        & $taskEsp version | Out-Host
        if($LASTEXITCODE -ne 0){throw '烧录工具无法启动。'}
        Write-Host 'PASS：板型、版本、两份镜像大小/SHA256/格式及工具检查通过；未访问串口、未烧录。' -ForegroundColor Green
        exit 0
    }
    Write-Host '只适用于微雪1.75 AMOLED；不能刷到1.85B或M5Stack StopWatch。'
    Write-Host '  1. 已有同板型、同分区MotoNav：升级，保留设置和配对（默认）'
    Write-Host '  2. 首次安装/救砖：完整刷写，整片擦除，会清空设置和配对'
    while($true){
        $taskMode=(Read-Host '输入1或2；回车默认升级；Q退出').Trim()
        if($taskMode -eq 'Q'){exit 0}
        if(!$taskMode -or $taskMode -eq '1' -or $taskMode -eq '2'){break}
    }
    $taskFull=$taskMode -eq '2'
    $taskFile=if($taskFull){$taskFullFile}else{$taskApp}
    $taskAddress=if($taskFull){'0x0'}else{'0x10000'}
    $taskPorts=@([IO.Ports.SerialPort]::GetPortNames() | Sort-Object)
    Write-Host ('当前串口：'+($taskPorts -join '、'))
    if($taskPorts.Count -eq 0){throw '没有检测到串口。请用USB数据线连接圆屏后重试。'}
    $taskPort=(Read-Host '输入圆屏的COM号，例如COM4；Q退出').Trim().ToUpperInvariant()
    if($taskPort -eq 'Q'){exit 0}
    if($taskPort -notmatch '^COM[0-9]+$' -or $taskPorts -notcontains $taskPort){throw '串口无效，未执行刷写。'}
    Write-Host ('板型：'+$taskBoard.label)
    Write-Host ('固件：'+$taskVersion.version+'；串口：'+$taskPort+'；写入：'+$taskAddress)
    if($taskFull){
        Write-Host '完整刷写会整片擦除！如需保留现有程序，请先运行备份原厂。' -ForegroundColor Yellow
        if((Read-Host '确认整片擦除并完整安装，请输入YES；其他输入取消') -ne 'YES'){exit 0}
    }elseif((Read-Host '确认板型、串口和升级方式无误后，输入Y开始') -ne 'Y'){exit 0}
    $taskOK=Invoke-Burn '460800'
    if(-not $taskOK){Write-Host '降速至115200重试。';$taskOK=Invoke-Burn '115200'}
    if(-not $taskOK){
        Write-Host '按住BOOT，再复位或重新插USB，进入下载模式后松开BOOT。'
        if((Read-Host '准备好后回车重试；Q退出') -eq 'Q'){exit 1}
        $taskPort=(Read-Host '输入此时实际COM号').Trim().ToUpperInvariant()
        if($taskPort -notmatch '^COM[0-9]+$' -or @([IO.Ports.SerialPort]::GetPortNames()) -notcontains $taskPort){throw '串口无效。'}
        $taskOK=Invoke-Burn '115200' $true
    }
    if(-not $taskOK){throw '烧录失败，请保留窗口错误信息，检查数据线和串口占用。'}
    Write-Host '烧录完成，圆屏将重新启动。' -ForegroundColor Green
    exit 0
}catch{
    Write-Host ('操作未完成：'+$_.Exception.Message) -ForegroundColor Red
    exit 1
}
