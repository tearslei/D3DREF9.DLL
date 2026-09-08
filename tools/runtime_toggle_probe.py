#!/usr/bin/env python3
import argparse,sys,time,json
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE))
import runtime_hotkey_batch as b
import runtime_patch_capture as r

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--pid',type=int);ap.add_argument('--keys',required=True);ap.add_argument('--out',type=Path,default=Path('runtime-diffs/toggle-probe'));ap.add_argument('--wait-ms',type=int,default=800);ap.add_argument('--exec-only',action='store_true');ns=ap.parse_args()
 pid=ns.pid or b.find_pid();
 if not pid: raise SystemExit('crossfire not found')
 hp=b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION|b.PROCESS_VM_READ,False,pid);mods=r.modules(pid);ns.out.mkdir(parents=True,exist_ok=True)
 hwnd=b.find_hwnd(pid)
 if hwnd:b.user32.ShowWindow(hwnd,b.SW_RESTORE);b.user32.SetForegroundWindow(hwnd)
 for combo in [x.strip().upper() for x in ns.keys.split(',') if x.strip()]:
  if hwnd:b.user32.SetForegroundWindow(hwnd)
  time.sleep(.2)
  base=r.snapshot(hp,mods,ns.exec_only); gb=b.read_globals(hp,mods)
  b.send_combo(combo);time.sleep(ns.wait_ms/1000)
  en=r.snapshot(hp,mods,ns.exec_only); ge=b.read_globals(hp,mods)
  # second press: restore; allow full settle before capture
  b.send_combo(combo);time.sleep(ns.wait_ms/1000)
  dis=r.snapshot(hp,mods,ns.exec_only); gd=b.read_globals(hp,mods)
  rec={'pid':pid,'combo':combo,'globals_before':gb,'globals_enabled':ge,'globals_disabled':gd,'enable_from_baseline':r.diff(base,en,mods),'disable_vs_enable':r.diff(en,dis,mods),'modules':mods}
  path=ns.out/(combo.replace('+','_')+'.json');path.write_text(json.dumps(rec,ensure_ascii=False,indent=2),encoding='utf-8')
  print(combo,'globals',gb,'=>',ge,'=>',gd,'code/data segments',len(rec['disable_vs_enable']),flush=True)
 b.k32.CloseHandle(hp)
if __name__=='__main__':main()

