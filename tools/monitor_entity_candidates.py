#!/usr/bin/env python3
"""Sample suspected cshell world-position records and nearby fields."""
import csv, struct, sys, time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import runtime_hotkey_batch as b, runtime_patch_capture as r
pid=b.find_pid()
if not pid: raise SystemExit('crossfire not found')
hp=b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION|b.PROCESS_VM_READ,False,pid)
m=next((x for x in r.modules(pid) if x['name']=='cshell.dll'),None)
if not m: raise SystemExit('cshell.dll not loaded')
rv=[0x1e71000,0x1e715e4,0x1e71ba4,0x1e71bb0,0x1e8e498]
out=Path('runtime-diffs/match-baseline/entity-candidates.csv')
out.parent.mkdir(parents=True,exist_ok=True)
fields=['t']+[f'{x:x}_{i}' for x in rv for i in range(3)]
with out.open('w',newline='') as f:
 w=csv.DictWriter(f,fieldnames=fields); w.writeheader(); start=time.time(); n=0
 while time.time()-start<60:
  row={'t':time.time()}
  for x in rv:
   raw=r.read_region(hp,m['base']+x,12); vals=struct.unpack('<3f',raw) if len(raw)==12 else [None]*3
   for i,v in enumerate(vals): row[f'{x:x}_{i}']=v
  w.writerow(row); f.flush(); n+=1; time.sleep(.1)
print(f'pid={pid} samples={n} output={out.resolve()}')
b.k32.CloseHandle(hp)
