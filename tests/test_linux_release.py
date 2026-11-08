"""Guard release rejection rules without needing a Linux runtime on Windows."""
from pathlib import Path
import importlib.util
import io
import struct
import tarfile
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('verify_linux_release', ROOT / 'tools/verify_linux_release.py')
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class LinuxReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.package = self.root / 'package'
        (self.package / 'data').mkdir(parents=True)
        elf = bytearray(64)
        elf[:6] = b'\x7fELF\x02\x01'
        struct.pack_into('<H', elf, 18, 62)
        (self.package / 'alenka').write_bytes(elf)
        (self.package / 'alenka').chmod(0o755)
        for name in ('ALLE.LIB', 'ALLE.FLD', 'ALLE.TXT'):
            (self.package / 'data' / name).write_bytes((ROOT / 'original/ALENKA' / name).read_bytes())
        (self.package / 'data/ALENKA.DAT').write_bytes((ROOT / 'original/ALENKA/ALLE.EXE').read_bytes()[0x8730:0x9a30])
        (self.package / 'README.txt').write_bytes((ROOT / 'resources/README-linux.txt').read_bytes())
        (self.package / 'LICENSE').write_bytes((ROOT / 'LICENSE').read_bytes())

    def archive(self, extra=None):
        path = self.root / 'release.tar.gz'
        with tarfile.open(str(path), 'w:gz') as tar:
            for name in sorted(release.FILES):
                data = (self.package / name).read_bytes()
                entry = tarfile.TarInfo('alenka/' + name)
                entry.size = len(data)
                entry.mode = 0o755 if name == 'alenka' else 0o644
                tar.addfile(entry, io.BytesIO(data))
            if extra:
                tar.addfile(tarfile.TarInfo(extra), io.BytesIO(b''))
        return path

    def test_runtime_bytes_and_architecture(self):
        release.verify(self.package, self.archive(), 'amd64')
        with self.assertRaisesRegex(ValueError, 'architecture'):
            release.verify(self.package, arch='i386')
        (self.package / 'data/ALLE.TXT').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'Original resource changed'):
            release.verify(self.package)

    def test_private_data_and_extra_archive_members_rejected(self):
        with self.assertRaisesRegex(ValueError, 'tarball contents'):
            release.verify(self.package, self.archive('alenka/profiles/private.ini'))
        (self.package / 'alenka.ini').write_text('private')
        with self.assertRaisesRegex(ValueError, 'contents'):
            release.verify(self.package)

    def test_archive_must_match_staged_binary(self):
        archive = self.archive()
        elf = bytearray((self.package / 'alenka').read_bytes())
        elf[-1] = 1
        (self.package / 'alenka').write_bytes(elf)
        with self.assertRaisesRegex(ValueError, 'Tarball differs'):
            release.verify(self.package, archive)

    def stage_deb(self):
        root = self.root / 'deb'
        for name, source in release.deb_files(self.package).items():
            target = root / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(source.read_bytes())
            if name in ('usr/bin/alenka', 'usr/lib/alenka-modern/alenka'):
                target.chmod(0o755)
        return root

    def test_deb_uses_shared_resources_and_matches_portable(self):
        root = self.stage_deb()
        release.verify(self.package, self.archive(), 'amd64', root)
        (root / 'usr/share/alenka-modern/ALLE.TXT').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'Deb differs'):
            release.verify(self.package, deb_root=root)

    def test_deb_rejects_old_resource_location(self):
        root = self.stage_deb()
        (root / 'usr/share/alenka-modern').rename(root / 'usr/lib/alenka-modern/data')
        with self.assertRaisesRegex(ValueError, 'deb contents'):
            release.verify(self.package, deb_root=root)

    def test_i386_binary_is_checked_in_both_packages(self):
        elf = bytearray((self.package / 'alenka').read_bytes())
        elf[4] = 1
        struct.pack_into('<H', elf, 18, 3)
        (self.package / 'alenka').write_bytes(elf)
        root = self.stage_deb()
        release.verify(self.package, self.archive(), 'i386', root)
        with self.assertRaisesRegex(ValueError, 'architecture'):
            release.verify(self.package, arch='amd64', deb_root=root)
