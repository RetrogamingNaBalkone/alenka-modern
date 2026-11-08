"""Validate the actual AppImage payload, relocation boundaries and ELF baseline."""
import argparse
from pathlib import Path
import re
import struct
import subprocess

from workspace import temporary_work

ROOT = Path(__file__).resolve().parents[1]


def elf_arch(data):
    if len(data) < 20 or data[:4] != b'\x7fELF' or data[5] != 1:
        raise ValueError('Expected a little-endian ELF')
    arch = {(1, 3): 'i386', (2, 62): 'amd64'}.get((data[4], struct.unpack_from('<H', data, 18)[0]))
    if not arch:
        raise ValueError('Unsupported ELF architecture')
    return arch


def verify_appdir(appdir, arch, check_elf=True):
    appdir = appdir.resolve()
    expected = {
        'AppRun': 'resources/AppRun',
        'alenka.desktop': 'resources/alenka.desktop',
        'alenka.png': 'resources/alenka.png',
        'usr/share/doc/alenka-modern/LICENSE': 'LICENSE',
        'usr/share/doc/alenka-modern/README.txt': 'resources/README-linux.txt',
        'usr/share/doc/alenka-modern/AppImage-runtime-LICENSE.txt': 'resources/AppImage-runtime-LICENSE.txt',
        'usr/share/doc/alenka-modern/AppImage-runtime-SOURCE.txt': 'resources/AppImage-runtime-SOURCE.txt',
        'usr/share/metainfo/io.github.RetrogamingNaBalkone.alenka-modern.metainfo.xml': 'resources/alenka.metainfo.xml',
    }
    for target, source in expected.items():
        actual = (appdir / target).read_bytes()
        original = (ROOT / source).read_bytes()
        if target == 'alenka.desktop':
            version = re.search(r'project\(alenka-modern VERSION (\S+)',
                                (ROOT / 'CMakeLists.txt').read_text()).group(1)
            suffix = ('X-AppImage-Version=' + version + '\n').encode('ascii')
            if actual == original + suffix:
                actual = original
        if actual != original:
            raise ValueError('AppDir asset differs: ' + target)
    for name in ('ALLE.LIB', 'ALLE.FLD', 'ALLE.TXT'):
        if (appdir / 'usr/share/alenka-modern' / name).read_bytes() != (ROOT / 'original/ALENKA' / name).read_bytes():
            raise ValueError('Original resource differs: ' + name)
    if (appdir / 'usr/share/alenka-modern/ALENKA.DAT').read_bytes() != (ROOT / 'original/ALENKA/ALLE.EXE').read_bytes()[0x8730:0x9a30]:
        raise ValueError('Extracted DOS data differs')
    libraries = appdir / 'usr/lib'
    if not list(libraries.glob('libSDL2*.so*')) or not list(libraries.glob('libstdc++*.so*')):
        raise ValueError('SDL2 and libstdc++ must be bundled')
    notices = appdir / 'usr/share/doc/alenka-modern/libraries'
    if not (notices / 'SOURCES.tsv').is_file() or not list(notices.glob('*.copyright')):
        raise ValueError('Bundled library notices/sources missing')
    executable = appdir / 'usr/bin/alenka'
    if elf_arch(executable.read_bytes()) != arch:
        raise ValueError('Wrong payload architecture')
    for path in appdir.rglob('*'):
        relative = path.relative_to(appdir)
        resolved = path.resolve()
        if appdir not in resolved.parents and resolved != appdir:
            raise ValueError('AppDir symlink escapes: ' + str(relative))
        if path.name in ('profiles', 'alenka.ini', '.alenka.lock') or path.suffix.lower() == '.exe':
            raise ValueError('Private data or DOS executable packaged: ' + str(relative))
        if not path.is_file():
            continue
        data = path.read_bytes()
        if data[:4] == b'\x7fELF':
            if path.name in {'libc.so.6', 'libm.so.6', 'libpthread.so.0', 'libdl.so.2',
                             'librt.so.1', 'libresolv.so.2', 'libutil.so.1'} or path.name.startswith('ld-linux'):
                raise ValueError('Host C runtime must not be bundled: ' + str(relative))
            if elf_arch(data) != arch:
                raise ValueError('Mixed ELF architectures: ' + str(relative))
            if check_elf:
                symbols = subprocess.check_output(['readelf', '--version-info', str(path)], universal_newlines=True)
                versions = [tuple(map(int, v.split('.'))) for v in re.findall(r'GLIBC_(\d+(?:\.\d+)+)', symbols)]
                if any(v > (2, 27) for v in versions):
                    raise ValueError('GLIBC newer than Ubuntu 18.04: ' + str(relative))
    if check_elf:
        deps = subprocess.check_output(['ldd', str(executable)], universal_newlines=True)
        if 'not found' in deps:
            raise ValueError('Unresolved payload dependencies: ' + deps)
    print('AppDir verified: ' + arch)


def verify_image(image, arch):
    image = image.resolve()
    with image.open('rb') as source:
        header = source.read(20)
    if elf_arch(header) != arch or header[8:11] != b'AI\x02':
        raise ValueError('Wrong AppImage type or architecture')
    with temporary_work('verify-appimage-') as directory:
        subprocess.run([str(image), '--appimage-extract'], cwd=str(directory), check=True, stdout=subprocess.DEVNULL)
        verify_appdir(directory / 'squashfs-root', arch)
    print('AppImage extracted payload verified: ' + str(image))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', nargs='?', type=Path)
    parser.add_argument('--appdir', type=Path)
    parser.add_argument('--arch', choices=['amd64', 'i386'], required=True)
    args = parser.parse_args()
    if args.appdir:
        verify_appdir(args.appdir, args.arch)
    elif args.image:
        verify_image(args.image, args.arch)
    else:
        parser.error('image or --appdir is required')
