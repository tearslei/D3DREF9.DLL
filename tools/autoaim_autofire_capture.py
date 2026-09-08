import sys,time,json,ctypes,ctypes.wintypes as wt
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE))
import runtime_hotkey_batch as b
import runtime_patch_capture as r
pid=12648
hp=b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION|b.PROCESS_VM_READ,False,pid); mods=r.modules(pid)
# bring CF to foreground
hwnd=b.find_hwnd(pid)
if hwnd: b.user32.ShowWindow(hwnd,b.SW_RESTORE); b.user32.SetForegroundWindow(hwnd)
def alt1():
    b.key_event(0xA4,True); time.sleep(.03); b.key_event(0x31,True); time.sleep(.06); b.key_event(0x31,False); time.sleep(.03); b.key_event(0xA4,False)
def capture(label, action):
    before=r.snapshot(hp,mods); gb=b.read_globals(hp,mods); print(label,'before globals',gb,flush=True)
    action(); time.sleep(.2); gs=b.read_globals(hp,mods); after=r.snapshot(hp,mods); ch=r.diff(before,after,mods)
    rec={'label':label,'globals_before':gb,'globals_after':gs,'changes':ch,'modules':mods,'timestamp':time.time()}
    out=Path('runtime-diffs')/'special'; out.mkdir(parents=True,exist_ok=True); (out/(label+'.json')).write_text(json.dumps(rec,ensure_ascii=False,indent=2),encoding='utf-8')
    print(label,'after globals',gs,'diff_segments',len(ch),flush=True)

capture('Alt1_autoaim',alt1)
capture('F7Q_autofire_start',lambda:b.send_combo('F7+Q'))
time.sleep(.5)
capture('F7W_autofire_stop',lambda:b.send_combo('F7+W'))
# read final globals
print('final nonzero',[(k,v) for k,v in b.read_globals(hp,mods).items() if v!='00000000'],flush=True)
b.k32.CloseHandle(hp)
