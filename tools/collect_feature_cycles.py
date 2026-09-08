#!/usr/bin/env python3
"""Run controlled, reversible hotkey cycles while doing read-only sampling.

The collector never writes target memory.  It deliberately excludes Alt+1,
F7+Q and F7+W (aim/fire) and records the D3DREF9 state bytes plus a small set
of previously observed transform candidates.  F7+Y is treated as a one-shot
action because the original handler has no reversible state byte.
"""
from __future__ import annotations
import argparse, csv, json, struct, sys, threading, time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import runtime_hotkey_batch as b
import runtime_patch_capture as r

FEATURES = [
    ('两键优化游戏进程', 'F7+Y', None, 'action'),
    ('房间挂房不卡', 'F7+3', 0x1A8D50, 'toggle'),
    ('游戏旧不掉血', 'F7+5', 0x1A8A88, 'toggle'),
    ('游戏摔不掉血', 'F9+V', 0x1A8C24, 'toggle'),
    ('无限背包', 'F9+J', 0x1A8DE0, 'toggle'),
    ('刷无线电', 'F10+X', 0x1A8F28, 'toggle'),
    ('人物透视', 'F7+6', 0x1A8D58, 'toggle'),
    ('无后坐力', 'F7+8', 0x1A8D60, 'toggle'),
    ('零秒换弹', 'F7+9', 0x1A8A90, 'toggle'),
    ('子弹穿墙', 'F7+0', 0x1A8A98, 'toggle'),
    ('刀枪爆头', 'F9+1', 0x1A8A9C, 'toggle'),
    ('第三人称', 'F9+L', 0x1A8DF4, 'toggle'),
    ('空格连跳', 'F9+D', 0x1A8DBC, 'toggle'),
    ('人物穿墙', 'F9+E', 0x1A8DC4, 'toggle'),
    ('瞬移通地', 'F10+W', 0x1A8F20, 'toggle'),
]
CANDIDATES = {
    'crossfire.exe': [0xDB90C4, 0xDBFE54, 0xDBFE94, 0xDC13E0, 0xDCBB9C, 0xDD7AA0],
    'cshell.dll': [0x16A53E0, 0x16A5400, 0x1E71000, 0x1E71BA4, 0x1E71BB0, 0x1E8E498],
}

def scalar(hp, mods, module, rva):
    m = mods.get(module)
    raw = r.read_region(hp, m['base'] + rva, 16) if m else b''
    if len(raw) != 16: return [None] * 4
    return list(struct.unpack('<4f', raw))

def make_phase(hp, mods, globals_map, phase, cycle, label, combo):
    row = {'t': time.time(), 'cycle': cycle, 'feature': label, 'combo': combo, 'phase': phase}
    for rv in b.GLOBAL_RVAS:
        row[f'd3d@{rv:x}'] = globals_map.get(hex(rv), '')
    for module, rvas in CANDIDATES.items():
        for rv in rvas:
            vals = scalar(hp, mods, module, rv)
            for i, v in enumerate(vals): row[f'{module}@{rv:x}+{i}'] = v
    return row

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--pid', type=int, default=0)
    ap.add_argument('--cycles', type=int, default=4)
    ap.add_argument('--baseline', type=float, default=2.0)
    ap.add_argument('--settle', type=float, default=4.0)
    ap.add_argument('--gap', type=float, default=2.0)
    ap.add_argument('--out', type=Path, default=Path('runtime-diffs/feature-cycles'))
    ns = ap.parse_args()
    pid = ns.pid or b.find_pid()
    if not pid: raise SystemExit('crossfire not found')
    hp = b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION | b.PROCESS_VM_READ, False, pid)
    if not hp: raise OSError('OpenProcess failed')
    mods = {m['name']: m for m in r.modules(pid)}
    hwnd = b.find_hwnd(pid)
    if hwnd:
        b.user32.ShowWindow(hwnd, b.SW_RESTORE); b.user32.SetForegroundWindow(hwnd)
    ns.out.mkdir(parents=True, exist_ok=True)
    run = f'pid{pid}-{int(time.time())}'
    csv_path = ns.out / f'{run}.csv'; json_path = ns.out / f'{run}.json'
    fields = ['t','cycle','feature','combo','phase']
    fields += [f'd3d@{rv:x}' for rv in b.GLOBAL_RVAS]
    fields += [f'{mn}@{rv:x}+{i}' for mn, rs in CANDIDATES.items() for rv in rs for i in range(4)]
    operations=[]; stop = threading.Event(); timeline=[]
    def sample_loop():
        while not stop.is_set():
            gm=b.read_globals(hp, list(mods.values()))
            row={'t':time.time(),'cycle':'','feature':'','combo':'','phase':'continuous'}
            for rv in b.GLOBAL_RVAS: row[f'd3d@{rv:x}']=gm.get(hex(rv),'')
            for module,rvas in CANDIDATES.items():
                for rv in rvas:
                    vals=scalar(hp,mods,module,rv)
                    for i,v in enumerate(vals): row[f'{module}@{rv:x}+{i}']=v
            timeline.append(row); time.sleep(0.1)
    th=threading.Thread(target=sample_loop, daemon=True); th.start()
    def wait_s(seconds):
        end=time.time()+max(0,seconds)
        while time.time()<end: time.sleep(min(0.1,end-time.time()))
    try:
        print(f'PID={pid}; cycles={ns.cycles}; features={len(FEATURES)}', flush=True)
        for cycle in range(1, ns.cycles+1):
            for label, combo, rva, kind in FEATURES:
                if hwnd: b.user32.SetForegroundWindow(hwnd)
                wait_s(ns.gap)
                before=b.read_globals(hp,list(mods.values()))
                # Normalize a known toggle to OFF before its baseline.
                if kind == 'toggle' and rva is not None and before.get(hex(rva),'') != '00000000':
                    b.send_combo(combo); wait_s(ns.settle)
                    before=b.read_globals(hp,list(mods.values()))
                rec={'cycle':cycle,'feature':label,'combo':combo,'kind':kind,
                     'before':before,'timestamp_before':time.time()}
                if kind == 'action':
                    b.send_combo(combo); wait_s(ns.settle)
                    rec['after_trigger']=b.read_globals(hp,list(mods.values()))
                    rec['phase_rows']=[make_phase(hp,mods,rec['after_trigger'],'trigger',cycle,label,combo)]
                else:
                    b.send_combo(combo); wait_s(ns.settle)
                    enabled=b.read_globals(hp,list(mods.values()))
                    rec['enabled']=enabled
                    rec['phase_rows']=[make_phase(hp,mods,enabled,'enabled',cycle,label,combo)]
                    b.send_combo(combo); wait_s(ns.settle)
                    rec['disabled']=b.read_globals(hp,list(mods.values()))
                    rec['phase_rows'].append(make_phase(hp,mods,rec['disabled'],'disabled',cycle,label,combo))
                operations.append(rec)
                print(f'cycle {cycle}/{ns.cycles}: {combo} {kind} captured', flush=True)
        print('feature cycles complete', flush=True)
    finally:
        stop.set(); th.join(timeout=2)
        b.k32.CloseHandle(hp)
    with csv_path.open('w', newline='', encoding='utf-8') as f:
        w=csv.DictWriter(f,fieldnames=fields); w.writeheader(); w.writerows(timeline)
    json_path.write_text(json.dumps({'pid':pid,'modules':mods,'operations':operations,
                                     'timeline_rows':len(timeline),'excluded':['Alt+1','F7+Q','F7+W']},ensure_ascii=False,indent=2),encoding='utf-8')
    print(f'CSV={csv_path.resolve()}'); print(f'JSON={json_path.resolve()}')

if __name__ == '__main__': main()
