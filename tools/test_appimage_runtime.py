"""Exercise real packaged SDL input and profile persistence under Xvfb (no FUSE)."""
import argparse
import configparser
import os
from pathlib import Path
import shutil
import subprocess
import time

from workspace import temporary_work


def wait_for(predicate, description):
    deadline = time.monotonic() + 20
    while time.monotonic() < deadline:
        value = predicate()
        if value:
            return value
        time.sleep(0.1)
    raise RuntimeError('Timeout: ' + description)


def profile(root):
    files = list((root / 'profiles').glob('*.ini'))
    if len(files) != 1:
        return None
    parser = configparser.ConfigParser()
    parser.read(str(files[0]), encoding='utf-8')
    return files[0], parser


def session(command, environment, cwd, data, toggle):
    with (data.parent / 'session.log').open('w') as log:
        process = subprocess.Popen(command + ['--screen', 'main'], env=environment, cwd=str(cwd), stdout=log, stderr=log)
        try:
            def window():
                if process.poll() is not None:
                    raise RuntimeError('Application exited during startup; see ' + str(data.parent / 'session.log'))
                found = subprocess.run(['xdotool', 'search', '--onlyvisible', '--class', '^alenka$'],
                                       stdout=subprocess.PIPE, universal_newlines=True)
                return found.stdout.splitlines()
            handle = wait_for(window, 'SDL window')[0]
            subprocess.run(['xdotool', 'windowactivate', '--sync', handle], check=True)
            wait_for(lambda: profile(data), 'profile creation')
            if toggle:
                # Profile -> Language -> Sound. Save immediately through the real event loop.
                subprocess.run(['xdotool', 'key', '--clearmodifiers',
                                'Right', 'Right', 'space'], check=True)
                wait_for(lambda: profile(data)[1].get('settings', 'sound') == '0', 'sound setting saved')
            subprocess.run(['xdotool', 'key', '--clearmodifiers', 'alt+F4'], check=True)
            if process.wait(timeout=20) != 0:
                raise RuntimeError('Application close failed')
        finally:
            if process.poll() is None:
                process.kill()
                process.wait()


def verify(image, mounted=False, extract_and_run=False):
    with temporary_work('appimage-runtime-') as work:
        package = work / 'path with spaces'
        package.mkdir()
        relocated = package / image.name
        shutil.copyfile(str(image), str(relocated))
        relocated.chmod(0o755)
        environment = dict(os.environ, SDL_AUDIODRIVER='dummy', SDL_RENDER_DRIVER='software')
        environment['HOME'] = str(work / 'home')
        environment['XDG_DATA_HOME'] = str(work / 'user data')
        data = Path(environment['XDG_DATA_HOME']) / 'alenka-modern'
        data.mkdir(parents=True)
        if mounted or extract_and_run:
            command = [str(relocated)] + (['--appimage-extract-and-run'] if extract_and_run else [])
        else:
            extracted = work / 'extracted'
            extracted.mkdir()
            subprocess.run([str(relocated), '--appimage-extract'], cwd=str(extracted), check=True, stdout=subprocess.DEVNULL)
            command = [str(extracted / 'squashfs-root/AppRun')]
        # /usr is deliberately not writable by the ordinary user running the check.
        subprocess.run(command + ['--smoke-test'], cwd='/usr', env=environment, check=True, timeout=40)
        session(command, environment, Path('/usr'), data, True)
        before = profile(data)[0].read_bytes()
        identity = (data / 'alenka.ini').read_bytes()
        session(command, environment, Path('/usr'), data, False)
        if profile(data)[0].read_bytes() != before or (data / 'alenka.ini').read_bytes() != identity:
            raise ValueError('Profile/settings changed on restart')
        if list(package.glob('*.ini')) or (package / 'profiles').exists():
            raise ValueError('User data written next to the AppImage')
        print('AppImage relocation, read-only CWD, SDL input, save and restart passed')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('image', type=Path)
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--mounted', action='store_true', help='Also requires working /dev/fuse')
    mode.add_argument('--extract-and-run', action='store_true', help='Use the runtime fallback without FUSE')
    args = parser.parse_args()
    # A fresh X server per check avoids other application windows capturing keys.
    if not os.environ.get('ALENKA_TEST_XVFB'):
        env = dict(os.environ, ALENKA_TEST_XVFB='1', LC_ALL='C.UTF-8')
        subprocess.run(['xvfb-run', '-a', 'python3', str(Path(__file__).resolve()), str(args.image.resolve())]
                       + (['--mounted'] if args.mounted else [])
                       + (['--extract-and-run'] if args.extract_and_run else []), env=env, check=True)
    else:
        manager = subprocess.Popen(['openbox'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        try:
            wait_for(lambda: 'window id #' in subprocess.check_output(
                     ['xprop', '-root', '_NET_SUPPORTING_WM_CHECK'], universal_newlines=True),
                     'window manager')
            verify(args.image.resolve(), args.mounted, args.extract_and_run)
        finally:
            manager.terminate()
            manager.wait(timeout=10)
