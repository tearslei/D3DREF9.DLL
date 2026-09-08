import sys,time,struct,json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent)); import runtime_patch_capture as r, runtime_hotkey_batch as b
pid=b.find_pid();hp=b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION|b.PROCESS_VM_READ,False,pid);m=next(x for x in r.modules(pid) if x['name']=='cshell.dll');addr=m['base']+0x1e8e478
rows=[]
for i in range(30):
 raw=r.read_region(hp,addr,64); vals=struct.unpack('<16f',raw) if len(raw)==64 else []
 rows.append({'t':time.time(),'values':vals}); print(i,[round(x,3) for x in vals],flush=True);time.sleep(.1)
Path('runtime-diffs/match-baseline/cshell_candidate_1e8e478.json').write_text(json.dumps({'pid':pid,'address':hex(addr),'rows':rows},indent=2),encoding='utf-8');b.k32.CloseHandle(hp)
