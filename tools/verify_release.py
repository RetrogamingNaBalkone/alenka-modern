"""Check the user release against its exact file list and original asset bytes."""
import argparse
from pathlib import Path
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]
FILES = {
    'alenka.exe', 'SDL2.dll', 'README.txt', 'licenses/SDL2.txt',
    'data/ALLE.LIB', 'data/ALLE.FLD', 'data/ALLE.TXT', 'data/ALENKA.DAT',
}


def verify(package, archive):
    actual = {p.relative_to(package).as_posix() for p in package.rglob('*') if p.is_file()}
    if actual != FILES:
        raise ValueError(f'Unexpected release contents: extra={actual-FILES}, missing={FILES-actual}')
    with ZipFile(archive) as zipped:
        names = [entry.filename for entry in zipped.infolist() if not entry.is_dir()]
        if set(names) != {'alenka/' + name for name in FILES} or len(names) != len(FILES):
            raise ValueError('ZIP contents do not match the release file list')
        for name in FILES:
            if zipped.read('alenka/' + name) != (package / name).read_bytes():
                raise ValueError('ZIP differs from package: ' + name)
    for name in ('ALLE.LIB', 'ALLE.FLD', 'ALLE.TXT'):
        if (package / 'data' / name).read_bytes() != (ROOT / 'original/ALENKA' / name).read_bytes():
            raise ValueError('Packaged resource differs: ' + name)
    data = (ROOT / 'original/ALENKA/ALLE.EXE').read_bytes()[0x8730:0x9a30]
    if (package / 'data/ALENKA.DAT').read_bytes() != data:
        raise ValueError('Extracted data differs from the original')
    if (package / 'README.txt').read_bytes() != (ROOT / 'resources/README.txt').read_bytes():
        raise ValueError('User instructions differ')
    print('Release verified: 8 runtime/user files; no DOS executable or developer documents')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', nargs='?', type=Path, default=ROOT/'artifacts/releases/alenka-windows')
    parser.add_argument('archive', nargs='?', type=Path, default=ROOT/'artifacts/releases/alenka-windows.zip')
    args = parser.parse_args()
    verify(args.package, args.archive)
