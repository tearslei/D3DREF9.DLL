import sys,struct,math,json,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent)); import runtime_hotkey_batch as b, runtime_patch_capture as r
pid=b.find_pid();hp=b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION|b.PROCESS_VM_READ,False,pid);mods=[m for m in r.modules(pid) if m['name'] in ('cshell.dll','crossfire.exe')]
a=r.snapshot(hp,mods);time.sleep(1.0);c=r.snapshot(hp,mods);out=[]
for key,d1 in a.items():
 d2=c.get(key,b'');
 for off in range(0,min(len(d1),len(d2))-64,4):
  try:x=struct.unpack_from('<16f',d1,off);y=struct.unpack_from('<16f',d2,off)
  except:continue
  if not all(math.isfinite(v) and abs(v)<1e5 for v in x+y):continue
  if any(abs(x[i]-y[i])>1e-3 for i in range(16)) and abs(y[15]-1)<.05 and sum(abs(y[i]) for i in (12,13,14))<5000:
   out.append({'module':key[0],'rva':hex(key[1]+off),'before':[round(v,3) for v in x],'after':[round(v,3) for v in y]})
print('dynamic candidates',len(out));
for q in out[:80]:print(q)
Path('runtime-diffs/match-baseline/dynamic-matrix-candidates.json').write_text(json.dumps({'pid':pid,'candidates':out},ensure_ascii=False,indent=2),encoding='utf-8');b.k32.CloseHandle(hp)
