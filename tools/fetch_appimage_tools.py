"""Fetch immutable upstream release assets, verifying every cached/downloaded byte."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import urllib.request

ROOT = Path(__file__).resolve().parents[1]


def fetch(arch, destination):
    pins = json.loads((ROOT / 'resources/appimage-tools.json').read_text())[arch]
    destination.mkdir(parents=True, exist_ok=True)
    for name, pin in pins.items():
        path = destination / name
        if path.exists() and hashlib.sha256(path.read_bytes()).hexdigest() == pin['sha256']:
            continue
        request = urllib.request.Request(
            'https://api.github.com/repos/{}/releases/assets/{}'.format(pin['project'], pin['asset']),
            headers={'Accept': 'application/octet-stream', 'User-Agent': 'alenka-modern-build'})
        temporary = path.with_suffix('.download')
        try:
            with urllib.request.urlopen(request, timeout=120) as response, temporary.open('wb') as output:
                shutil.copyfileobj(response, output)
            if hashlib.sha256(temporary.read_bytes()).hexdigest() != pin['sha256']:
                raise ValueError('SHA256 mismatch: ' + name)
            temporary.chmod(0o755)
            temporary.replace(path)
        finally:
            if temporary.exists():
                temporary.unlink()
        print('Verified AppImage tool: ' + name)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('arch', choices=['amd64', 'i386'])
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    fetch(args.arch, args.destination)
