"""Record bundled Ubuntu libraries, their copyright notices and source versions."""
import argparse
from pathlib import Path
import shutil
import subprocess


def collect(appdir):
    document = appdir / 'usr/share/doc/alenka-modern/libraries'
    document.mkdir(parents=True, exist_ok=True)
    cache = subprocess.check_output(['ldconfig', '-p'], universal_newlines=True)
    libraries = {}
    for line in cache.splitlines():
        if ' => ' in line:
            name = line.split()[0]
            libraries.setdefault(name, Path(line.split(' => ')[1]))
    packages = set()
    for path in (appdir / 'usr/lib').rglob('*'):
        if not path.is_file() or path.read_bytes()[:4] != b'\x7fELF':
            continue
        original = libraries.get(path.name)
        # Private libraries such as pulseaudio/libpulsecommon are not in ldconfig.
        owners = subprocess.check_output(['dpkg-query', '-S',
            str(original) if original else '*/' + path.name], universal_newlines=True)
        matches = set()
        header = path.read_bytes()[:20]
        for line in owners.splitlines():
            package, filename = line.split(': ', 1)
            candidate = Path(filename)
            if candidate.is_file():
                source = candidate.read_bytes()[:20]
                if source[:4] == b'\x7fELF' and source[4:6] == header[4:6] and source[18:20] == header[18:20]:
                    matches.add(package)
        if len(matches) != 1:
            raise ValueError('Cannot identify unique bundled library owner: ' + path.name)
        packages.update(matches)
    records = []
    for package in sorted(packages):
        metadata = subprocess.check_output(['dpkg-query', '-W',
            '-f=${binary:Package}\t${Version}\t${source:Package}\t${source:Version}', package], universal_newlines=True)
        binary, version, source, source_version = metadata.split('\t')
        notice = Path('/usr/share/doc') / binary.split(':')[0] / 'copyright'
        if not notice.is_file():
            raise ValueError('Missing copyright notice: ' + package)
        shutil.copyfile(str(notice), str(document / (binary.replace(':', '_') + '.copyright')))
        records.append(metadata + '\thttps://launchpad.net/ubuntu/+source/{}/{}'.format(source, source_version))
    (document / 'SOURCES.tsv').write_text(
        'binary\tversion\tsource\tsource_version\tsource_url\n' + '\n'.join(records) + '\n', encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('appdir', type=Path)
    collect(parser.parse_args().appdir)
