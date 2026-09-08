#!/usr/bin/env python3
"""Small dependency-free PE export verifier for the generated temporary proxy."""
import argparse, struct
from pathlib import Path

ap = argparse.ArgumentParser(); ap.add_argument('path', type=Path); ns = ap.parse_args()
b = ns.path.read_bytes(); pe = struct.unpack_from('<I', b, 0x3C)[0]; opt = pe + 24
magic = struct.unpack_from('<H', b, opt)[0]; dd = opt + (96 if magic == 0x10B else 112)
er, _ = struct.unpack_from('<II', b, dd); nsec = struct.unpack_from('<H', b, pe + 6)[0]; so = opt + struct.unpack_from('<H', b, pe + 20)[0]
secs = [struct.unpack_from('<IIII', b, so + i * 40 + 8) for i in range(nsec)]
def off(r):
    for vs, va, rs, rp in secs:
        if va <= r < va + max(vs, rs): return rp + r - va
    raise ValueError(f'RVA not mapped: 0x{r:X}')
o = off(er); base, count, names, funcs, name_rvas, ordinals = struct.unpack_from('<IIIIII', b, o + 16)
print(f'Machine=0x{struct.unpack_from("<H", b, pe+4)[0]:04X} Magic=0x{magic:04X} ExportBase={base} Functions={count} Names={names}')
for i in range(count): print(f'ordinal {base+i}: RVA 0x{struct.unpack_from("<I", b, off(funcs)+4*i)[0]:X}')
