#!/usr/bin/env python3
"""Autonomous match sampler; no console input required.

Continuously records candidate transform triples and D3DREF9 state while the
operator enters/plays a match.  The capture starts immediately and runs for
three minutes, so key presses in the game window are irrelevant to the
sampler's control flow.
"""
from __future__ import annotations
import argparse, csv, shutil, struct, sys, time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import runtime_hotkey_batch as b, runtime_patch_capture as r

ap=argparse.ArgumentParser(description='Read-only CF runtime timeline sampler')
ap.add_argument('--duration',type=float,default=180.0,help='采样秒数，默认180')
ap.add_argument('--interval',type=float,default=0.1,help='采样间隔秒数，默认0.1')
ap.add_argument('--pid',type=int,default=0,help='可选，固定目标 PID')
ns=ap.parse_args()
pid=ns.pid or b.find_pid()
if not pid: raise SystemExit('crossfire not found')
hp=b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION|b.PROCESS_VM_READ,False,pid)
mods={m['name']:m for m in r.modules(pid)}
targets={
 'crossfire.exe':[0xdb90c4,0xdbfe54,0xdbfe94,0xdc13e0,0xdcbb9c,0xdd7aa0],
 'cshell.dll':[0x16a53e0,0x16a5400,0x1e71000,0x1e71ba4,0x1e71bb0,0x1e8e498],
}
out_dir=Path('runtime-diffs/match-baseline');out_dir.mkdir(parents=True,exist_ok=True)
run_id=f'pid{pid}-{int(time.time())}'
out=out_dir/f'auto-entity-timeline-{run_id}.csv'
latest=out_dir/'auto-entity-timeline.csv'
fields=['t','pid']+[f'{mn}@{rv:x}+{i}' for mn,rs in targets.items() for rv in rs for i in range(4)]
fields += [f'd3d@{rv:x}' for rv in b.GLOBAL_RVAS]
start=time.time(); n=0
with out.open('w',newline='',encoding='utf-8') as f:
 w=csv.DictWriter(f,fieldnames=fields);w.writeheader()
 while time.time()-start<max(1.0,ns.duration):
  row={'t':time.time(),'pid':pid}
  for mn,rs in targets.items():
   m=mods.get(mn)
   for rv in rs:
    raw=r.read_region(hp,m['base']+rv,16) if m else b''
    vals=struct.unpack('<4f',raw) if len(raw)==16 else [None]*4
    for i,v in enumerate(vals): row[f'{mn}@{rv:x}+{i}']=v
  dm=mods.get('d3dref9.dll')
  if dm:
   for rv in b.GLOBAL_RVAS:
    raw=r.read_region(hp,dm['base']+rv,4);row[f'd3d@{rv:x}']=raw.hex() if len(raw)==4 else ''
  w.writerow(row);f.flush();n+=1;time.sleep(max(0.02,ns.interval))
shutil.copyfile(out, latest)
print(f'pid={pid} samples={n} archive={out.resolve()} latest={latest.resolve()}')
b.k32.CloseHandle(hp)
