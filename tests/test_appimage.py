"""Reject broken or host-dependent AppDirs before release packaging."""
import os
from pathlib import Path
import re
import struct
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import verify_appimage as appimage


class AppImageTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / 'AppDir'
        assets = {
            'AppRun': 'resources/AppRun',
            'alenka.desktop': 'resources/alenka.desktop',
            'alenka.png': 'resources/alenka.png',
            'usr/share/doc/alenka-modern/LICENSE': 'LICENSE',
            'usr/share/doc/alenka-modern/README.txt': 'resources/README-linux.txt',
            'usr/share/doc/alenka-modern/AppImage-runtime-LICENSE.txt': 'resources/AppImage-runtime-LICENSE.txt',
            'usr/share/doc/alenka-modern/AppImage-runtime-SOURCE.txt': 'resources/AppImage-runtime-SOURCE.txt',
            'usr/share/metainfo/io.github.RetrogamingNaBalkone.alenka-modern.metainfo.xml': 'resources/alenka.metainfo.xml',
        }
        for target, source in assets.items():
            self.write(target, (ROOT / source).read_bytes())
        for name in ('ALLE.LIB', 'ALLE.FLD', 'ALLE.TXT'):
            self.write('usr/share/alenka-modern/' + name, (ROOT / 'original/ALENKA' / name).read_bytes())
        self.write('usr/share/alenka-modern/ALENKA.DAT',
                   (ROOT / 'original/ALENKA/ALLE.EXE').read_bytes()[0x8730:0x9a30])
        self.elf = bytearray(64)
        self.elf[:6] = b'\x7fELF\x02\x01'
        struct.pack_into('<H', self.elf, 18, 62)
        for name in ('usr/bin/alenka', 'usr/lib/libSDL2-2.0.so.0', 'usr/lib/libstdc++.so.6'):
            self.write(name, self.elf)
        self.write('usr/share/doc/alenka-modern/libraries/SOURCES.tsv', b'sources\n')
        self.write('usr/share/doc/alenka-modern/libraries/test.copyright', b'notice\n')

    def write(self, name, data):
        path = self.root / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def verify(self):
        appimage.verify_appdir(self.root, 'amd64', check_elf=False)

    def test_assets_and_appimagetool_version(self):
        self.verify()
        desktop = self.root / 'alenka.desktop'
        version = re.search(r'project\(alenka-modern VERSION (\S+)',
                            (ROOT / 'CMakeLists.txt').read_text()).group(1)
        desktop.write_bytes(desktop.read_bytes() + ('X-AppImage-Version=' + version + '\n').encode('ascii'))
        self.verify()
        desktop.write_bytes(desktop.read_bytes().replace(b'Exec=alenka', b'Exec=other'))
        with self.assertRaisesRegex(ValueError, 'asset differs'):
            self.verify()

    def test_modified_resources_rejected(self):
        self.write('usr/share/alenka-modern/ALLE.TXT', b'changed')
        with self.assertRaisesRegex(ValueError, 'Original resource differs'):
            self.verify()

    def test_mixed_architecture_rejected(self):
        self.elf[4] = 1
        struct.pack_into('<H', self.elf, 18, 3)
        self.write('usr/lib/libSDL2-2.0.so.0', self.elf)
        with self.assertRaisesRegex(ValueError, 'Mixed ELF'):
            self.verify()

    def test_host_runtime_rejected(self):
        self.write('usr/lib/libc.so.6', self.elf)
        with self.assertRaisesRegex(ValueError, 'Host C runtime'):
            self.verify()

    def test_private_profile_rejected(self):
        self.write('usr/bin/profiles/private.ini', b'private')
        with self.assertRaisesRegex(ValueError, 'Private data'):
            self.verify()

    @unittest.skipIf(os.name == 'nt', 'Symlink creation needs Windows privileges')
    def test_external_symlink_rejected(self):
        (self.root / 'outside').symlink_to(self.root.parent)
        with self.assertRaisesRegex(ValueError, 'symlink escapes'):
            self.verify()
