#!/usr/bin/env python3
"""Interactive module snapshot/diff tool for the local CF test client.

It does not inject code or alter the target process.  It snapshots readable
pages of crossfire.exe/cshell.dll/D3DREF9.DLL, lets the operator toggle one
feature, then reports byte changes as module+RVA with old/new bytes.
"""
from __future__ import annotations
import argparse, ctypes, ctypes.wintypes as wt, json, os, subprocess, sys, time
from pathlib import Path

kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
TH32CS_SNAPMODULE = 0x00000008
TH32CS_SNAPMODULE32 = 0x00000010
PROCESS_QUERY_INFORMATION = 0x0400
PROCESS_VM_READ = 0x0010
LIST_MODULES = ("crossfire.exe", "cshell.dll", "d3dref9.dll")

class MODULEENTRY32W(ctypes.Structure):
    _fields_ = [("dwSize", wt.DWORD), ("th32ModuleID", wt.DWORD),
                ("th32ProcessID", wt.DWORD), ("GlblcntUsage", wt.DWORD),
                ("ProccntUsage", wt.DWORD), ("modBaseAddr", ctypes.POINTER(wt.BYTE)),
                ("modBaseSize", wt.DWORD), ("hModule", wt.HMODULE),
                ("szModule", wt.WCHAR * 256), ("szExePath", wt.WCHAR * 260)]

class MEMORY_BASIC_INFORMATION(ctypes.Structure):
    _fields_ = [("BaseAddress", wt.LPVOID), ("AllocationBase", wt.LPVOID),
                ("AllocationProtect", wt.DWORD), ("RegionSize", ctypes.c_size_t),
                ("State", wt.DWORD), ("Protect", wt.DWORD), ("Type", wt.DWORD)]

kernel32.CreateToolhelp32Snapshot.restype = wt.HANDLE
kernel32.Module32FirstW.argtypes = [wt.HANDLE, ctypes.POINTER(MODULEENTRY32W)]
kernel32.Module32NextW.argtypes = [wt.HANDLE, ctypes.POINTER(MODULEENTRY32W)]
kernel32.OpenProcess.restype = wt.HANDLE
kernel32.ReadProcessMemory.restype = wt.BOOL
kernel32.VirtualQueryEx.restype = ctypes.c_size_t
kernel32.CloseHandle.argtypes = [wt.HANDLE]

# PAGE_* low byte values accepted for a read snapshot.
READABLE = {0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80}
EXECUTABLE = {0x10, 0x20, 0x40, 0x80}
MEM_COMMIT = 0x1000

def find_pid(name: str) -> int | None:
    out = subprocess.run(["powershell", "-NoProfile", "-Command",
                          f"(Get-Process -Name '{name}' -ErrorAction SilentlyContinue | Select-Object -First 1 -Expand Id)"],
                         capture_output=True, text=True).stdout.strip()
    return int(out) if out.isdigit() else None

def modules(pid: int):
    h = kernel32.CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid)
    if h == wt.HANDLE(-1).value:
        raise OSError(ctypes.get_last_error(), "CreateToolhelp32Snapshot")
    try:
        me = MODULEENTRY32W(); me.dwSize = ctypes.sizeof(me)
        rows = []
        ok = kernel32.Module32FirstW(h, ctypes.byref(me))
        while ok:
            n = me.szModule.lower()
            if n in LIST_MODULES:
                rows.append({"name": n, "base": ctypes.addressof(me.modBaseAddr.contents), "size": me.modBaseSize, "path": me.szExePath})
            ok = kernel32.Module32NextW(h, ctypes.byref(me))
        # WOW64 snapshots can report the same module more than once; keep the
        # largest image range for each case-insensitive module name.
        unique = {}
        for row in rows:
            old = unique.get(row["name"])
            if old is None or row["size"] > old["size"]:
                unique[row["name"]] = row
        return list(unique.values())
    finally:
        kernel32.CloseHandle(h)

def read_region(hp, addr: int, size: int) -> bytes:
    buf = ctypes.create_string_buffer(size); got = ctypes.c_size_t()
    if not kernel32.ReadProcessMemory(hp, ctypes.c_void_p(addr), buf, size, ctypes.byref(got)):
        return b""
    return buf.raw[:got.value]

def snapshot(hp, mods, exec_only=False):
    result = {}
    mbi = MEMORY_BASIC_INFORMATION
    for m in mods:
        cur, end = m["base"], m["base"] + m["size"]
        while cur < end:
            q = mbi(); n = kernel32.VirtualQueryEx(hp, ctypes.c_void_p(cur), ctypes.byref(q), ctypes.sizeof(q))
            if not n or not q.RegionSize: break
            rb = int(q.BaseAddress); re = rb + int(q.RegionSize)
            if q.State == MEM_COMMIT and (q.Protect & 0xff) in (EXECUTABLE if exec_only else READABLE):
                a, b = max(rb, m["base"]), min(re, end)
                if b > a:
                    data = read_region(hp, a, b-a)
                    if data: result[(m["name"], a-m["base"])] = data
            cur = re
    return result

def diff(before, after, mods):
    by_name = {m["name"]: m for m in mods}; changes=[]
    for key, old in before.items():
        new = after.get(key)
        if new is None: continue
        n = min(len(old), len(new)); i=0
        while i < n:
            if old[i] == new[i]: i += 1; continue
            start=i
            while i<n and old[i]!=new[i]: i+=1
            # include small unchanged gaps inside a patch cluster
            while i<n and i-start < 256 and old[i:i+4] != new[i:i+4]: i+=1
            changes.append({"module": key[0], "rva": hex(key[1]+start),
                            "length": i-start, "old": old[start:i].hex(), "new": new[start:i].hex()})
    return changes

def main():
    ap=argparse.ArgumentParser(); ap.add_argument("--pid", type=int); ap.add_argument("--wait", action="store_true"); ap.add_argument("--out", type=Path, default=Path("runtime-diffs")); ns=ap.parse_args()
    pid=ns.pid or find_pid("crossfire")
    if not pid and ns.wait:
        print("等待 crossfire.exe ...", flush=True)
        while not pid: time.sleep(1); pid=find_pid("crossfire")
    if not pid: raise SystemExit("未找到 crossfire.exe；先启动客户端，或指定 --pid")
    hp=kernel32.OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ, False, pid)
    if not hp: raise OSError(ctypes.get_last_error(), f"OpenProcess({pid})")
    try:
        mods=modules(pid); names={m["name"] for m in mods}
        print(f"PID={pid}; modules={', '.join(sorted(names))}")
        missing=set(LIST_MODULES)-names
        if missing: print("未加载:", ", ".join(sorted(missing)))
        if "crossfire.exe" not in names:
            raise SystemExit("指定 PID 不是已加载目标模块的 crossfire.exe；PowerShell 中不要使用 $PID（它是只读自动变量），请改用 --wait 或 $cfPid")
        print("回车建立基线；随后只切换一个功能，再回车采集差分。")
        input(); before=snapshot(hp,mods); print(f"基线页数: {len(before)}")
        input("完成一次功能切换后按回车: "); after=snapshot(hp,mods)
        changes=diff(before,after,mods); ns.out.mkdir(parents=True,exist_ok=True)
        out=ns.out/f"diff_{pid}_{int(time.time())}.json"; out.write_text(json.dumps({"pid":pid,"modules":mods,"changes":changes},ensure_ascii=False,indent=2),encoding="utf-8")
        print(f"变化段: {len(changes)}; 输出: {out.resolve()}")
        for c in changes[:80]: print(c["module"],c["rva"],c["old"],"=>",c["new"])
    finally: kernel32.CloseHandle(hp)

if __name__ == "__main__": main()


