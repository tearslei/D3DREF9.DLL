import sys,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import runtime_hotkey_batch as b, runtime_patch_capture as r
pid=b.find_pid(); hp=b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION|b.PROCESS_VM_READ,False,pid); mods=r.modules(pid)
map={'F9+1':'0x1a8a9c','F9+4':'0x1a8d28','F9+J':'0x1a8de0'}
for combo,rva in map.items():
 g=b.read_globals(hp,mods).get(rva); print(combo,'before',g)
 if g!='00000000': b.send_combo(combo); time.sleep(1.5); print(combo,'after',b.read_globals(hp,mods).get(rva))
print('nonzero',[(k,v) for k,v in b.read_globals(hp,mods).items() if v!='00000000'])
b.k32.CloseHandle(hp)
