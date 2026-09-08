import sys,json,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent)); import runtime_hotkey_batch as b, runtime_patch_capture as r
pid=b.find_pid();
if not pid: raise SystemExit('crossfire not found')
hp=b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION|b.PROCESS_VM_READ,False,pid);mods=r.modules(pid);snap=r.snapshot(hp,mods)
out=Path('runtime-diffs')/'match-baseline';out.mkdir(parents=True,exist_ok=True)
(out/'baseline.json').write_text(json.dumps({'pid':pid,'timestamp':time.time(),'modules':mods,'pages':len(snap),'globals':b.read_globals(hp,mods)},ensure_ascii=False,indent=2),encoding='utf-8')
print('pid',pid,'pages',len(snap),'globals_nonzero',[(k,v) for k,v in b.read_globals(hp,mods).items() if v!='00000000'])
b.k32.CloseHandle(hp)
