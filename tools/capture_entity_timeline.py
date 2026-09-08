#!/usr/bin/env python3
"""Capture synchronized target-action windows for entity pointer analysis.

The operator presses Enter immediately before each target state. The script
then records 5 seconds of read-only samples for candidate records and D3D9
state globals, preserving phase labels for later correlation.
"""
import csv, struct, sys, time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import runtime_hotkey_batch as b, runtime_patch_capture as r

pid=b.find_pid()
if not pid: raise SystemExit('crossfire not found')
hp=b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION|b.PROCESS_VM_READ,False,pid)
m=next((x for x in r.modules(pid) if x['name']=='cshell.dll'),None)
if not m: raise SystemExit('cshell.dll not loaded')
rv=[0x1e71000,0x1e71ba4,0x1e71bb0,0x1e8e498]
phases=['静止','移动','跳跃','死亡']
out=Path('runtime-diffs/match-baseline/entity-timeline.csv');out.parent.mkdir(parents=True,exist_ok=True)
fields=['phase','t']+[f'{x:x}_{i}' for x in rv for i in range(3)]
with out.open('w',newline='',encoding='utf-8') as f:
 w=csv.DictWriter(f,fieldnames=fields); w.writeheader()
 for phase in phases:
  input(f'让目标进入【{phase}】状态后按回车开始采样（5秒）...')
  end=time.time()+5; n=0
  while time.time()<end:
   row={'phase':phase,'t':time.time()}
   for x in rv:
    raw=r.read_region(hp,m['base']+x,12); vals=struct.unpack('<3f',raw) if len(raw)==12 else [None]*3
    for i,v in enumerate(vals): row[f'{x:x}_{i}']=v
   w.writerow(row); f.flush(); n+=1; time.sleep(.1)
  print(phase,n,'samples',flush=True)
print('output=',out.resolve())
b.k32.CloseHandle(hp)
