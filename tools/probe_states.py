import sys,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import runtime_hotkey_batch as b, runtime_patch_capture as r
pid=12648; hp=b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION|b.PROCESS_VM_READ,False,pid); mods=r.modules(pid)
for n in range(1,5):
 print('before',n,b.read_globals(hp,mods).get('0x1a8ab0'),b.read_globals(hp,mods).get('0x1a8d6c'),flush=True)
 b.send_combo('F9+3'); time.sleep(.3)
 print('after',n,b.read_globals(hp,mods).get('0x1a8ab0'),b.read_globals(hp,mods).get('0x1a8d6c'),flush=True)
b.k32.CloseHandle(hp)
