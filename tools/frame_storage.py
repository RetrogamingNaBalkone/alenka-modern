"""Lossless storage for DOS framebuffers; raw files exist only during C++ checks."""
import argparse
import gzip
from pathlib import Path
import subprocess
from workspace import temporary_work


def read_frame(path):
    path = Path(path)
    data = path.read_bytes()
    return gzip.decompress(data) if path.suffix == '.gz' else data


def write_frame(path, data):
    path = Path(path)
    path.write_bytes(gzip.compress(data, compresslevel=9, mtime=0))


def compress_frame(path):
    path = Path(path)
    target = path.with_suffix(path.suffix + '.gz')
    data = path.read_bytes()
    write_frame(target, data)
    if read_frame(target) != data:
        raise ValueError('Compressed framebuffer differs: ' + str(path))
    path.unlink()
    return target


def run_core(executable, data, reference=None):
    if reference is None:
        return subprocess.run([str(executable), str(data)]).returncode
    with temporary_work('reference-') as directory:
        frames = []
        for name in ('intro', 'help', 'main'):
            path = directory / (name + '.raw')
            path.write_bytes(read_frame(Path(reference) / (name + '.raw.gz')))
            frames.append(str(path))
        return subprocess.run([str(executable), str(data), *frames]).returncode


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('executable', type=Path)
    parser.add_argument('data', type=Path)
    parser.add_argument('--reference', type=Path)
    args = parser.parse_args()
    raise SystemExit(run_core(args.executable, args.data, args.reference))
