import sys,json,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import runtime_hotkey_batch as b, runtime_patch_capture as r
pid=b.find_pid()
if not pid: raise SystemExit("crossfire not found")
hp=b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION|b.PROCESS_VM_READ,False,pid)
mods=r.modules(pid)
state={'pid':pid,'timestamp':time.time(),'modules':mods,'globals':b.read_globals(hp,mods)}
out=Path('runtime-diffs')/'baseline_current.json'
out.write_text(json.dumps(state,ensure_ascii=False,indent=2),encoding='utf-8')
print(out.resolve())
print([(k,v) for k,v in state['globals'].items() if v!='00000000'])
b.k32.CloseHandle(hp)
