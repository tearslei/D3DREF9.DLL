import sys,time,ctypes,ctypes.wintypes as wt
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE))
import runtime_hotkey_batch as b
import runtime_patch_capture as r
pid=12648
k=b.k32; hp=k.OpenProcess(b.PROCESS_QUERY_INFORMATION|b.PROCESS_VM_READ,False,pid); mods=r.modules(pid)
items=[('F7+2','0x1a8bf4'),('F9+4','0x1a8d28'),('F7+6','0x1a8d58'),('F7+7','0x1a8d5c'),('F7+8','0x1a8d60')]
for combo,rva in items:
    g=b.read_globals(hp,mods).get(rva); print(combo,'before',g,flush=True)
    if g and g!='00000000':
      b.send_combo(combo); time.sleep(.35)
      print(combo,'after',b.read_globals(hp,mods).get(rva),flush=True)
k.CloseHandle(hp)
