import sys,struct,math,json,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent)); import runtime_hotkey_batch as b, runtime_patch_capture as r
pid=b.find_pid(); hp=b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION|b.PROCESS_VM_READ,False,pid);mods=r.modules(pid);out=[]
for m in mods:
 if m['name'] not in ('cshell.dll','crossfire.exe'): continue
 snap=r.snapshot(hp,[m]);
 for (name,rva),data in snap.items():
  # scan 4x4 float matrices with last row near [0,0,0,1] or [0,0,1,0]
  for off in range(0,len(data)-64,4):
   try: fs=struct.unpack_from('<16f',data,off)
   except: continue
   if not all(math.isfinite(x) and abs(x)<1e5 for x in fs): continue
   if (abs(fs[15]-1)<0.01 and sum(abs(fs[i]) for i in (12,13,14))<0.2) or (abs(fs[11]-1)<0.01 and sum(abs(fs[i]) for i in (3,7,15))<0.2):
    out.append({'module':name,'rva':hex(rva+off),'values':[round(x,4) for x in fs]})
print('matrix_candidates',len(out));
for x in out[:100]:print(x)
Path('runtime-diffs/match-baseline/matrix-candidates.json').write_text(json.dumps({'pid':pid,'timestamp':time.time(),'candidates':out},ensure_ascii=False,indent=2),encoding='utf-8');b.k32.CloseHandle(hp)
