"""Disposable, repository-local working directories for analysis and validation."""
from contextlib import contextmanager
from pathlib import Path
import shutil
from uuid import uuid4

ROOT = Path(__file__).resolve().parents[1]


@contextmanager
def temporary_work(prefix):
    parent = (ROOT / 'build/work').resolve()
    if not parent.is_relative_to(ROOT):
        raise ValueError('Working directory must stay inside the repository')
    parent.mkdir(parents=True, exist_ok=True)
    path = (parent / (prefix + uuid4().hex)).resolve()
    if not path.is_relative_to(parent):
        raise ValueError('Temporary directory escaped the build workspace')
    # Inherit the workspace ACL on Windows, including sandbox access entries.
    path.mkdir()
    try:
        yield path
    finally:
        shutil.rmtree(path)


@contextmanager
def diagnostic_directory(name, keep=False):
    if keep:
        path = ROOT / 'diagnostics/validation' / name
        path.mkdir(parents=True, exist_ok=True)
        yield path
    else:
        with temporary_work(name + '-') as path:
            yield path
