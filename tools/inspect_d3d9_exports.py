#!/usr/bin/env python3
"""Record D3D9 export/device-module facts for the live CF process."""
import json, subprocess, sys, time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import runtime_hotkey_batch as b, runtime_patch_capture as r
pid=b.find_pid()
if not pid: raise SystemExit('crossfire not found')
mods=r.modules(pid)
rows=[]
for m in mods:
 if m['name'] in ('d3d9.dll','d3dref9.dll'):
  rows.append(m)
out=Path('runtime-diffs/match-baseline/d3d9-modules.json');out.parent.mkdir(parents=True,exist_ok=True)
out.write_text(json.dumps({'pid':pid,'timestamp':time.time(),'modules':rows},ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'pid':pid,'modules':rows},ensure_ascii=False,indent=2))
