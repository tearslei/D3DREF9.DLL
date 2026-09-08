#!/usr/bin/env python3
"""Cross-reference TCII source offset strings with a live CF process.

This is a read-only triage helper.  It does not write memory, inject code, or
send input.  Source .e/.ec files are binary EasyLanguage containers; the
offsets are extracted from nearby ASCII strings and checked against the
currently loaded cshell.dll image.
"""
from __future__ import annotations
import argparse, ctypes, json, re, struct, math
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import runtime_hotkey_batch as hotkeys
import runtime_patch_capture as mem

FEATURES = (
    "透视", "自瞄", "不掉血", "摔不掉血", "无后坐力", "零秒换弹",
    "子弹穿墙", "人物穿墙", "第三人称", "无限背包", "刷无线电",
    "矩阵", "骨骼", "自动开枪", "空格连跳", "瞬移通地",
)

def ascii_strings(blob: bytes):
    for m in re.finditer(rb"[ -~]{4,}", blob):
        yield m.start(), m.group().decode("ascii", "ignore")

def collect_source_offsets(root: Path):
    rows = []
    for p in sorted(root.rglob("*.e")) + sorted(root.rglob("*.ec")):
        try:
            blob = p.read_bytes()
        except OSError:
            continue
        # Offsets in these containers are grouped immediately after a
        # ``cshell.dll`` marker.  Restrict extraction to that window; scanning
        # the whole container would mistake ordinary decimal text (for
        # example ``12345``) for an RVA.
        markers = [m.start() for m in re.finditer(rb"cshell\.dll", blob, re.I)]
        for marker in markers:
            window_start, window_end = marker, min(len(blob), marker + 0x900)
            window = blob[window_start:window_end]
            strings = list(ascii_strings(window))
            for rel, s in strings:
                if not re.fullmatch(r"[0-9A-Fa-f]{5,8}", s):
                    continue
                value = int(s, 16)
                if not (0x10000 <= value < 0x4000000):
                    continue
                nearby = " ".join(t for q, t in strings if abs(q-rel) <= 512)
                feats = sorted({f for f in FEATURES if f in nearby})
                rows.append({"file": str(p), "offset_text": s.upper(),
                             "rva": value, "source_pos": marker + rel,
                             "nearby_features": feats})
    # Deduplicate exact file/RVA entries while retaining feature unions.
    merged = {}
    for r in rows:
        k = (r["file"], r["rva"])
        if k not in merged:
            merged[k] = r
        else:
            merged[k]["nearby_features"] = sorted(set(merged[k]["nearby_features"]) | set(r["nearby_features"]))
    return list(merged.values())

def classify(raw: bytes, module_base: int, module_size: int):
    if len(raw) < 16:
        return {"readable": False}
    words = struct.unpack("<4I", raw[:16])
    floats = struct.unpack("<4f", raw[:16])
    ptrs = [f"0x{x:08X}" for x in words if module_base <= x < module_base + module_size]
    finite = all(math.isfinite(x) and abs(x) < 1e6 for x in floats)
    return {
        "readable": True,
        "bytes": raw.hex(),
        "u32": [f"0x{x:08X}" for x in words],
        "f32": [round(x, 6) if math.isfinite(x) else None for x in floats],
        "internal_ptrs": ptrs,
        "float_like": finite,
    }

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", type=Path, default=Path(r"E:\game\辅助\2.0 TCII全套开源"))
    ap.add_argument("--pid", type=int, default=0)
    ap.add_argument("--out", type=Path, default=Path("runtime-diffs/match-baseline/tcii-runtime-offsets.json"))
    ns = ap.parse_args()
    pid = ns.pid or hotkeys.find_pid()
    if not pid:
        raise SystemExit("crossfire.exe not found")
    hp = hotkeys.k32.OpenProcess(hotkeys.PROCESS_QUERY_INFORMATION | hotkeys.PROCESS_VM_READ, False, pid)
    if not hp:
        raise OSError(ctypes.get_last_error(), "OpenProcess failed")
    try:
        mods = {m["name"].lower(): m for m in mem.modules(pid)}
        cs = mods.get("cshell.dll")
        if not cs:
            raise SystemExit("cshell.dll not loaded")
        src = collect_source_offsets(ns.source)
        out = []
        for row in src:
            raw = mem.read_region(hp, cs["base"] + row["rva"], 16)
            q = dict(row)
            q["address"] = f"0x{cs['base'] + row['rva']:08X}"
            q["runtime"] = classify(raw, cs["base"], cs["size"])
            out.append(q)
        result = {
            "pid": pid,
            "process": "crossfire.exe",
            "module": {"name": "cshell.dll", "base": f"0x{cs['base']:08X}", "size": cs["size"]},
            "source_root": str(ns.source),
            "read_only": True,
            "entries": out,
            "notes": [
                "RVA 文本来自 EasyLanguage 容器，不能单独证明语义。",
                "internal_ptrs 仅表示指向 cshell.dll 映像内的数值；代码 RVA 与数据 RVA 需进一步区分。",
                "需通过重复运行、生命周期和可观察结果验证后，才能写入处理器适配表。",
            ],
        }
        ns.out.parent.mkdir(parents=True, exist_ok=True)
        ns.out.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
        print(json.dumps({"pid": pid, "entries": len(out), "output": str(ns.out.resolve())}, ensure_ascii=False))
        for q in out[:60]:
            print(f"{q['offset_text']} {q['address']} features={','.join(q['nearby_features']) or '-'} bytes={q['runtime'].get('bytes','')}")
    finally:
        hotkeys.k32.CloseHandle(hp)

if __name__ == "__main__":
    main()
