import sys,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import runtime_hotkey_batch as b, runtime_patch_capture as r
pid=b.find_pid(); hp=b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION|b.PROCESS_VM_READ,False,pid); mods=r.modules(pid)
for i in range(3):
 print('before',i,b.read_globals(hp,mods).get('0x1a8de0')); b.send_combo('F9+J');
 for t in [0.2,0.8,1.5]: time.sleep(t if t==0.2 else t-([0.2,0.8,1.5][[0.2,0.8,1.5].index(t)-1])); print(' t',t,b.read_globals(hp,mods).get('0x1a8de0'))
b.k32.CloseHandle(hp)
