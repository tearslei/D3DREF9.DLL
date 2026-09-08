#!/usr/bin/env python3
"""Extract source-declared client bases/offsets into a reviewable catalog.

The EasyLanguage project files are not a C++ symbol database.  This helper
only extracts declarations that are explicitly written in the exported text
(``cshell.dll + 十六到十`` and named decimal constants); it never modifies a
process or treats an offset as valid without a runtime signature check.
"""
from __future__ import annotations
import argparse, json, re
from pathlib import Path

HEX = re.compile(r"动态链接库地址\s*\+\s*十六到十\s*\(\s*[\"“]([0-9A-Fa-f]+)[\"”]\s*\)")
DECL = re.compile(r"^\s*([\w\u4e00-\u9fff]+)\s*=\s*([0-9]{5,9})\b")
INTEREST = ("地址", "基址", "偏移", "数组", "内存", "对象", "OBJECT", "FOV", "HOOK", "鼠标", "本人")

def scan(path: Path):
    rows = []
    try:
        lines = path.read_text(encoding="utf-8", errors="ignore").splitlines()
    except OSError:
        return rows
    for no, line in enumerate(lines, 1):
        mh = HEX.search(line)
        if mh:
            value = int(mh.group(1), 16)
            left = line.split("=", 1)[0].strip()
            name = left.split()[-1] if left else "unknown"
            rows.append({"name": name, "value": value, "format": "hex_rva",
                         "module": "cshell.dll", "source": str(path), "line": no,
                         "text": line.strip()})
            continue
        md = DECL.search(line)
        if md and any(k in md.group(1) for k in INTEREST):
            rows.append({"name": md.group(1), "value": int(md.group(2)),
                         "format": "decimal_constant", "module": "unknown",
                         "source": str(path), "line": no, "text": line.strip()})
    return rows

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--source", type=Path, default=Path(r"E:\game\辅助\2.0 TCII全套开源"))
    ap.add_argument("--dump", type=Path, default=Path(__file__).resolve().parents[1] / "source-dumps")
    ap.add_argument("--out", type=Path, default=Path(__file__).resolve().parents[1] / "config" / "source-base-catalog.json")
    ns = ap.parse_args()
    files = list(ns.dump.glob("*.txt")) if ns.dump.exists() else []
    if not files and ns.source.exists():
        files = list(ns.source.glob("*.e")) + list(ns.source.glob("*.ec"))
    rows = []
    for p in sorted(files): rows.extend(scan(p))
    # Keep the first declaration for a name/value pair and retain provenance.
    uniq = {}
    for r in rows: uniq.setdefault((r["name"], r["value"], r["module"]), r)
    result = {"source_root": str(ns.source), "read_only": True,
              "entries": sorted(uniq.values(), key=lambda x: (x["module"], x["value"], x["name"])),
              "notes": ["Values are source declarations only; validate module signature and lifecycle before use."]}
    ns.out.parent.mkdir(parents=True, exist_ok=True)
    ns.out.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"files": len(files), "entries": len(result["entries"]), "output": str(ns.out.resolve())}, ensure_ascii=False))

if __name__ == "__main__": main()
