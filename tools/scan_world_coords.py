#!/usr/bin/env python3
"""List map-coordinate candidates sharing a stable Y plane in cshell.dll."""
import json, math, struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import runtime_hotkey_batch as b
import runtime_patch_capture as r
pid=b.find_pid()
if not pid: raise SystemExit('crossfire not found')
hp=b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION|b.PROCESS_VM_READ,False,pid)
m=next((x for x in r.modules(pid) if x['name']=='cshell.dll'),None)
if not m: raise SystemExit('cshell.dll not loaded')
snap=r.snapshot(hp,[m]); rows=[]
for (name,rva),data in snap.items():
 for off in range(0,len(data)-12,4):
  try:v=struct.unpack_from('<3f',data,off)
  except struct.error:continue
  if not all(math.isfinite(x) and abs(x)<20000 for x in v):continue
  # report triples where any component is near the observed map Y=-2356
  if not any(abs(x+2356.0)<3 for x in v):continue
  if max(abs(x) for x in v)<100:continue
  rows.append({'rva':hex(rva+off),'values':[round(x,3) for x in v]})
out=Path('runtime-diffs/match-baseline/world-coords.json');out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps({'pid':pid,'count':len(rows),'rows':rows},ensure_ascii=False,indent=2),encoding='utf-8')
print('pid',pid,'count',len(rows),'output',out.resolve())
for x in rows[:200]:print(x['rva'],x['values'])
b.k32.CloseHandle(hp)
