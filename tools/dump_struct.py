import sys,struct
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent)); import runtime_patch_capture as r, runtime_hotkey_batch as b
pid=b.find_pid();hp=b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION|b.PROCESS_VM_READ,False,pid);mods=r.modules(pid)
for mn,rva in [('crossfire.exe',0xdbfe54),('cshell.dll',0x1e8e478)]:
 m=next(x for x in mods if x['name']==mn);a=m['base']+rva;raw=r.read_region(hp,a-128,512);print('\n',mn,hex(a),'len',len(raw))
 for off in range(0,len(raw),16):
  vals=struct.unpack_from('<4f',raw,off); print(hex(rva-128+off), ' '.join(f'{x:10.3f}' for x in vals))
b.k32.CloseHandle(hp)
