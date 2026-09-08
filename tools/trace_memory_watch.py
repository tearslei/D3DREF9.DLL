#!/usr/bin/env python3
"""Capture x86 read/write hardware-watchpoint evidence from a live CF PID.

This is the non-GUI fallback for the bundled x32dbg workflow.  It uses the
same Windows debug-register mechanism as x32dbg, but explicitly continues
EXCEPTION_BREAKPOINT from the client's nvppe module so an anti-debug INT3 does
not terminate the debugger.  Target memory is read-only; no process bytes are
patched.
"""
from __future__ import annotations

import argparse
import ctypes
import ctypes.wintypes as wt
import json
import struct
import sys
import time
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import runtime_patch_capture as rpc  # type: ignore
import runtime_hotkey_batch as hotkeys  # type: ignore

k = ctypes.WinDLL("kernel32", use_last_error=True)

DBG_CONTINUE = 0x00010002
EXCEPTION_DEBUG_EVENT = 1
CREATE_THREAD_DEBUG_EVENT = 2
CREATE_PROCESS_DEBUG_EVENT = 3
EXIT_THREAD_DEBUG_EVENT = 4
EXIT_PROCESS_DEBUG_EVENT = 5
EXCEPTION_BREAKPOINT = 0x80000003
EXCEPTION_SINGLE_STEP = 0x80000004

THREAD_SET_CONTEXT = 0x0010
THREAD_GET_CONTEXT = 0x0008
THREAD_QUERY_INFORMATION = 0x0040
CONTEXT_DEBUG = 0x00010010


class WOW64_FLOAT(ctypes.Structure):
    _fields_ = [
        ("ControlWord", wt.DWORD), ("StatusWord", wt.DWORD),
        ("TagWord", wt.DWORD), ("ErrorOffset", wt.DWORD),
        ("ErrorSelector", wt.DWORD), ("DataOffset", wt.DWORD),
        ("DataSelector", wt.DWORD), ("RegisterArea", wt.BYTE * 80),
        ("Cr0NpxState", wt.DWORD),
    ]


class WOW64_CONTEXT(ctypes.Structure):
    _fields_ = [
        ("ContextFlags", wt.DWORD), ("Dr0", wt.DWORD), ("Dr1", wt.DWORD),
        ("Dr2", wt.DWORD), ("Dr3", wt.DWORD), ("Dr6", wt.DWORD),
        ("Dr7", wt.DWORD), ("FloatSave", WOW64_FLOAT),
        ("SegGs", wt.DWORD), ("SegFs", wt.DWORD), ("SegEs", wt.DWORD),
        ("SegDs", wt.DWORD), ("Edi", wt.DWORD), ("Esi", wt.DWORD),
        ("Ebx", wt.DWORD), ("Edx", wt.DWORD), ("Ecx", wt.DWORD),
        ("Eax", wt.DWORD), ("Ebp", wt.DWORD), ("Eip", wt.DWORD),
        ("SegCs", wt.DWORD), ("EFlags", wt.DWORD), ("Esp", wt.DWORD),
        ("SegSs", wt.DWORD), ("ExtendedRegisters", wt.BYTE * 512),
    ]


k.OpenThread.argtypes = [wt.DWORD, wt.BOOL, wt.DWORD]
k.OpenThread.restype = wt.HANDLE
k.CloseHandle.argtypes = [wt.HANDLE]
k.Wow64GetThreadContext.argtypes = [wt.HANDLE, ctypes.POINTER(WOW64_CONTEXT)]
k.Wow64GetThreadContext.restype = wt.BOOL
k.Wow64SetThreadContext.argtypes = [wt.HANDLE, ctypes.POINTER(WOW64_CONTEXT)]
k.Wow64SetThreadContext.restype = wt.BOOL
k.DebugActiveProcess.argtypes = [wt.DWORD]
k.DebugActiveProcess.restype = wt.BOOL
k.DebugActiveProcessStop.argtypes = [wt.DWORD]
k.DebugActiveProcessStop.restype = wt.BOOL
k.WaitForDebugEvent.argtypes = [ctypes.c_void_p, wt.DWORD]
k.WaitForDebugEvent.restype = wt.BOOL
k.ContinueDebugEvent.argtypes = [wt.DWORD, wt.DWORD, wt.DWORD]
k.ContinueDebugEvent.restype = wt.BOOL
k.CreateToolhelp32Snapshot.argtypes = [wt.DWORD, wt.DWORD]
k.CreateToolhelp32Snapshot.restype = wt.HANDLE
k.Thread32First.argtypes = [wt.HANDLE, ctypes.c_void_p]
k.Thread32First.restype = wt.BOOL
k.Thread32Next.argtypes = [wt.HANDLE, ctypes.c_void_p]
k.Thread32Next.restype = wt.BOOL

TH32CS_SNAPTHREAD = 0x00000004


class THREADENTRY32(ctypes.Structure):
    _fields_ = [
        ("dwSize", wt.DWORD), ("cntUsage", wt.DWORD),
        ("th32ThreadID", wt.DWORD), ("th32OwnerProcessID", wt.DWORD),
        ("tpBasePri", wt.LONG), ("tpDeltaPri", wt.LONG),
        ("dwFlags", wt.DWORD),
    ]


def thread_ids(pid: int) -> list[int]:
    h = k.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)
    if h in (0, wt.HANDLE(-1).value):
        return []
    out: list[int] = []
    try:
        e = THREADENTRY32()
        e.dwSize = ctypes.sizeof(e)
        if k.Thread32First(h, ctypes.byref(e)):
            while True:
                if e.th32OwnerProcessID == pid:
                    out.append(int(e.th32ThreadID))
                if not k.Thread32Next(h, ctypes.byref(e)):
                    break
    finally:
        k.CloseHandle(h)
    return out


def set_watchpoints(tid: int, addresses: list[int]) -> bool:
    h = k.OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION, False, tid)
    if not h:
        return False
    try:
        c = WOW64_CONTEXT()
        c.ContextFlags = CONTEXT_DEBUG
        if not k.Wow64GetThreadContext(h, ctypes.byref(c)):
            return False
        regs = ["Dr0", "Dr1", "Dr2", "Dr3"]
        for i, reg in enumerate(regs):
            setattr(c, reg, int(addresses[i]) if i < len(addresses) else 0)
        c.Dr6 = 0
        # DR7 local-enable bits are 0,2,4,6 (one per slot).  RW=read/write
        # (3), LEN=4 bytes (3) occupies four bits at 16+4*slot.  Do not set
        # the reserved/global bits: malformed DR7 values make some protected
        # clients terminate immediately after DebugActiveProcess.
        enable = 0
        control = 0
        for i in range(min(4, len(addresses))):
            enable |= 1 << (i * 2)
            control |= 0xF << (16 + i * 4)
        c.Dr7 = (int(c.Dr7) & ~(0xFF | 0xFFFF0000)) | enable | control
        return bool(k.Wow64SetThreadContext(h, ctypes.byref(c)))
    finally:
        k.CloseHandle(h)


def read_u32(hp, address: int) -> int:
    raw = rpc.read_region(hp, address, 4)
    return struct.unpack("<I", raw)[0] if len(raw) == 4 else 0


def resolve_targets(pid: int, hp) -> tuple[dict, list[int]]:
    mods = rpc.modules(pid)
    shell = next((m for m in mods if m["name"] == "cshell.dll"), None)
    cf = next((m for m in mods if m["name"] == "crossfire.exe"), None)
    if not shell or not cf:
        return ({"modules": mods}, [])
    sb = int(shell["base"])
    players = read_u32(hp, sb + 0x166AD00)
    person = read_u32(hp, players + 0x30) if players else 0
    info = {
        "cshell_base": f"0x{sb:08X}",
        "crossfire_base": f"0x{int(cf['base']):08X}",
        "players": f"0x{players:08X}" if players else "0x00000000",
        "person": f"0x{person:08X}" if person else "0x00000000",
        "person_pointer_address": f"0x{players + 0x30:08X}" if players else "0x00000000",
    }
    # Three transform fields (X/Z/Y) plus the new-mode alive/HP candidate.
    targets = [person + off for off in (0x6C, 0x70, 0x74)] if person else []
    if person:
        targets.append(person + 0x528)
    return info, targets


def decode_context(c: WOW64_CONTEXT) -> dict:
    return {
        "eax": f"0x{int(c.Eax):08X}", "ebx": f"0x{int(c.Ebx):08X}",
        "ecx": f"0x{int(c.Ecx):08X}", "edx": f"0x{int(c.Edx):08X}",
        "esi": f"0x{int(c.Esi):08X}", "edi": f"0x{int(c.Edi):08X}",
        "ebp": f"0x{int(c.Ebp):08X}", "esp": f"0x{int(c.Esp):08X}",
        "eip": f"0x{int(c.Eip):08X}", "eflags": f"0x{int(c.EFlags):08X}",
        "dr6": f"0x{int(c.Dr6):08X}", "dr7": f"0x{int(c.Dr7):08X}",
    }


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--pid", type=int, required=True)
    ap.add_argument("--seconds", type=float, default=180)
    ap.add_argument("--out", type=Path, default=Path("runtime-diffs/match-baseline/coordinate-access-trace.json"))
    ns = ap.parse_args()

    hp = hotkeys.k32.OpenProcess(hotkeys.PROCESS_QUERY_INFORMATION | hotkeys.PROCESS_VM_READ, False, ns.pid)
    if not hp:
        raise OSError(ctypes.get_last_error(), f"OpenProcess({ns.pid})")
    initial_info, addresses = resolve_targets(ns.pid, hp)
    events: list[dict] = []
    configured: set[int] = set()
    debug_attached = False
    start = time.time()
    last_refresh = 0.0
    current_info = initial_info
    try:
        if not k.DebugActiveProcess(ns.pid):
            raise OSError(ctypes.get_last_error(), f"DebugActiveProcess({ns.pid})")
        debug_attached = True
        for tid in thread_ids(ns.pid):
            if set_watchpoints(tid, addresses):
                configured.add(tid)
        evbuf = ctypes.create_string_buffer(176)
        while time.time() - start < ns.seconds and len(events) < 20000:
            if time.time() - last_refresh >= 1.0:
                try:
                    current_info, fresh = resolve_targets(ns.pid, hp)
                    if fresh and fresh != addresses:
                        addresses = fresh
                        for tid in thread_ids(ns.pid):
                            if set_watchpoints(tid, addresses):
                                configured.add(tid)
                    last_refresh = time.time()
                except OSError:
                    pass
            if not k.WaitForDebugEvent(evbuf, 250):
                continue
            raw = evbuf.raw
            code = int.from_bytes(raw[0:4], "little")
            event_pid = int.from_bytes(raw[4:8], "little")
            tid = int.from_bytes(raw[8:12], "little")
            if code == EXCEPTION_DEBUG_EVENT:
                exc = int.from_bytes(raw[12:16], "little")
                if exc in (EXCEPTION_SINGLE_STEP, EXCEPTION_BREAKPOINT):
                    h = k.OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_QUERY_INFORMATION, False, tid)
                    if h:
                        try:
                            c = WOW64_CONTEXT(); c.ContextFlags = CONTEXT_DEBUG
                            if k.Wow64GetThreadContext(h, ctypes.byref(c)):
                                dr6 = int(c.Dr6)
                                if exc == EXCEPTION_SINGLE_STEP:
                                    hit = [i for i in range(4) if dr6 & (1 << i)]
                                    events.append({
                                        "t": time.time(), "exception": "single_step",
                                        "thread": tid, "hit_slots": hit,
                                        "watched_addresses": [f"0x{x:08X}" for x in addresses],
                                        "context": decode_context(c),
                                    })
                                else:
                                    # Preserve the INT3 as evidence, but pass it
                                    # through so nvppe's anti-debug path cannot
                                    # crash this tracer.
                                    events.append({
                                        "t": time.time(), "exception": "breakpoint",
                                        "thread": tid, "module_context": current_info,
                                        "context": decode_context(c),
                                    })
                                c.Dr6 = 0
                                k.Wow64SetThreadContext(h, ctypes.byref(c))
                        finally:
                            k.CloseHandle(h)
                elif exc not in (0x4000001F,):
                    events.append({"t": time.time(), "exception": f"0x{exc:08X}", "thread": tid})
            elif code == CREATE_THREAD_DEBUG_EVENT:
                if set_watchpoints(tid, addresses):
                    configured.add(tid)
            elif code == EXIT_PROCESS_DEBUG_EVENT:
                break
            k.ContinueDebugEvent(event_pid, tid, DBG_CONTINUE)
    finally:
        if debug_attached:
            k.DebugActiveProcessStop(ns.pid)
        hotkeys.k32.CloseHandle(hp)
    result = {
        "pid": ns.pid,
        "seconds": time.time() - start,
        "initial_targets": initial_info,
        "final_targets": current_info,
        "watched_addresses": [f"0x{x:08X}" for x in addresses],
        "configured_threads": sorted(configured),
        "event_count": len(events),
        "events": events,
        "read_only_target_memory": True,
        "debug_register_method": "WOW64_CONTEXT DR0-DR3 RW LEN4",
    }
    ns.out.parent.mkdir(parents=True, exist_ok=True)
    ns.out.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"pid": ns.pid, "seconds": result["seconds"], "events": len(events), "output": str(ns.out.resolve())}, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
