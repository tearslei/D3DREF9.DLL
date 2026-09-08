#!/usr/bin/env python3
"""Batch runtime capture for the local CF test client.

For each configured hotkey this tool:
  1) snapshots readable pages of crossfire.exe/cshell.dll/D3DREF9.DLL;
  2) sends the F-key + second-key through Win32 SendInput;
  3) captures short/settled snapshots and computes module+RVA diffs;
  4) optionally sends a second press to restore ordinary boolean toggles.

It only records user-mode memory deltas; it does not patch the target process.
"""
from __future__ import annotations
import argparse, ctypes, ctypes.wintypes as wt, json, os, sys, time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from runtime_patch_capture import modules, snapshot, diff, read_region  # type: ignore

user32 = ctypes.WinDLL("user32", use_last_error=True)
k32 = ctypes.WinDLL("kernel32", use_last_error=True)
PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_READ = 0x0010
INPUT_KEYBOARD = 1
KEYEVENTF_KEYUP = 0x0002
KEYEVENTF_EXTENDEDKEY = 0x0001
SW_RESTORE = 9

class KEYBDINPUT(ctypes.Structure):
    _fields_ = [("wVk", wt.WORD), ("wScan", wt.WORD), ("dwFlags", wt.DWORD),
                ("time", wt.DWORD), ("dwExtraInfo", ctypes.c_void_p)]
class INPUTUNION(ctypes.Union):
    _fields_ = [("ki", KEYBDINPUT)]
class INPUT(ctypes.Structure):
    _anonymous_ = ("u",)
    _fields_ = [("type", wt.DWORD), ("u", INPUTUNION)]

user32.SendInput.argtypes = [wt.UINT, ctypes.POINTER(INPUT), ctypes.c_int]
user32.SendInput.restype = wt.UINT
user32.SetForegroundWindow.argtypes = [wt.HWND]
user32.ShowWindow.argtypes = [wt.HWND, ctypes.c_int]
user32.GetForegroundWindow.restype = wt.HWND
user32.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
k32.OpenProcess.restype = wt.HANDLE
k32.CloseHandle.argtypes = [wt.HANDLE]

FKEYS = {"F7":0x76, "F9":0x78, "F10":0x79, "F11":0x7A}
GLOBAL_RVAS = [0x1A8878,0x1A8A88,0x1A8A90,0x1A8A98,0x1A8A9C,0x1A8AA0,0x1A8AB0,0x1A8ABC,0x1A8BF4,0x1A8C24,0x1A8CDC,0x1A8D28,0x1A8D50,0x1A8D58,0x1A8D5C,0x1A8D60,0x1A8D6C,0x1A8D74,0x1A8D88,0x1A8D8C,0x1A8D98,0x1A8DA4,0x1A8DB4,0x1A8DBC,0x1A8DC4,0x1A8DCC,0x1A8DD0,0x1A8DD4,0x1A8DD8,0x1A8DE0,0x1A8DEC,0x1A8DF4,0x1A8E04,0x1A8E0C,0x1A8E10,0x1A8E18,0x1A8E24,0x1A8E44,0x1A8F20,0x1A8F28]

DEFAULT_KEYS = [
    "F7+Y","F7+D","F7+1","F7+2","F7+3","F7+4","F7+5","F7+6","F7+7","F7+8","F7+9","F7+0",
    "F9+1","F9+2","F10+X","F10+Z","F11+1","F11+2","F11+3","F11+4","F11+5","F11+6","F11+7",
    "F9+3","F9+4","F9+6","F9+7","F9+8","F9+9","F9+A","F9+C","F9+D","F9+E","F9+F","F9+G","F9+H","F9+J","F9+K","F9+L","F9+N","F9+O","F9+P","F9+Q","F9+R","F9+T","F9+V","F9+X","F9+Y",
    "F10+2","F10+4","F10+5","F10+6","F10+7","F10+8","F10+9","F10+0","F10+D","F10+E","F10+G","F10+H","F10+I","F10+J","F10+K","F10+M","F10+N","F10+Q","F10+R","F10+W",
]
# Confirmed ordinary toggles from the static cross-reference. Unknown keys are single-shot.
TOGGLES = {
    "F7+2","F7+3","F7+4","F7+5","F7+6","F7+7","F7+8","F7+9","F7+0",
    "F9+1","F9+2","F9+4","F9+6","F9+7","F9+8","F9+9","F9+A","F9+C","F9+D","F9+E","F9+F","F9+G","F9+H","F9+J","F9+K","F9+L","F9+N","F9+O","F9+P","F9+Q","F9+R","F9+T","F9+V","F9+X","F9+Y",
    "F10+W","F10+X"
}
ACTIONS = {"F7+Y","F7+D","F7+1"}
MULTI = {"F9+3"}
PAIR = {"F7+Q","F7+W"}

def parse_combo(s: str):
    s = s.strip().upper().replace(" ", "")
    if "+" not in s: raise ValueError(s)
    f, k = s.split("+", 1)
    if f not in FKEYS or len(k) != 1: raise ValueError(s)
    return FKEYS[f], ord(k)

def key_event(vk: int, down: bool):
    flags = 0 if down else KEYEVENTF_KEYUP
    x = INPUT(type=INPUT_KEYBOARD, ki=KEYBDINPUT(vk, 0, flags, 0, None))
    sent = user32.SendInput(1, ctypes.byref(x), ctypes.sizeof(INPUT))
    if sent != 1:
        user32.keybd_event(vk, 0, flags, None)

def send_combo(combo: str, hold_ms: int = 45):
    mod, key = parse_combo(combo)
    key_event(mod, True); time.sleep(0.02)
    key_event(key, True); time.sleep(hold_ms/1000)
    key_event(key, False); time.sleep(0.02)
    key_event(mod, False)

def find_pid():
    import subprocess
    # During login/map transitions CF may leave an older helper process alive.
    # Prefer the newest process that owns a visible window, then newest start
    # time, instead of an arbitrary Select-Object -First 1 result.
    command = "(Get-Process -Name crossfire -ErrorAction SilentlyContinue | Sort-Object @{Expression={$_.MainWindowHandle -ne 0};Descending=$true},StartTime -Descending | Select-Object -First 1 -Expand Id)"
    out = subprocess.run(["powershell","-NoProfile","-Command",command],capture_output=True,text=True).stdout.strip()
    return int(out) if out.isdigit() else None

def read_globals(hp, mods):
    d = next((m for m in mods if m["name"] == "d3dref9.dll"), None)
    if not d: return {}
    out = {}
    for rva in GLOBAL_RVAS:
        b = read_region(hp, d["base"] + rva, 4)
        if len(b)==4: out[hex(rva)] = b.hex()
    return out

def find_hwnd(pid: int):
    found = []
    EnumWindows = user32.EnumWindows
    EnumWindowsProc = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
    def cb(hwnd, _):
        p = wt.DWORD(); user32.GetWindowThreadProcessId(hwnd, ctypes.byref(p))
        if p.value == pid and user32.IsWindowVisible(hwnd): found.append(hwnd)
        return True
    EnumWindows(EnumWindowsProc(cb), 0)
    return found[0] if found else None

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--pid", type=int); ap.add_argument("--keys", help="comma separated combos; default is the supplied list"); ap.add_argument("--out", type=Path, default=Path("runtime-diffs/batch")); ap.add_argument("--settle-ms", type=int, default=800); ap.add_argument("--dry-run", action="store_true"); ap.add_argument("--globals-only", action="store_true"); ap.add_argument("--exec-only", action="store_true"); ap.add_argument("--restore-wait-ms", type=int, default=1200); ns=ap.parse_args()
    pid = ns.pid or find_pid()
    if not pid: raise SystemExit("未找到 crossfire.exe")
    hp = k32.OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, False, pid)
    if not hp: raise OSError(ctypes.get_last_error(), f"OpenProcess({pid})")
    try:
        mods = modules(pid); names={m["name"] for m in mods}
        print(f"PID={pid}; modules={','.join(sorted(names))}", flush=True)
        if "crossfire.exe" not in names: raise SystemExit("指定 PID 未加载 crossfire.exe")
        hwnd = find_hwnd(pid)
        if hwnd:
            user32.ShowWindow(hwnd, SW_RESTORE); user32.SetForegroundWindow(hwnd)
        keys = [x.strip().upper() for x in (ns.keys.split(",") if ns.keys else DEFAULT_KEYS) if x.strip()]
        ns.out.mkdir(parents=True, exist_ok=True)
        manifest=[]
        for idx, combo in enumerate(keys, 1):
            try: parse_combo(combo)
            except ValueError: print(f"[{idx}/{len(keys)}] 跳过格式错误: {combo}", flush=True); continue
            print(f"[{idx}/{len(keys)}] {combo}: baseline", flush=True)
            if hwnd: user32.SetForegroundWindow(hwnd)
            time.sleep(0.15)
            before = snapshot(hp, mods)
            globals_before = read_globals(hp, mods)
            if not ns.dry_run:
                send_combo(combo)
            t_short = time.time(); time.sleep(0.15); short = {} if ns.globals_only else snapshot(hp,mods, ns.exec_only)
            globals_short = read_globals(hp, mods)
            time.sleep(max(0, ns.settle_ms-150)/1000); settled = {} if ns.globals_only else snapshot(hp,mods, ns.exec_only)
            globals_settled = read_globals(hp, mods)
            rows = [{"phase":"short","changes":diff(before,short,mods),"globals":globals_short}, {"phase":"settled","changes":diff(before,settled,mods),"globals":globals_settled}]
            restore = combo in TOGGLES
            if restore and not ns.dry_run:
                time.sleep(0.15); send_combo(combo); time.sleep(ns.restore_wait_ms/1000)
                restored = {} if ns.globals_only else snapshot(hp,mods, ns.exec_only); rows.append({"phase":"restored","changes":diff(before,restored,mods)})
            record={"pid":pid,"combo":combo,"classification":"toggle" if restore else ("multi-state" if combo in MULTI else ("pair" if combo in PAIR else ("action" if combo in ACTIONS else "single-shot/unknown"))),"timestamp":t_short,"modules":mods,"globals_before":globals_before,"phases":rows}
            path=ns.out/f"{idx:02d}_{combo.replace('+','_')}.json"; path.write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding="utf-8")
            counts=[len(x["changes"]) for x in rows]
            print(f"    diff segments: {counts}; saved {path}", flush=True)
            manifest.append({"combo":combo,"file":str(path),"classification":record["classification"],"segments":counts})
        (ns.out/"manifest.json").write_text(json.dumps({"pid":pid,"count":len(manifest),"items":manifest},ensure_ascii=False,indent=2),encoding="utf-8")
        print(f"完成：{len(manifest)} 项；清单 {ns.out/'manifest.json'}")
    finally: k32.CloseHandle(hp)

if __name__ == "__main__": main()










