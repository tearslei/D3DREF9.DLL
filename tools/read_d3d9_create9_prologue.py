#!/usr/bin/env python3
"""Read the live d3d9!Direct3DCreate9 prologue; no process modification."""
import ctypes, json, os, struct
from pathlib import Path
import runtime_hotkey_batch as b
import runtime_patch_capture as r

pid = b.find_pid()
if not pid: raise SystemExit('crossfire.exe not found')
r.LIST_MODULES = tuple(set(r.LIST_MODULES + ('d3d9.dll',)))
mod = next((m for m in r.modules(pid) if m['name'] == 'd3d9.dll'), None)
if not mod: raise SystemExit('d3d9.dll not loaded')
data = Path(os.environ['WINDIR']) / 'SysWOW64' / 'd3d9.dll'
raw_pe = data.read_bytes()
pe = struct.unpack_from('<I', raw_pe, 0x3C)[0]
opt = pe + 24
assert struct.unpack_from('<H', raw_pe, opt)[0] == 0x10B
dd = opt + 96
exp_rva = struct.unpack_from('<I', raw_pe, dd)[0]
sections = []
section_count = struct.unpack_from('<H', raw_pe, pe + 6)[0]
so = opt + struct.unpack_from('<H', raw_pe, pe + 20)[0]
for i in range(section_count):
    q = so + i * 40
    vs, va, rs, rp = struct.unpack_from('<IIII', raw_pe, q + 8)
    sections.append((va, max(vs, rs), rp))
def off(rva):
    for va, size, ptr in sections:
        if va <= rva < va + size: return ptr + rva - va
    raise ValueError(hex(rva))
eo = off(exp_rva)
base, nf, nn, af, an, ao = struct.unpack_from('<IIIIII', raw_pe, eo + 16)
rva = None
for i in range(nn):
    nr = struct.unpack_from('<I', raw_pe, off(an) + i * 4)[0]
    name_off = off(nr); end = raw_pe.index(b'\0', name_off)
    if raw_pe[name_off:end] == b'Direct3DCreate9':
        ordinal = struct.unpack_from('<H', raw_pe, off(ao) + i * 2)[0]
        rva = struct.unpack_from('<I', raw_pe, off(af) + ordinal * 4)[0]
        break
if rva is None: raise SystemExit('Direct3DCreate9 export missing')
hp = b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION | b.PROCESS_VM_READ, False, pid)
if not hp: raise OSError(ctypes.get_last_error(), 'OpenProcess')
try:
    address = mod['base'] + rva
    raw = r.read_region(hp, address, 16)
    out = {'pid': pid, 'd3d9_base': f'0x{mod["base"]:08X}', 'Direct3DCreate9_rva': f'0x{rva:X}', 'remote_address': f'0x{address:08X}', 'first_16_bytes': raw.hex()}
    p = Path('runtime-diffs/match-baseline/d3d9-create9-prologue.json'); p.write_text(json.dumps(out, indent=2), encoding='utf-8')
    print(json.dumps(out))
finally:
    b.k32.CloseHandle(hp)
