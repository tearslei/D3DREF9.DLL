#!/usr/bin/env python3
"""Scan cshell/crossfire readable memory for changing vec3/vec4 values.

This is observational only: it snapshots committed readable pages and reports
addresses whose consecutive float triples change between samples.  Results are
grouped by nearby offsets so repeated entity records can be reviewed.
"""
from __future__ import annotations
import json, math, struct, sys, time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import runtime_hotkey_batch as b
import runtime_patch_capture as r

pid = b.find_pid()
if not pid:
    raise SystemExit("crossfire not found")
hp = b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION | b.PROCESS_VM_READ, False, pid)
mods = [m for m in r.modules(pid) if m["name"] in ("cshell.dll", "crossfire.exe")]
if not mods:
    raise SystemExit("target modules not loaded")

samples = []
for i in range(3):
    samples.append(r.snapshot(hp, mods))
    if i != 2:
        time.sleep(0.35)

hits = []
for key, a in samples[0].items():
    b1, c = samples[1].get(key, b""), samples[2].get(key, b"")
    n = min(len(a), len(b1), len(c)) - 16
    if n <= 0:
        continue
    for off in range(0, n, 4):
        try:
            v0 = struct.unpack_from("<4f", a, off)
            v1 = struct.unpack_from("<4f", b1, off)
            v2 = struct.unpack_from("<4f", c, off)
        except struct.error:
            continue
        if not all(math.isfinite(x) and abs(x) < 100000 for x in v0 + v1 + v2):
            continue
        # Require monotonic movement in at least two components; this filters
        # most static constants while retaining player/camera transforms.
        delta = [max(abs(v1[i]-v0[i]), abs(v2[i]-v1[i])) for i in range(4)]
        if sum(d > 0.01 for d in delta[:3]) == 0:
            continue
        if max(delta[:3]) < 0.05:
            continue
        hits.append({"module": key[0], "rva": hex(key[1]+off),
                     "v0": [round(x,3) for x in v0],
                     "v1": [round(x,3) for x in v1],
                     "v2": [round(x,3) for x in v2],
                     "delta": [round(x,3) for x in delta]})

# Group by 0x100-byte windows, a useful first pass for repeated records.
groups = {}
for h in hits:
    base = int(h["rva"], 16) & ~0xff
    groups.setdefault((h["module"], base), []).append(h)
groups_out = [{"module": k[0], "window": hex(k[1]), "count": len(v),
               "items": v[:32]} for k, v in sorted(groups.items(), key=lambda kv: -len(kv[1]))]

out = Path("runtime-diffs/match-baseline/dynamic-vectors.json")
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps({"pid": pid, "samples": 3, "hits": len(hits),
                           "groups": groups_out}, ensure_ascii=False, indent=2), encoding="utf-8")
print(f"pid={pid} hits={len(hits)} groups={len(groups_out)} output={out.resolve()}")
for g in groups_out[:40]:
    print(g["module"], g["window"], "count", g["count"], flush=True)
b.k32.CloseHandle(hp)
