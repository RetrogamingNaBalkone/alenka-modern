"""Capture the three reference screens from a freshly launched DOS original.

Requires a DOSBox Automation HTTP API on localhost and DOSBOX_API_TOKEN in the
environment. The original must be waiting on its intro, not already in a menu.
Only use a working COPY of original/ALENKA: the DOS program can write state.
"""
import argparse
import json
import os
from pathlib import Path
import time
import urllib.request
from frame_storage import write_frame


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--url', default='http://127.0.0.1:8387')
    parser.add_argument('--output', type=Path, default=Path('diagnostics/validation/reference'))
    args = parser.parse_args()
    headers = {'Authorization': 'Bearer '+os.environ['DOSBOX_API_TOKEN']}
    args.output.mkdir(parents=True, exist_ok=True)

    def request(path, body=None):
        data = None if body is None else json.dumps(body).encode()
        req = urllib.request.Request(args.url+'/api/v1/'+path, data=data,
                                     headers={**headers, 'Content-Type': 'application/json'})
        with urllib.request.urlopen(req, timeout=10) as response:
            return response.read()

    def capture(name):
        for extension in ('raw', 'png'):
            data = request('video/frame?format='+extension)
            if extension == 'raw':
                write_frame(args.output/(name+'.raw.gz'), data)
            else:
                (args.output/(name+'.'+extension)).write_bytes(data)

    def press(name):
        request('input/sequence', {'events': [
            {'t': 0, 'type': 'key', 'key': 'KBD_'+name, 'pressed': True},
            {'t': 50, 'type': 'key', 'key': 'KBD_'+name, 'pressed': False}]})
        time.sleep(0.5)

    capture('intro')
    press('enter')
    capture('help')
    press('space')
    capture('main')
    print('Captured original intro, help and main; run tools/build.ps1 -Reference')


if __name__ == '__main__':
    main()
