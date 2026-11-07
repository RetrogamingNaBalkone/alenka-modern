"""Own a headless DOS reference process, mounting only a disposable original copy."""
from pathlib import Path
from contextlib import ExitStack
import json
import os
import secrets
import shutil
import subprocess
import time
import urllib.request
from workspace import temporary_work
from frame_storage import write_frame

ROOT = Path(__file__).resolve().parents[1]


class Reference:
    def __init__(self, directory='diagnostics/validation/reference-trace', port=8387):
        self.directory = ROOT / directory
        self.directory.mkdir(parents=True, exist_ok=True)
        self._stack = ExitStack()
        self.process = None
        self.token = secrets.token_hex(32)
        self.url = f'http://127.0.0.1:{port}/api/v1/'
        try:
            self.work = self._stack.enter_context(temporary_work('dosbox-'))
            self.game = self.work / 'ALENKA'
            self.game.mkdir()
            for file in (ROOT/'original/ALENKA').iterdir():
                shutil.copy2(file, self.game/file.name)
            config = self.work/'dosbox.conf'
            config.write_text(f'''[sdl]
fullscreen = false
window_resolution = 640x480
output = texture
texture_renderer = software
[cpu]
cycles = fixed 10000
[mixer]
nosound = true
[webserver]
webserver_enabled = true
webserver_port = {port}
webserver_token_file = false
[autoexec]
mount c "{self.game}"
c:
alle.exe
''')
            dosbox = Path(os.environ['LOCALAPPDATA'])/'Programs/DOSBox-Automation/0.85.1/dosbox-automation-0.85.1/dosbox.exe'
            env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy', DOSBOX_API_TOKEN=self.token)
            self.log = self._stack.enter_context((self.directory/'dosbox.log').open('wb'))
            self.process = subprocess.Popen([str(dosbox), '--noprimaryconf', '--nolocalconf', '--conf', str(config)],
                                        env=env, cwd=ROOT, stdout=self.log, stderr=self.log,
                                        creationflags=subprocess.CREATE_NO_WINDOW)
            for _ in range(100):
                if self.process.poll() is not None:
                    raise RuntimeError('DOSBox exited; inspect '+str(self.directory/'dosbox.log'))
                try:
                    info = self.request('video/frame/info')
                    if info.get('video_mode', {}).get('bios_mode_number') == 16:
                        break
                except (OSError, ValueError):
                    pass
                time.sleep(.1)
            else:
                raise TimeoutError('DOS reference startup')
        except BaseException:
            self.close()
            raise

    def request(self, endpoint, body=None, raw=False):
        req = urllib.request.Request(self.url+endpoint,
                data=None if body is None else json.dumps(body).encode(),
                headers={'Authorization': 'Bearer '+self.token, 'Content-Type': 'application/json'})
        with urllib.request.urlopen(req, timeout=3) as response:
            data = response.read()
        return data if raw else json.loads(data)

    def key(self, name, hold=30, settle=.12):
        self.request('input/sequence', {'events': [
            {'t': 0, 'type': 'key', 'key': 'KBD_'+name, 'pressed': True},
            {'t': hold, 'type': 'key', 'key': 'KBD_'+name, 'pressed': False}]})
        time.sleep(settle)

    def capture(self, name):
        for ext in ('raw', 'png'):
            data = self.request('video/frame?format='+ext, raw=True)
            if ext == 'raw':
                write_frame(self.directory/(name+'.raw.gz'), data)
            else:
                (self.directory/(name+'.'+ext)).write_bytes(data)

    def enter(self):
        self.key('enter')
        self.key('space')

    def data_segment(self):
        memory = self.request('memory/0/131072', raw=True)
        marker = memory.find(b'Turbo C++ - Copyright 1990 Borland Intl.')
        if marker < 0 or (marker-4) % 16:
            raise RuntimeError('Cannot locate original data segment')
        return marker-4

    def memory(self, address, size):
        return self.request(f'memory/{address}/{size}', raw=True)

    def write_memory(self,address,data):
        req=urllib.request.Request(self.url+f'memory/{address}',data=data,method='PUT',
                headers={'Authorization':'Bearer '+self.token,'Content-Type':'application/octet-stream'})
        with urllib.request.urlopen(req,timeout=3) as response:
            return response.read()

    def input(self,name,pressed,settle=.06):
        self.request('input/sequence',{'events':[{'t':0,'type':'key','key':'KBD_'+name,'pressed':pressed}]})
        time.sleep(settle)

    def close(self):
        try:
            if self.process is not None and self.process.poll() is None:
                try:
                    self.request('control/shutdown', {})
                    self.process.wait(timeout=5)
                except (OSError, ValueError, subprocess.TimeoutExpired):
                    self.process.terminate()
                    self.process.wait(timeout=5)
        finally:
            self._stack.close()

    def __enter__(self):
        return self

    def __exit__(self, *args):
        self.close()


if __name__ == '__main__':
    with Reference() as ref:
        ref.enter()
        ref.capture('00-main')
        print(json.dumps(ref.request('cpu/state'), indent=2))
        for i, key in enumerate(['l', 'k', 'd', 'k', 'd', 'space', 'enter', 'right', 'up', 'right', 'up', 'esc'], 1):
            ref.key(key)
            ref.capture(f'{i:02}-{key}')
    print('Reference trace captured')
