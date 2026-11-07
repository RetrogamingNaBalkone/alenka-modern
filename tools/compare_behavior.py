"""Run identical original scan-code scenarios in DOS and the native trainer."""
from pathlib import Path
from collections import Counter
import argparse
import json
import struct
import subprocess
from dos_reference import Reference, ROOT
from workspace import diagnostic_directory
from frame_storage import read_frame, compress_frame

SCANS = {'esc':1,'enter':28,'leftshift':42,'rightshift':54,'space':57,'capslock':58,
         'f1':59,'numlock':69,'scrolllock':70,'left':92,'right':99,'up':95,'down':96}
for code, char in enumerate('1234567890-=',2): SCANS[char]=code
for code, char in enumerate('qwertyuiop[]',16): SCANS[char]=code
for code, char in enumerate("asdfghjkl;'",30): SCANS[char]=code
for code, char in enumerate('zxcvbnm,./',44): SCANS[char]=code


def scenarios():
    menus=['right','up','space','down','right','up','down','right','up','up','up','down',
           'right','space','right','space','a','right','space','a','right','space','a',
           'right','space','a','right','space','a','f1','a','capslock','capslock','enter',
           'right','down','up','right','up','up','down','f1','a','esc','enter']
    data=(ROOT/'original/ALENKA/ALLE.EXE').read_bytes()[0x8730:]
    letters=data[0x477:0x477+59]
    first=(ROOT/'original/ALENKA/ALLE.TXT').read_bytes().split(b'\r\n')[1]
    scan_names={scan:key for key,scan in SCANS.items()}
    lesson=[scan_names[letters.index(c)] for c in first]
    return {'menus':menus, 'lesson':lesson+['d','enter','esc'],
            'errors':['right','up','enter','d','j','k','k','space','enter','esc'],
            'modifiers':['capslock','leftshift','rightshift','leftshift','rightshift','capslock','f1','a'],
            'held':['leftshift+','rightshift+','rightshift-','leftshift-','leftshift+','rightshift+',
                    'rightshift-','leftshift-','capslock+','capslock-','enter+','enter-',
                    'k+','k-','enter+','enter-','f1+','f1-','a+','a-']}


def _compare(directory,name,keys):
    directory.mkdir(parents=True,exist_ok=True)
    native=directory/'native'
    native.mkdir(exist_ok=True)
    commands=[]
    manifest=[]
    with Reference(directory) as ref:
        ref.enter()
        base=ref.data_segment()
        epoch=0  # The first original reaction interval starts at scheduler startup.
        def capture(index,key):
            label=f'{index:03}-{key}'
            ref.capture(label)
            tick=(int.from_bytes(ref.memory(base+0xed6,2),'little')-epoch)&0xffff
            commands.append(f'advance {(tick*16384000+1193181)//1193182}')
            commands.append('capture '+(native/(label+'.raw')).relative_to(ROOT).as_posix())
            manifest.append(label)
        capture(0,'main')
        for i,key in enumerate(keys,1):
            previous=ref.memory(base+0x123e,2)
            tick=int.from_bytes(ref.memory(base+0xed6,2),'little')
            name_key=key.rstrip('+-')
            if key.endswith(('+','-')): ref.input(name_key,key.endswith('+'))
            else: ref.key(key)
            after=ref.memory(base+0x123e,2)
            if after!=previous:
                tick=int.from_bytes(after,'little')
            ticks=(tick-epoch)&0xffff
            milliseconds=(ticks*16384000+1193181)//1193182
            if key.endswith(('+','-')):
                commands.append(f'key {SCANS[name_key]} {int(key.endswith("+"))} {milliseconds}')
            else:
                commands.extend([f'key {SCANS[key]} 1 {milliseconds}', f'key {SCANS[key]} 0 {milliseconds+30}'])
            capture(i,key)
        counters=ref.memory(base+0x4f0,224*8)
        expected_counts={str(c+32):list(struct.unpack_from('<IHH',counters,c*8)) for c in range(224)
                         if any(struct.unpack_from('<IHH',counters,c*8))}
        (directory/'counts-original.json').write_text(json.dumps(expected_counts,indent=2))
    path=directory/'native.txt'
    path.write_text('\n'.join(commands)+'\nstate\n')
    result=subprocess.run([str(ROOT/'build/bin/trainer-trace.exe'),str(ROOT/'original/ALENKA'),str(path)],
                          check=True,cwd=ROOT,capture_output=True,text=True)
    native_state=json.loads(result.stdout)
    (directory/'state-native.json').write_text(json.dumps(native_state,indent=2))
    count_mismatch={k:v[1:] for k,v in native_state['keys'].items()}!={k:v[1:] for k,v in expected_counts.items()}
    reaction_time_diff=native_state['keys']!=expected_counts
    if count_mismatch: print('Correct/error counter mismatch:',expected_counts,native_state['keys'])
    failures=[]
    data=(ROOT/'original/ALENKA/ALLE.EXE').read_bytes()[0x8730:]
    columns={data[0x2bc+i*2] for i in range(44)}
    for label in manifest:
        actual=read_frame(compress_frame(native/(label+'.raw')))
        expected=read_frame(directory/(label+'.raw.gz'))
        bad=[i for i in range(224000) if actual[15+i*4:18+i*4]!=expected[15+i*4:18+i*4]]
        if bad:
            tiles=Counter((i%640//8,i//640//8) for i in bad)
            def timing_pixel(pixel):
                x,y=pixel%640//8,pixel//640//8
                if name=='lesson' and y in (11,13) and 5<=x<75: return True
                return reaction_time_diff and label.endswith('-esc') and x in columns and 18<=y<=37
            failures.append({'frame':label,'pixels':len(bad),'timing_only':all(map(timing_pixel,bad)),
                             'tiles':[[*cell,count] for cell,count in tiles.most_common()]})
    (directory/'differences.json').write_text(json.dumps(failures,indent=2))
    timing=sum(f['timing_only'] for f in failures)
    print(f'{name}: {len(manifest)-len(failures)}/{len(manifest)} exact frames; {timing} live clock/capture differences')
    for failure in failures:
        print(f"  {failure['frame']}: {failure['pixels']} pixels; {failure['tiles'][:6]}")
    return {'scenario':name, 'frames':len(manifest), 'exact_frames':len(manifest)-len(failures),
            'timing_differences':timing, 'counter_mismatch':count_mismatch, 'differences':failures,
            'passed':not count_mismatch and all(f['timing_only'] for f in failures)}


def compare(name,keys,keep=False):
    report_path=ROOT/'diagnostics/validation'/('behavior-'+name+'.json')
    report_path.parent.mkdir(parents=True,exist_ok=True)
    with diagnostic_directory('behavior-'+name,keep) as directory:
        report=_compare(directory,name,keys)
        report_path.write_text(json.dumps(report,indent=2)+'\n')
        return report['passed']


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('scenario',nargs='?',choices=list(scenarios()))
    parser.add_argument('--keep-artifacts',action='store_true',help='Keep compressed frames and replay scripts for diagnosis')
    args=parser.parse_args()
    results=[compare(name,keys,args.keep_artifacts) for name,keys in scenarios().items() if args.scenario in (None,name)]
    raise SystemExit(0 if all(results) else 1)
