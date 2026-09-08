#!/usr/bin/env python3
"""Find changing world-position-like float triples and nearby record layouts.

The probe is read-only. It samples the live process three times, retains
float triples in map-coordinate ranges, and reports addresses that change in
concert with the known local-player position region.
"""
from __future__ import annotations
import ctypes, json, math, struct, sys, time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import runtime_hotkey_batch as b
import runtime_patch_capture as r

pid = b.find_pid()
if not pid:
    raise SystemExit("crossfire not found")
hp = b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION | b.PROCESS_VM_READ, False, pid)
mods = [m for m in r.modules(pid) if m["name"] in ("cshell.dll", "crossfire.exe")]

snaps = []
for i in range(3):
    snaps.append(r.snapshot(hp, mods))
    if i < 2:
        time.sleep(0.4)

def triples(data: bytes):
    out = []
    # 4-byte alignment is sufficient for native float fields.
    for off in range(0, len(data) - 12, 4):
        try: v = struct.unpack_from("<3f", data, off)
        except struct.error: continue
        if not all(math.isfinite(x) and abs(x) < 20000 for x in v):
            continue
        # Map coordinates tend to have at least one large component while
        # avoiding obvious colors/timers and denormals.
        if max(abs(x) for x in v) < 100:
            continue
        out.append((off, v))
    return out

rows = []
for key, d0 in snaps[0].items():
    d1, d2 = snaps[1].get(key, b""), snaps[2].get(key, b"")
    if not d1 or not d2:
        continue
    t0 = triples(d0); idx1 = {o: v for o, v in triples(d1)}; idx2 = {o: v for o, v in triples(d2)}
    for off, v0 in t0:
        v1, v2 = idx1.get(off), idx2.get(off)
        if v1 is None or v2 is None:
            continue
        delta = [max(abs(v1[i] - v0[i]), abs(v2[i] - v1[i])) for i in range(3)]
        if max(delta) < 0.5:
            continue
        # Require a smooth change or a stable pair of coordinates. This
        # removes most random animation values.
        smooth = sum(abs((v2[i] - v1[i]) - (v1[i] - v0[i])) < 500 for i in range(3)) >= 2
        if not smooth:
            continue
        rows.append({"module": key[0], "rva": hex(key[1] + off),
                     "v0": [round(x, 3) for x in v0],
                     "v1": [round(x, 3) for x in v1],
                     "v2": [round(x, 3) for x in v2],
                     "delta": [round(x, 3) for x in delta]})

# Keep only windows with multiple nearby candidates; repeated records usually
# produce several vec3 fields within a few hundred bytes.
windows = {}
for row in rows:
    w = int(row["rva"], 16) & ~0x3ff
    windows.setdefault((row["module"], w), []).append(row)
groups = [{"module": k[0], "window": hex(k[1]), "count": len(v), "items": v[:128]}
          for k, v in sorted(windows.items(), key=lambda kv: -len(kv[1]))]

out = Path("runtime-diffs/match-baseline/position-clusters.json")
out.parent.mkdir(parents=True, exist_ok=True)
out.write_text(json.dumps({"pid": pid, "rows": len(rows), "groups": groups}, ensure_ascii=False, indent=2), encoding="utf-8")
print(f"pid={pid} rows={len(rows)} groups={len(groups)} output={out.resolve()}")
for g in groups[:50]:
    print(g["module"], g["window"], "count", g["count"], flush=True)
b.k32.CloseHandle(hp)
