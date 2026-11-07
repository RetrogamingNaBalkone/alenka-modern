"""Export runtime assets without distributing the DOS application."""
import argparse
import hashlib
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parents[1]


def prepare(destination):
    original = ROOT / 'original'
    for line in (original / 'SHA256SUMS').read_text().splitlines():
        expected, name = line.split('  ')
        if hashlib.sha256((original / name).read_bytes()).hexdigest() != expected:
            raise ValueError('Original resource changed: ' + name)
    destination.mkdir(parents=True, exist_ok=True)
    for name in ('ALLE.LIB', 'ALLE.FLD', 'ALLE.TXT'):
        shutil.copyfile(original / 'ALENKA' / name, destination / name)
    exe = (original / 'ALENKA/ALLE.EXE').read_bytes()
    # Only the data segment used by the port: tables, strings and constants.
    # No MZ header or executable code is included.
    (destination / 'ALENKA.DAT').write_bytes(exe[0x8730:0x9a30])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('destination', type=Path)
    prepare(parser.parse_args().destination)
