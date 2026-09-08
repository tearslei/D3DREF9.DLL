#!/usr/bin/env python3
"""Five-minute F9+V focused read-only sampler.
Enables F9+V once, samples D3DREF9 state and known runtime candidates for 300s,
then disables it and records the final state. No target-memory writes.
"""
from __future__ import annotations
import argparse,csv,json,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import runtime_hotkey_batch as b, runtime_patch_capture as r

def main():
 ap=argparse.ArgumentParser(); ap.add_argument('--pid',type=int,default=0); ap.add_argument('--duration',type=float,default=300); ap.add_argument('--interval',type=float,default=.1); ap.add_argument('--pre-delay',type=float,default=0,help='进入对局后等待秒数，再发送 F9+V'); ap.add_argument('--out',type=Path,default=Path('runtime-diffs/f9v-focus')); ns=ap.parse_args()
 pid=ns.pid or b.find_pid()
 if not pid: raise SystemExit('crossfire not found')
 hp=b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION|b.PROCESS_VM_READ,False,pid)
 if not hp: raise OSError('OpenProcess failed')
 mods={m['name']:m for m in r.modules(pid)}
 hwnd=b.find_hwnd(pid)
 if hwnd:
  b.user32.ShowWindow(hwnd,b.SW_RESTORE); b.user32.SetForegroundWindow(hwnd)
 ns.out.mkdir(parents=True,exist_ok=True); run=f'pid{pid}-{int(time.time())}'
 csvp=ns.out/f'{run}.csv'; jsonp=ns.out/f'{run}.json'
 targets={'crossfire.exe':[0xdb90c4,0xdbfe54,0xdbfe94,0xdc13e0,0xdcbb9c,0xdd7aa0], 'cshell.dll':[0x16a53e0,0x16a5400,0x1e71000,0x1e71ba4,0x1e71bb0,0x1e8e498]}
 fields=['t','elapsed','pid']+[f'{mn}@{rv:x}+{i}' for mn,rs in targets.items() for rv in rs for i in range(4)]+[f'd3d@{rv:x}' for rv in b.GLOBAL_RVAS]
 def row():
  x={'t':time.time(),'elapsed':time.time()-start,'pid':pid}
  for mn,rs in targets.items():
   m=mods.get(mn)
   for rv in rs:
    raw=r.read_region(hp,m['base']+rv,16) if m else b''
    vals=struct.unpack('<4f',raw) if len(raw)==16 else [None]*4
    for i,v in enumerate(vals): x[f'{mn}@{rv:x}+{i}']=v
  dm=mods.get('d3dref9.dll')
  for rv in b.GLOBAL_RVAS:
   raw=r.read_region(hp,dm['base']+rv,4) if dm else b''
   x[f'd3d@{rv:x}']=raw.hex() if len(raw)==4 else ''
  return x
 if ns.pre_delay>0:
  print(f'pre-delay {ns.pre_delay:.1f}s; enter/settle the match now',flush=True)
  time.sleep(ns.pre_delay)
 start=time.time(); before=row(); b.send_combo('F9+V'); enabled_at=time.time(); time.sleep(1.0)
 rows=[]; last=before
 with csvp.open('w',newline='',encoding='utf-8') as f:
  w=csv.DictWriter(f,fieldnames=fields); w.writeheader()
  while time.time()-enabled_at<ns.duration:
   x=row(); rows.append(x); w.writerow(x); f.flush(); last=x; time.sleep(max(.02,ns.interval))
 b.send_combo('F9+V'); time.sleep(1.0); after=row()
 changes=[]
 for k in fields:
  if k in ('t','elapsed','pid'): continue
  if before.get(k)!=after.get(k): changes.append({'key':k,'before':before.get(k),'after':after.get(k)})
 rec={'pid':pid,'duration':ns.duration,'interval':ns.interval,'pre_delay':ns.pre_delay,'modules':mods,'hotkey':'F9+V','before':before,'after_disable':after,'rows':len(rows),'changes_after_full_window':changes,'note':'F9+V was enabled once after pre-delay and disabled once at end; rows are read-only samples.'}
 jsonp.write_text(json.dumps(rec,ensure_ascii=False,indent=2),encoding='utf-8')
 print(f'complete pid={pid} rows={len(rows)} csv={csvp.resolve()} json={jsonp.resolve()}')
 b.k32.CloseHandle(hp)
if __name__=='__main__': main()
