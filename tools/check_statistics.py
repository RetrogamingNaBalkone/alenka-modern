"""Differential component check: identical counter fixtures in DOS and native UI.

Counters are seeded explicitly in both implementations. This isolates the graph
formula from DOS emulation speed, input latency and framebuffer capture timing.
"""
from pathlib import Path
import argparse
import json
import struct
import subprocess
from dos_reference import Reference,ROOT
from workspace import diagnostic_directory
from frame_storage import read_frame, compress_frame

PROFILES={
    'mixed':{139:(1600,8,4),142:(180,3,8),130:(10000,100,0)},
    'fractions':{139:(12,3,1),142:(1,1,3),130:(8,2,3)},
    'errors':{139:(0,0,17),142:(0,0,6),130:(0,0,1)},
    'long':{139:(500000,100,4),142:(88888,24,3),130:(12,12,0)},
}


def check(name,counts,directory):
    directory.mkdir(parents=True,exist_ok=True)
    with Reference(directory) as ref:
        ref.enter()
        base=ref.data_segment()
        for char,values in counts.items():
            ref.write_memory(base+0x3f0+char*8,struct.pack('<IHH',*values))
        ref.key('esc')
        ref.capture('original')
    script=directory/'native.txt'
    script.write_text('\n'.join([f'counter {c} {t} {n} {e}' for c,(t,n,e) in counts.items()]+
            ['key 1 1 100','key 1 0 130','capture '+(directory/'native.raw').relative_to(ROOT).as_posix()])+'\n')
    subprocess.run([str(ROOT/'build/bin/trainer-trace.exe'),str(ROOT/'original/ALENKA'),str(script)],check=True)
    a=read_frame(directory/'original.raw.gz'); b=read_frame(compress_frame(directory/'native.raw'))
    bad=[i for i in range(224000) if a[15+i*4:18+i*4]!=b[15+i*4:18+i*4]]
    print(f'{name}: {len(bad)} differing pixels')
    if bad:
        from collections import Counter
        print(Counter((i%640//8,i//640//8) for i in bad))
    return {'profile':name,'differing_pixels':len(bad),'passed':not bad}


def main(keep=False):
    failures=0
    out=ROOT/'diagnostics/validation'
    out.mkdir(parents=True,exist_ok=True)
    for name,counts in PROFILES.items():
        with diagnostic_directory('statistics-'+name,keep) as directory:
            report=check(name,counts,directory)
            (out/('statistics-'+name+'.json')).write_text(json.dumps(report,indent=2)+'\n')
            failures+=not report['passed']
    return failures


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--keep-artifacts',action='store_true',help='Keep compressed frames and replay scripts for diagnosis')
    raise SystemExit(bool(main(parser.parse_args().keep_artifacts)))
