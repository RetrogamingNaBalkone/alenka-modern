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
