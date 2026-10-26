#!/usr/bin/env python3
"""Windows-only integration build. Supply the exact patched car APK yourself."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent


def sha(data):
    return hashlib.sha256(data).hexdigest()


def patch_car():
    spec = json.loads((ROOT / 'car-integration-deltas.json').read_text('utf8'))
    for item in spec['files']:
        path = ROOT / 'car-code/smali' / item['path']
        text = path.read_text('utf8')
        if sha(text.encode()) == item['afterSHA256']:
            continue
        if sha(text.encode()) != item['beforeSHA256']:
            raise RuntimeError('Unexpected car source: ' + item['path'])
        lines = text.splitlines(keepends=True)
        for edit in reversed(item['edits']):
            if lines[edit['start']:edit['end']] != edit['old']:
                raise RuntimeError('Car integration preimage mismatch')
            lines[edit['start']:edit['end']] = edit['new']
        text = ''.join(lines)
        if sha(text.encode()) != item['afterSHA256']:
            raise RuntimeError('Car integration result mismatch')
        path.write_text(text, encoding='utf8', newline='\n')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--provider', required=True, type=Path)
    parser.add_argument('--apktool', required=True, type=Path)
    parser.add_argument('--gradle', required=True, type=Path)
    parser.add_argument('--sdk', required=True, type=Path)
    parser.add_argument('--java-home', required=True, type=Path)
    parser.add_argument('--offline', action='store_true')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('This integration entry point currently supports Windows only.')
    spec = json.loads((ROOT / 'car-integration-deltas.json').read_text('utf8'))
    if sha(args.provider.read_bytes()) != spec['providerSHA256']:
        parser.error('The supplied APK does not match the tested patched 9.5.13 input; refusing to patch another version.')
    os.environ['JAVA_HOME'] = str(args.java_home.resolve())
    os.environ['ANDROID_SDK_ROOT'] = str(args.sdk.resolve())
    os.environ['ANDROID_HOME'] = os.environ['ANDROID_SDK_ROOT']
    java = args.java_home.resolve() / 'bin/java.exe'
    bt = args.sdk.resolve() / 'build-tools/35.0.0'
    apktool = args.apktool.resolve()
    def run(argv, cwd=ROOT):
        subprocess.run([str(x) for x in argv], cwd=cwd, check=True)
    def tool(*argv):
        run([java, '-Xmx768m', '-jar', apktool, *argv])

    inputs = ROOT / 'inputs'
    inputs.mkdir(exist_ok=True)
    target = inputs / 'provider.apk'
    if args.provider.resolve() != target.resolve():
        shutil.copyfile(args.provider, target)
    # Re-decode on each build so stale generated code cannot alter the result.
    tool('d', '-f', target, '-o', ROOT / 'car-resources', '-p', ROOT / 'framework')
    tool('d', '-r', '-f', target, '-o', ROOT / 'car-code', '-p', ROOT / 'framework')
    smali = ROOT / 'car-code/smali'
    for path in smali.rglob('*.smali'):
        text = path.read_text('utf8').replace('androidx/', 'carxlibx/').replace('androidx.', 'carxlibx.')
        text = text.replace('android/support/', 'carolds/support/').replace('android.support.', 'carolds.support.')
        path.write_text(text, encoding='utf8', newline='\n')
    (smali / 'androidx').rename(smali / 'carxlibx')
    if (smali / 'android/support').exists():
        (smali / 'carolds').mkdir(exist_ok=True)
        (smali / 'android/support').rename(smali / 'carolds/support')
    patch_car()
    gradle_args = [args.gradle.resolve(), '--no-daemon', '--max-workers=2']
    if args.offline:
        gradle_args.append('--offline')
    run([*gradle_args, 'assembleDebug'], ROOT / 'android')
    moto = ROOT / 'android/app/build/outputs/apk/debug/app-debug.apk'
    tool('d', '-r', '-f', moto, '-o', ROOT / 'moto-code', '-p', ROOT / 'framework')
    for name in ['moto-code', 'car-code']:
        tool('b', ROOT / name, '-o', ROOT / (name + '.apk'), '-p', ROOT / 'framework')
    for name in ['prepare-merge.py', 'compile-manifest.py', 'pack.py']:
        run([sys.executable, ROOT / name])
    key = ROOT / 'android/.local/debug.keystore'
    out = ROOT / 'output'
    out.mkdir(exist_ok=True)
    aligned = out / 'fusion-aligned.apk'
    final = out / 'MotoNav-8.3.26-fusion.21.apk'
    run([bt / 'zipalign.exe', '-f', '4', ROOT / 'fusion-unsigned.apk', aligned])
    run([bt / 'apksigner.bat', 'sign', '--ks', key, '--ks-pass', 'pass:android',
         '--key-pass', 'pass:android', '--ks-key-alias', 'androiddebugkey', '--out', final, aligned])
    run([bt / 'apksigner.bat', 'verify', '--print-certs', final])
    run([bt / 'zipalign.exe', '-c', '4', final])
    print('Package: com.autonavi.amapauto; certificate SHA1 is printed above.')
    print('Keep android/.local/debug.keystore privately to sign future updates with the same identity.')
    print(final)


if __name__ == '__main__':
    main()
