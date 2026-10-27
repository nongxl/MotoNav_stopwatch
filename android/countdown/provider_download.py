"""Fetch the pinned countdown build input; never install or run the input APK."""
import argparse
import hashlib
import json
from pathlib import Path
import tempfile
import time
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parent


def read_spec():
    spec = json.loads((ROOT / 'provider.json').read_text(encoding='utf-8'))
    integration = json.loads((ROOT / 'provider-integration-deltas.json').read_text(encoding='utf-8'))
    if spec['sha256'] != integration['providerSHA256']:
        raise ValueError('Provider download and integration SHA256 do not agree.')
    if not spec['url'].startswith('https://'):
        raise ValueError('Provider download requires HTTPS.')
    return spec


def verify(path, spec):
    path = Path(path)
    if not path.is_file():
        raise ValueError(f'Provider APK not found: {path}')
    if path.stat().st_size != spec['bytes']:
        raise ValueError(f'Provider APK size mismatch: {path}. Expected {spec["bytes"]} bytes.')
    digest = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    if digest.hexdigest() != spec['sha256']:
        raise ValueError(f'Provider APK SHA256 mismatch: {path}. No integration was applied.')
    return path.resolve()


def resolve_provider(provider=None, *, offline=False):
    spec = read_spec()
    # Explicit user files and existing cache entries must pass validation unchanged.
    if provider is not None:
        return verify(provider, spec)
    destination = ROOT / 'inputs' / spec['fileName']
    if destination.exists():
        print(f'Checking cached provider: {destination}', flush=True)
        return verify(destination, spec)
    if offline:
        raise ValueError(f'Offline mode: missing {destination}. Run once online or pass --provider PATH.')
    destination.parent.mkdir(parents=True, exist_ok=True)
    print(f'Downloading provider.apk (about 148 MiB) to {destination}', flush=True)
    last_error = None
    for attempt in range(1, 4):
        temporary = None
        try:
            request = Request(spec['url'], headers={'User-Agent': 'MotoNav-Countdown-Builder/1.0'})
            with urlopen(request, timeout=90) as response, tempfile.NamedTemporaryFile(
                    dir=destination.parent, prefix='provider-', suffix='.part', delete=False) as output:
                temporary = Path(output.name)
                total = 0
                next_progress = 20 * 1024 * 1024
                while True:
                    block = response.read(1024 * 1024)
                    if not block:
                        break
                    total += len(block)
                    if total > spec['bytes']:
                        raise ValueError('Provider download is larger than the pinned input.')
                    output.write(block)
                    if total >= next_progress:
                        print(f'Provider download: {total * 100 // spec["bytes"]}%', flush=True)
                        next_progress += 20 * 1024 * 1024
            verify(temporary, spec)
            temporary.replace(destination)
            print('Provider APK size and SHA256 verified.', flush=True)
            return destination.resolve()
        except Exception as error:
            last_error = error
            if temporary is not None:
                temporary.unlink(missing_ok=True)
            if attempt < 3:
                print(f'Provider download attempt {attempt} failed: {error}; retrying.', flush=True)
                time.sleep(attempt)
    raise ValueError(
        f'Provider download failed: {last_error}. Download {spec["url"]} manually, '
        f'save it as {destination}, and retry. Do not substitute a different APK.'
    ) from last_error


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--provider', type=Path, help='Verify an existing APK instead of downloading')
    parser.add_argument('--offline', action='store_true')
    args = parser.parse_args()
    try:
        print(resolve_provider(args.provider, offline=args.offline))
    except (ValueError, OSError) as error:
        parser.exit(1, f'{error}\n')


if __name__ == '__main__':
    main()
