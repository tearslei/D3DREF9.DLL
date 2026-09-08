import sys,time,json
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import runtime_hotkey_batch as b, runtime_patch_capture as r
pid=b.find_pid();
if not pid: raise SystemExit('crossfire not found')
hp=b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION|b.PROCESS_VM_READ,False,pid); mods=[m for m in r.modules(pid) if m['name']=='d3dref9.dll']
out=Path('runtime-diffs')/'focused'; out.mkdir(parents=True,exist_ok=True)
for combo,rva in [('F7+6','0x1a8d58'),('F7+8','0x1a8d60')]:
 gb=b.read_globals(hp,mods); before=r.snapshot(hp,mods); print(combo,'before',gb[rva],flush=True)
 b.send_combo(combo); time.sleep(1.2); ge=b.read_globals(hp,mods); en=r.snapshot(hp,mods); print(combo,'enabled',ge[rva],flush=True)
 time.sleep(.3); b.send_combo(combo); time.sleep(1.8); gd=b.read_globals(hp,mods); dis=r.snapshot(hp,mods); print(combo,'disabled',gd[rva],flush=True)
 rec={'pid':pid,'combo':combo,'global_rva':rva,'globals_before':gb,'globals_enabled':ge,'globals_disabled':gd,'enable_diff':r.diff(before,en,mods),'disable_diff':r.diff(en,dis,mods),'modules':mods}
 (out/(combo.replace('+','_')+'.json')).write_text(json.dumps(rec,ensure_ascii=False,indent=2),encoding='utf-8')
 print(combo,'enable_segments',len(rec['enable_diff']),'disable_segments',len(rec['disable_diff']),flush=True)
# final state
print('final',b.read_globals(hp,mods),flush=True); b.k32.CloseHandle(hp)
