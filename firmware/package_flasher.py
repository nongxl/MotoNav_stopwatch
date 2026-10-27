#!/usr/bin/env python3
"""Package existing, verified 1.75 images. Does not connect to or flash hardware."""
from pathlib import Path
import argparse, hashlib, importlib.util, json, os, re, shutil, zipfile

ROOT=Path(__file__).resolve().parents[1]
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tools-dir',required=True,type=Path,help='Directory containing esptool.exe and its license notices')
    parser.add_argument('--validation',type=Path,help='Optional verification record pinned to this exact app image SHA256')
    args=parser.parse_args()
    source=ROOT/'firmware/boards/175/MotoNav'
    version=re.search(r'^#define FW_VERSION "v([^\"]+)"',(source/'src/core/FirmwareConfig.h').read_text('utf8'),re.M)[1]
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:-[A-Za-z0-9][A-Za-z0-9.-]*)?',version):raise ValueError('Unsafe firmware version')
    if 'rebel' in version.lower() or (source/'resources/RebelBootAsset.h').exists():raise ValueError('Public package must use generic boot resources')
    if 'RebelBootAsset' in (source/'BootAnimation.h').read_text('utf8'):raise ValueError('Private boot include found')
    dist=ROOT/'firmware/dist/175/panel0';prefix=f'MotoNav_universal_1.75_v{version}_panel0'
    manifest=dist/(prefix+'_manifest.json');meta=json.loads(manifest.read_text('utf8'))
    images={}
    for item in meta['files']:
        if Path(item['file']).name!=item['file']:raise ValueError('Unsafe image name')
        path=dist/item['file']
        if path.stat().st_size!=item['bytes'] or sha(path)!=item['sha256']:raise ValueError('Image hash/size mismatch')
        images['app' if '_app_' in path.name else 'full']=path
    if images['full'].read_bytes()[0x10000:]!=images['app'].read_bytes():raise ValueError('Unexpected application offset')
    out=ROOT/'output/firmware'/version;out.mkdir(parents=True,exist_ok=True)
    name=f'MotoNav-1.75-v{version}-Universal-Flasher';package=out/name
    if package.exists():
        if not package.resolve().is_relative_to(out.resolve()):raise ValueError('Unsafe output path')
        shutil.rmtree(package)
    shutil.copytree(ROOT/'firmware/windows-flasher',package)
    shutil.copytree(args.tools_dir.resolve(),package/'tools')
    if not (package/'tools/esptool.exe').is_file():raise ValueError('Missing tools/esptool.exe')
    panel={'label':'1.75 AMOLED 原面板'}
    for kind,path in images.items():
        dest=package/'firmware/panel0'/path.name;dest.parent.mkdir(exist_ok=True,parents=True);shutil.copy2(path,dest)
        panel[kind]={'path':dest.relative_to(package).as_posix(),'bytes':dest.stat().st_size,'sha256':sha(dest).upper()}
    bundle={'schema':1,'platform':'universal','boards':[{'id':'175','label':'微雪 ESP32-S3-Touch-AMOLED-1.75（466×466）',
        'versions':[{'label':f'v{version} · 通用开机 · 固定页面与时钟 · 启动稳定性修复','version':version,'panels':[panel]}]}]}
    (package/'firmware-manifest.json').write_text(json.dumps(bundle,ensure_ascii=False,indent=2)+'\n','utf8')
    verification={'firmware':version,'genericBoot':True,'personalRebelAssetIncluded':False,
        'imageHashesVerified':True,'publicBinaryFlashedOnDevice':False}
    if args.validation:
        supplied=json.loads(args.validation.read_text('utf8'))
        if supplied['firmware']!=version or supplied['publicAppSHA256']!=sha(images['app']):
            raise ValueError('Validation record is not pinned to this exact firmware')
        verification.update(supplied)
    (package/'firmware-verification.json').write_text(json.dumps(verification,indent=2)+'\n','utf8')
    if os.name=='nt':
        spec=importlib.util.spec_from_file_location('release_verify',ROOT/'scripts/release-firmware.py')
        module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module);module.verify_windows(package)
    sums=package/'SHA256SUMS.txt'
    sums.write_text(''.join(sha(p)+'  '+p.relative_to(package).as_posix()+'\n' for p in sorted(package.rglob('*')) if p.is_file() and p!=sums),'utf8')
    archive=out/(name+'.zip')
    with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for p in sorted(package.rglob('*')):
            if p.is_file():z.write(p,p.relative_to(out).as_posix())
    source_zip=out/f'MotoNav-Firmware-Source-1.75-v{version}.zip'
    with zipfile.ZipFile(source_zip,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for base in ['firmware/boards/175','firmware/libraries','firmware/tests','firmware/windows-flasher','licenses']:
            for p in sorted((ROOT/base).rglob('*')):
                if p.is_file() and not any(x in p.parts for x in ['__pycache__','.build','build']) and p.suffix not in ['.pyc','.exe','.pdb']:
                    z.write(p,'MotoNav/'+p.relative_to(ROOT).as_posix())
        for rel in ['LICENSE.md','firmware/README.md','firmware/build.py','firmware/build.ps1','firmware/package_flasher.py','firmware/flash_macos.py','一键烧录固件.command',f'docs/RELEASE-FIRMWARE-{version}.md']:
            z.write(ROOT/rel,'MotoNav/'+rel)
    flat=out/f'MotoNav-universal-firmware-{version}.zip'
    with zipfile.ZipFile(flat,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as z:
        for p in [*images.values(),manifest]:z.write(p,'firmware/dist/175/panel0/'+p.name)
    # Loose images are useful for existing flashing tools, without redownloading a whole tool bundle.
    assets=[archive,source_zip,flat]
    for p in [*images.values(),manifest]:
        dest=out/p.name;shutil.copy2(p,dest);assets.append(dest)
    notes=out/'INSTALL-1.75.txt';shutil.copy2(package/'使用说明.txt',notes);assets.append(notes)
    sums=out/'SHA256SUMS.txt';sums.write_text(''.join(sha(p)+'  '+p.name+'\n' for p in assets),'ascii');assets.append(sums)
    record={p.name:{'bytes':p.stat().st_size,'sha256':sha(p)} for p in assets}
    (out/'release-assets.json').write_text(json.dumps({'version':version,'files':record},ensure_ascii=False,indent=2)+'\n','utf8')
    print(json.dumps({'firmware':version,'out':str(out),'assets':list(record)},ensure_ascii=False))

if __name__=='__main__':main()
