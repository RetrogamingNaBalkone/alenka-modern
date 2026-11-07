"""Verify native Linux release contents, ELF architecture and original assets."""
import argparse
from pathlib import Path
import struct
import os
import tarfile

ROOT = Path(__file__).resolve().parents[1]
FILES = {'alenka', 'README.txt', 'LICENSE', 'data/ALLE.LIB', 'data/ALLE.FLD',
         'data/ALLE.TXT', 'data/ALENKA.DAT'}


def verify(package, archive=None, arch=None):
    actual = {p.relative_to(package).as_posix() for p in package.rglob('*') if p.is_file()}
    if actual != FILES or any(p.is_symlink() for p in package.rglob('*')):
        raise ValueError('Unexpected Linux release contents: ' + repr(actual ^ FILES))
    binary = (package / 'alenka').read_bytes()
    if binary[:4] != b'\x7fELF' or binary[5] != 1:
        raise ValueError('Expected a little-endian ELF executable')
    machine = struct.unpack_from('<H', binary, 18)[0]
    detected = {(1, 3): 'i386', (2, 62): 'amd64'}.get((binary[4], machine))
    if detected is None or (arch and detected != arch):
        raise ValueError('Wrong ELF architecture: ' + str(detected))
    if os.name != 'nt' and not (package / 'alenka').stat().st_mode & 0o111:
        raise ValueError('Binary is not executable')
    for name in ('ALLE.LIB', 'ALLE.FLD', 'ALLE.TXT'):
        if (package / 'data' / name).read_bytes() != (ROOT / 'original/ALENKA' / name).read_bytes():
            raise ValueError('Original resource changed: ' + name)
    if (package / 'data/ALENKA.DAT').read_bytes() != (ROOT / 'original/ALENKA/ALLE.EXE').read_bytes()[0x8730:0x9a30]:
        raise ValueError('Extracted data segment changed')
    for name, source in [('README.txt', 'resources/README-linux.txt'), ('LICENSE', 'LICENSE')]:
        if (package / name).read_bytes() != (ROOT / source).read_bytes():
            raise ValueError('User document changed: ' + name)
    if archive:
        with tarfile.open(str(archive), 'r:gz') as tar:
            members = [m for m in tar.getmembers() if not m.isdir()]
            if len(members) != len(FILES) or {m.name for m in members} != {'alenka/' + n for n in FILES}:
                raise ValueError('Unexpected tarball contents')
            for member in members:
                if not member.isfile() or tar.extractfile(member).read() != (package / member.name[7:]).read_bytes():
                    raise ValueError('Tarball differs: ' + member.name)
                if member.name == 'alenka/alenka' and not member.mode & 0o111:
                    raise ValueError('Tarball executable mode lost')
    print('Linux release verified: ' + detected)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('package', type=Path)
    parser.add_argument('--archive', type=Path)
    parser.add_argument('--arch', choices=['amd64', 'i386'])
    args = parser.parse_args()
    verify(args.package, args.archive, args.arch)
