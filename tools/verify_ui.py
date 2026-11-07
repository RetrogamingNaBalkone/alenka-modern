"""Capture deterministic profile UI states and compare visually reviewed native frames."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
from inspect_resources import png
from workspace import temporary_work

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = {'main', 'english', 'muted', 'statistics', 'help', 'help-profiles', 'profile-menu', 'rename', 'profiles', 'save-error'}


def verify(executable, data, output=None, record=False):
    (ROOT / 'build').mkdir(exist_ok=True)
    with temporary_work('ui-') as directory:
        subprocess.run([str(executable.resolve()), str(data.resolve()), str(directory)], check=True, cwd=ROOT)
        frames = {p.stem: p.read_bytes() for p in directory.glob('*.raw')}
        assert set(frames) == EXPECTED, 'Missing UI states'
        hashes = {}
        for name, raw in frames.items():
            assert len(raw) == 15 + 640 * 350 * 4 and raw[12] == 32, 'Invalid framebuffer'
            hashes[name] = hashlib.sha256(raw).hexdigest()
            if output:
                output.mkdir(parents=True, exist_ok=True)
                pixels = raw[15:]
                rgb = bytearray(640 * 350 * 3)
                rgb[0::3], rgb[1::3], rgb[2::3] = pixels[2::4], pixels[1::4], pixels[0::4]
                png(output / (name + '.png'), 640, 350, rgb)
        evidence = ROOT / 'docs/evidence/profile-ui.json'
        if record:
            evidence.write_text(json.dumps(dict(sorted(hashes.items())), indent=2) + '\n', encoding='utf-8')
        assert hashes == json.loads(evidence.read_text(encoding='utf-8')), 'Profile UI frame differs from reviewed baseline'
    print(f'{len(EXPECTED)} reviewed profile UI states passed at native 640x350 resolution')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('data', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument('--record', action='store_true', help='Explicitly record newly reviewed UI baselines')
    args = parser.parse_args()
    verify(args.executable, args.data, args.output, args.record)
