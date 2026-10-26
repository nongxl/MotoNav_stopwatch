"""Publish the verified original firmware package without recompiling its images."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import subprocess
import zipfile

ROOT=Path(__file__).resolve().parents[1]
sha=lambda data: hashlib.sha256(data).hexdigest()

def require(value,message):
    if not value: raise ValueError(message)

def run(*args,check=True):
    result=subprocess.run([str(x) for x in args],cwd=ROOT,capture_output=True)
    if check and result.returncode:
        raise RuntimeError(result.stdout.decode('utf-8',errors='replace')+result.stderr.decode('utf-8',errors='replace'))
    return result

def verify_windows(package):
    command=['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(package/'flash.ps1'),'-VerifyOnly']
    def check_case(label,success):
        result=run(*command,check=False)
        require((result.returncode==0)==success,label+' failed: '+result.stdout.decode('utf-8',errors='replace'))
        print(label+': PASS',flush=True)
    check_case('Fresh package and bundled esptool',True)
    manifest=json.loads((package/'firmware-manifest.json').read_text(encoding='utf-8-sig'))
    app=package/manifest['boards'][0]['versions'][0]['panels'][0]['app']['path']
    data=app.read_bytes()
    try:
        bad=bytearray(data);bad[80]^=1;app.write_bytes(bad)
        check_case('Corrupted firmware is rejected',False)
    finally: app.write_bytes(data)
    manifest_path=package/'firmware-manifest.json'
    original=manifest_path.read_bytes()
    try:
        manifest['boards'][0]['versions'][0]['panels'][0]['app']['path']='../outside.bin'
        manifest_path.write_text(json.dumps(manifest),encoding='utf-8')
        check_case('Outside firmware path is rejected',False)
    finally: manifest_path.write_bytes(original)
    check_case('Restored package is valid',True)

def prepare():
    version=(ROOT/'firmware/RELEASE_VERSION').read_text().strip()
    require(re.fullmatch(r'\d+\.\d+\.\d+(?:-[A-Za-z0-9][A-Za-z0-9.-]*)?',version),'Invalid firmware version')
    source_path=ROOT/'firmware/boards/175/MotoNav/src/core/FirmwareConfig.h'
    if not source_path.exists(): source_path=ROOT/'firmware/boards/175/MotoNav/MotoNav.ino'
    source=source_path.read_text(encoding='utf-8-sig')
    require(f'#define FW_VERSION "v{version}"' in source,'Source/release version mismatch')
    inputs=ROOT/'firmware/releases'/version
    manifest=json.loads((inputs/'manifest.json').read_text(encoding='utf-8'))
    require(manifest['version']==version,'Payload version mismatch')
    output=ROOT/'output/firmware'/version
    output.mkdir(parents=True,exist_ok=True)
    archive=output/manifest['package']
    require(Path(manifest['package']).name==manifest['package'],'Unsafe package name')
    with archive.open('wb') as stream:
        for part in manifest['parts']:
            require(Path(part['path']).name==part['path'],'Unsafe input path')
            data=(inputs/part['path']).read_bytes()
            require(len(data)==part['bytes'] and sha(data)==part['sha256'],'Input part checksum mismatch')
            stream.write(data)
    original_zip=archive.read_bytes()
    require(len(original_zip)==manifest['bytes'] and sha(original_zip)==manifest['sha256'],'Original archive checksum mismatch')
    extracted=output/'verify'
    package_name=archive.stem
    with zipfile.ZipFile(archive) as z:
        require(z.testzip() is None,'Invalid ZIP CRC')
        for name in z.namelist():
            target=(extracted/name).resolve()
            require(target.is_relative_to(extracted.resolve()),'Unsafe ZIP member')
        z.extractall(extracted)
    package=extracted/package_name
    verify_windows(package)
    for path in (ROOT/'firmware/windows-flasher').iterdir():
        if path.is_file():
            require(path.read_text(encoding='utf-8-sig')==(package/path.name).read_text(encoding='utf-8-sig'),'Flasher script differs from checked-in source: '+path.name)
    board_manifest=json.loads((package/'firmware-manifest.json').read_text(encoding='utf-8'))
    board=board_manifest['boards'][0]
    release=board['versions'][0]
    require(board['id']=='175' and release['version']==version,'Board/version mismatch')
    panel=release['panels'][0]
    image_data={}
    files=[]
    for kind in ('app','full'):
        item=panel[kind]; data=(package/item['path']).read_bytes()
        require(len(data)==item['bytes'] and sha(data)==item['sha256'].lower(),'Image checksum mismatch')
        offset=0 if kind=='app' else 0x10000
        require(data[offset]==0xe9 and struct.unpack_from('<H',data,offset+12)[0]==9,'Not ESP32-S3 firmware')
        require(('v'+version+'\0').encode() in data,'Wrong binary version')
        image_data[kind]=data
        files.append(dict(file=Path(item['path']).name,bytes=len(data),sha256=sha(data)))
    require(len(image_data['app'])<=0x300000 and image_data['full'][0x10000:]==image_data['app'],'Invalid partition size/application offset')
    flat=dict(firmware=version,platform='universal',board='Waveshare ESP32-S3-Touch-AMOLED-1.75',
        esp32Core='3.3.10',flashSize='16MB',partition='app3M_fat9M_16MB',verifiedApplicationOffset='0x10000',deviceTested=False,files=files)
    firmware_zip=output/f'MotoNav-universal-firmware-{version}.zip'
    with zipfile.ZipFile(firmware_zip,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for kind,item in zip(('app','full'),files): z.writestr('firmware/dist/175/panel0/'+item['file'],image_data[kind])
        z.writestr(f'firmware/dist/175/panel0/MotoNav_universal_1.75_v{version}_panel0_manifest.json',json.dumps(flat,indent=2)+'\n')
    source_zip=output/f'MotoNav-Firmware-Source-1.75-v{version}-universal.zip'
    source_files=[]
    for base in ('firmware/boards/175','firmware/libraries','firmware/tests','firmware/windows-flasher','licenses'):
        source_files.extend(p for p in (ROOT/base).rglob('*') if p.is_file() and not any(s in p.relative_to(ROOT).parts for s in ('build','.build','__pycache__')) and p.suffix!='.pyc')
    for path in ('LICENSE.md','firmware/README.md','firmware/build.ps1','firmware/build.py','firmware/flash_macos.py','一键烧录固件.command',f'docs/RELEASE-FIRMWARE-{version}.md'):
        if (ROOT/path).is_file(): source_files.append(ROOT/path)
    with zipfile.ZipFile(source_zip,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for path in sorted(set(source_files)):
            rel=path.relative_to(ROOT).as_posix()
            info=zipfile.ZipInfo('MotoNav/'+rel,date_time=(1980,1,1,0,0,0))
            info.compress_type=zipfile.ZIP_DEFLATED
            info.external_attr=(0o100755 if path.suffix=='.command' else 0o100644)<<16
            z.writestr(info,path.read_bytes())
    notes=ROOT/f'docs/RELEASE-FIRMWARE-{version}.md'
    shutil.copyfile(notes,output/'RELEASE-NOTES.md')
    shutil.copyfile(ROOT/'firmware/README.md',output/'FIRMWARE.md')
    assets=[archive,firmware_zip,source_zip,output/'RELEASE-NOTES.md',output/'FIRMWARE.md']
    sums=output/'SHA256SUMS.txt'
    sums.write_text(''.join(sha(p.read_bytes())+'  '+p.name+'\n' for p in assets),encoding='ascii')
    assets.append(sums)
    print('Original package SHA256: '+sha(archive.read_bytes()),flush=True)
    return version,notes,assets

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--prepare-only',action='store_true',help='Verify/package locally without GitHub writes')
    args=parser.parse_args()
    version,notes,assets=prepare()
    if args.prepare_only: return
    commit=os.environ['GITHUB_SHA']
    tag='firmware-v'+version
    existing=run('gh','release','view',tag,'--json','targetCommitish',check=False)
    if existing.returncode==0:
        require(json.loads(existing.stdout)['targetCommitish']==commit,'Existing release points to a different commit; refusing to replace it')
    else:
        run('gh','release','create',tag,'--target',commit,'--title',f'MotoNav 圆屏 {version} · 安卓/iOS通用','--notes-file',notes,'--draft')
    run('gh','release','upload',tag,*assets,'--clobber')
    run('gh','release','edit',tag,'--notes-file',notes,'--draft=false','--latest=false')
    print('Published '+tag,flush=True)

if __name__=='__main__': main()
