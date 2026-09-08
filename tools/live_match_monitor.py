#!/usr/bin/env python3
"""Live read-only monitor for the current CF match.

Samples known camera/transform candidates and D3DREF9 state globals while the
operator plays.  No keys are sent and no memory is written.
"""
from __future__ import annotations
import csv, json, struct, sys, time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import runtime_hotkey_batch as b
import runtime_patch_capture as r

pid = b.find_pid()
if not pid: raise SystemExit("crossfire not found")
hp = b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION | b.PROCESS_VM_READ, False, pid)
mods = {m['name']:m for m in r.modules(pid)}
targets = {
  'crossfire.exe':[0xdb906c,0xdb9094,0xdb90c4,0xdbfe54,0xdbfe94,0xdc13e0,0xdcbb9c,0xdd7aa0],
  'cshell.dll':[0x16a53e0,0x16a5400,0x1e71000,0x1e8e478,0x1e8e498],
}
out = Path('runtime-diffs/match-baseline/live-monitor.csv'); out.parent.mkdir(parents=True,exist_ok=True)
fields=['t','pid']
for mn,rs in targets.items():
  for rv in rs:
    fields += [f'{mn}@{rv:x}+{i}' for i in range(4)]
fields += [f'd3d@{rv:x}' for rv in b.GLOBAL_RVAS]
with out.open('w',newline='',encoding='utf-8') as f:
  w=csv.DictWriter(f,fieldnames=fields); w.writeheader()
  start=time.time(); count=0
  while time.time()-start < 120:
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
        raw=r.read_region(hp,dm['base']+rv,4); row[f'd3d@{rv:x}']=raw.hex() if len(raw)==4 else ''
    w.writerow(row); f.flush(); count+=1
    time.sleep(0.1)
print(f'pid={pid} samples={count} output={out.resolve()}')
b.k32.CloseHandle(hp)
