#!/usr/bin/env python3
"""Stage-1 read-only capture for a live CrossFire match.

The sampler waits for ``crossfire.exe`` instead of requiring the operator to
start it first.  It never sends input and never writes target-process memory.
It refreshes module bases after launch and records a compact candidate timeline
plus a Gate-A quality report at the end.
"""
from __future__ import annotations
import argparse, csv, json, shutil, struct, time
from pathlib import Path
import sys

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import runtime_hotkey_batch as hotkeys
import runtime_patch_capture as runtime


def module_map(pid: int) -> dict:
    try:
        return {m["name"].lower(): m for m in runtime.modules(pid)}
    except Exception:
        return {}


def open_read(pid: int):
    return hotkeys.k32.OpenProcess(
        hotkeys.PROCESS_QUERY_INFORMATION | hotkeys.PROCESS_VM_READ, False, pid
    )


def gate_report(entity_csv: Path, timeline_csv: Path, pid: int, started: float, ended: float) -> dict:
    rows = []
    if entity_csv.exists():
        try:
            with entity_csv.open(newline="", encoding="utf-8", errors="replace") as f:
                rows = list(csv.DictReader(f))
        except OSError:
            rows = []
    def truth(name: str, row: dict) -> bool:
        try:
            return float(row.get(name, "0") or 0) != 0
        except (TypeError, ValueError):
            return False
    nonlocal_rows = [r for r in rows if (r.get("entity_addr") or "0") not in ("0", "0x00000000")]
    report = {
        "pid": pid,
        "started": started,
        "ended": ended,
        "timeline": str(timeline_csv.resolve()),
        "entity_csv": str(entity_csv.resolve()),
        "samples": len(rows),
        "nonzero_entity_rows": len(nonlocal_rows),
        "ratios": {
            "local_ok": sum(truth("local_ok", r) for r in rows) / len(rows) if rows else 0.0,
            "coord_table": sum(truth("coord_table", r) for r in rows) / len(rows) if rows else 0.0,
            "pos_valid": sum(truth("pos_valid", r) for r in rows) / len(rows) if rows else 0.0,
            "bone_mask": sum(truth("bone_mask", r) for r in rows) / len(rows) if rows else 0.0,
            "screen_valid": sum(truth("screen_valid", r) for r in rows) / len(rows) if rows else 0.0,
        },
    }
    report["gate_a_pass"] = bool(
        report["samples"] >= 10
        and report["nonzero_entity_rows"] >= 10
        and report["ratios"]["local_ok"] > 0.5
        and report["ratios"]["coord_table"] > 0.1
        and report["ratios"]["pos_valid"] > 0.1
        and report["ratios"]["bone_mask"] > 0.1
    )
    return report


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--wait", type=float, default=900.0, help="等待 CF 出现的秒数")
    ap.add_argument("--duration", type=float, default=900.0, help="进程出现后的采样秒数")
    ap.add_argument("--interval", type=float, default=0.10)
    ap.add_argument("--pid", type=int, default=0)
    ap.add_argument("--out-dir", default="runtime-diffs/match-baseline")
    ns = ap.parse_args()
    out_dir = Path(ns.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    deadline = time.monotonic() + max(1.0, ns.wait)
    pid = ns.pid
    while not pid and time.monotonic() < deadline:
        pid = hotkeys.find_pid()
        if not pid:
            time.sleep(0.5)
    if not pid:
        print("crossfire.exe 未在等待窗口内出现", flush=True)
        return 2
    stamp = time.strftime("%Y%m%d-%H%M%S")
    timeline = out_dir / f"stage1-timeline-pid{pid}-{stamp}.csv"
    modules_log = out_dir / f"stage1-modules-pid{pid}-{stamp}.json"
    gate_json = out_dir / f"stage1-gate-pid{pid}-{stamp}.json"
    fields = ["t", "pid", "crossfire_base", "cshell_base"]
    targets = {
        "crossfire.exe": [0xDB906C, 0xDB9094, 0xDB90C4, 0xDBFE54, 0xDBFE94, 0xDC13E0, 0xDCBB9C],
        "cshell.dll": [0x166ACE0, 0x166AD00, 0x1E71000, 0x1E71BA4, 0x1E71BB0, 0x1E8E498],
    }
    for name, rvas in targets.items():
        for rva in rvas:
            fields.extend(f"{name}@{rva:x}+{i}" for i in range(4))
    modules_seen = []
    # d3dref9_entities.csv is produced by the in-process bridge and can be
    # left behind by an earlier run.  Never use an old snapshot as evidence
    # for the current PID: record the start time and accept the file only if
    # its mtime advances during this sampling window.
    temp = Path(hotkeys.temp_dir()) if hasattr(hotkeys, "temp_dir") else Path(__import__("tempfile").gettempdir())
    entity_source = temp / "d3dref9_entities.csv"
    entity_mtime_before = entity_source.stat().st_mtime if entity_source.exists() else 0.0
    hp = open_read(pid)
    if not hp:
        print(f"OpenProcess 失败 PID={pid}", flush=True)
        return 3
    initial_pid = pid
    started = time.time()
    end = time.monotonic() + max(1.0, ns.duration)
    count = 0
    try:
        with timeline.open("w", newline="", encoding="utf-8") as f:
            writer = csv.DictWriter(f, fieldnames=fields)
            writer.writeheader()
            while time.monotonic() < end:
                # CF may restart during launcher/login transitions. Rebind to
                # the new PID instead of sampling a stale handle forever.
                live_pid = ns.pid or hotkeys.find_pid()
                if live_pid and live_pid != pid:
                    hotkeys.k32.CloseHandle(hp)
                    pid = live_pid
                    hp = open_read(pid)
                    if not hp:
                        time.sleep(0.5)
                        continue
                    print(f"rebind pid={pid}", flush=True)
                mods = module_map(pid)
                if mods:
                    modules_seen.append({k: {"base": v["base"], "size": v["size"], "path": v.get("path", "")} for k, v in mods.items() if k in targets})
                row = {"t": time.time(), "pid": pid, "crossfire_base": "", "cshell_base": ""}
                for name, rvas in targets.items():
                    m = mods.get(name.lower())
                    if m:
                        row["crossfire_base" if name == "crossfire.exe" else "cshell_base"] = hex(m["base"])
                    for rva in rvas:
                        raw = runtime.read_region(hp, m["base"] + rva, 16) if m else b""
                        vals = struct.unpack("<4f", raw) if len(raw) == 16 else [""] * 4
                        for i, value in enumerate(vals):
                            row[f"{name}@{rva:x}+{i}"] = value
                writer.writerow(row)
                f.flush()
                count += 1
                time.sleep(max(0.02, ns.interval))
    finally:
        if hp:
            hotkeys.k32.CloseHandle(hp)
    entity_csv = entity_source
    snapshot = out_dir / f"entities-pid{pid}-{stamp}.csv"
    entity_mtime_after = entity_csv.stat().st_mtime if entity_csv.exists() else 0.0
    entity_source_fresh = bool(entity_mtime_after > max(entity_mtime_before, started))
    if entity_source_fresh:
        try:
            shutil.copy2(entity_csv, snapshot)
        except OSError:
            snapshot = entity_csv
    else:
        # Keep the evidence path deterministic, but make a stale/missing
        # source produce an empty Gate-A input rather than reusing old rows.
        snapshot.write_text("entity_addr,local_ok,coord_table,pos_valid,bone_mask,screen_valid\n", encoding="utf-8")
    report = gate_report(snapshot, timeline, pid, started, time.time())
    report["initial_pid"] = initial_pid
    report["timeline_samples"] = count
    report["entity_source"] = str(entity_csv.resolve())
    report["entity_source_fresh"] = entity_source_fresh
    report["entity_source_mtime_before"] = entity_mtime_before
    report["entity_source_mtime_after"] = entity_mtime_after
    report["modules_observed"] = modules_seen[-1] if modules_seen else {}
    gate_json.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    modules_log.write_text(json.dumps(modules_seen, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps({"timeline": str(timeline.resolve()), "gate": str(gate_json.resolve()), "gate_a_pass": report["gate_a_pass"], "samples": count}, ensure_ascii=False), flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
