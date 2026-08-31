#!/usr/bin/env python3
"""Offline triage for the local x86 D3DREF9 sample.

It records PE facts, feature-string locations, direct code references (when
present), and imported-API call sites. It does not execute the sample or patch
the original file.
"""
from __future__ import annotations
import argparse, json, struct
from pathlib import Path
import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

FEATURES = [
    "自动瞄准", "自动开枪", "无线电", "瞬移", "第三人称", "人物穿墙",
    "空格连跳", "刀枪爆头", "子弹穿墙", "无后坐力", "人物透视", "不掉血",
    "无限背包", "挂房", "两键优化游戏进程", "摔不掉血", "零秒换弹",
]
INTERESTING = {
    "WinExec", "ShellExecuteA", "RegQueryValueA", "RegSetValueExA",
    "RegOpenKeyExA", "RegCreateKeyExA", "RegCloseKey", "WSAStartup",
    "WSACleanup", "select", "send", "recv", "recvfrom", "accept",
    "closesocket", "ioctlsocket", "WSAAsyncSelect", "InternetCloseHandle",
    "OpenProcess", "VirtualAlloc", "CreateThread", "SetWindowsHookExA",
    "keybd_event", "mouse_event",
}

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("input", type=Path)
    ap.add_argument("output", type=Path)
    ns = ap.parse_args()
    pe = pefile.PE(str(ns.input))
    raw = bytes(pe.__data__)
    base = pe.OPTIONAL_HEADER.ImageBase
    sections = []
    for s in pe.sections:
        sections.append({"name": s.Name.rstrip(b"\\0").decode(errors="replace"),
                         "rva": hex(s.VirtualAddress), "raw": hex(s.PointerToRawData),
                         "raw_size": hex(s.SizeOfRawData), "vsize": hex(s.Misc_VirtualSize),
                         "executable": bool(s.Characteristics & 0x20)})
    imports = {}
    api_sites = []
    for d in pe.DIRECTORY_ENTRY_IMPORT:
        dll = d.dll.decode(errors="replace")
        for x in d.imports:
            if not x.name:
                continue
            name = x.name.decode(errors="replace")
            imports[x.address] = {"dll": dll, "name": name}

    md = Cs(CS_ARCH_X86, CS_MODE_32)
    md.detail = True
    for s in pe.sections:
        if not (s.Characteristics & 0x20):
            continue
        va = base + s.VirtualAddress
        for ins in md.disasm(s.get_data(), va):
            # Absolute memory operand pointing into IAT.
            for op in ins.operands:
                candidate = None
                if op.type == 3 and op.mem.base == 0 and op.mem.index == 0:
                    candidate = op.mem.disp & 0xFFFFFFFF
                elif op.type == 2:
                    candidate = op.imm & 0xFFFFFFFF
                ent = imports.get(candidate) if candidate is not None else None
                if ent and ent["name"] in INTERESTING:
                    api_sites.append({"rva": hex(ins.address - base), "mnemonic": ins.mnemonic,
                                      "op_str": ins.op_str, **ent})
                    break

    features = []
    for label in FEATURES:
        needle = label.encode("gbk")
        pos = raw.find(needle)
        item = {"label": label, "file_offset": hex(pos) if pos >= 0 else None}
        if pos >= 0:
            item["rva"] = hex(pe.get_rva_from_offset(pos))
            item["va"] = hex(base + pe.get_rva_from_offset(pos))
            # Search exact VA and RVA pointers in executable bytes. A lack of
            # direct references is useful evidence of runtime string tables or
            # generated UI data rather than a normal static xref.
            va = base + pe.get_rva_from_offset(pos)
            refs = []
            for value in (va, va - base):
                needle4 = struct.pack("<I", value & 0xFFFFFFFF)
                start = 0
                while True:
                    hit = raw.find(needle4, start)
                    if hit < 0:
                        break
                    for sec in pe.sections:
                        if sec.PointerToRawData <= hit < sec.PointerToRawData + sec.SizeOfRawData and sec.Characteristics & 0x20:
                            refs.append(hex(base + sec.VirtualAddress + hit - sec.PointerToRawData - base))
                    start = hit + 1
            item["direct_code_refs"] = sorted(set(refs))
        features.append(item)

    out = {"input": str(ns.input), "image_base": hex(base),
           "entry_rva": hex(pe.OPTIONAL_HEADER.AddressOfEntryPoint),
           "machine": hex(pe.FILE_HEADER.Machine), "sections": sections,
           "exports": [{"ordinal": x.ordinal, "name": x.name.decode(errors="replace") if x.name else None,
                        "rva": hex(x.address)} for x in pe.DIRECTORY_ENTRY_EXPORT.symbols],
           "features": features, "api_sites": api_sites}
    ns.output.parent.mkdir(parents=True, exist_ok=True)
    ns.output.write_text(json.dumps(out, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(out, ensure_ascii=False, indent=2))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
