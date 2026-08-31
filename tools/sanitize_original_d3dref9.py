#!/usr/bin/env python3
"""Create a reversible, locally sanitized copy of the original D3DREF9.DLL.

The patch is deliberately limited to imported network, registry, ShellExecute and
WinExec call sites.  The original file is never modified.  This is not a feature
reimplementation and must be tested against the private client before replacing
anything.
"""
from __future__ import annotations
import argparse, json, os, struct
from pathlib import Path
import pefile

RET = {
    # WS2_32 wrappers (all return zero so callers take their existing failure path)
    "select": 20, "WSACleanup": 0, "WSAStartup": 8, "send": 16,
    "closesocket": 4, "WSAAsyncSelect": 16, "recvfrom": 24,
    "ioctlsocket": 12, "getpeername": 12, "accept": 12, "recv": 16,
    # Registry / shell / WinINet / process launch
    "RegQueryValueA": 16, "RegSetValueExA": 24, "RegOpenKeyExA": 20,
    "RegCloseKey": 4, "RegCreateKeyExA": 36, "ShellExecuteA": 24,
    "InternetCloseHandle": 4, "WinExec": 8,
}

TARGET_DLLS = {"WS2_32.dll", "ADVAPI32.dll", "SHELL32.dll", "WININET.dll", "KERNEL32.dll"}

def p32(v: int) -> bytes:
    return struct.pack("<I", v & 0xFFFFFFFF)

def stub(ret_bytes: int) -> bytes:
    # xor eax,eax; ret / ret imm16.  Existing callers receive a deterministic
    # failure/empty result and the callee preserves stdcall stack balance.
    b = b"\x31\xC0"
    if ret_bytes:
        b += b"\xC2" + struct.pack("<H", ret_bytes)
    else:
        b += b"\xC3"
    return b

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("input", type=Path)
    ap.add_argument("output", type=Path)
    args = ap.parse_args()
    pe = pefile.PE(str(args.input))
    if pe.FILE_HEADER.Machine != 0x14C:
        raise SystemExit("仅支持 x86 PE（Machine=0x14c）")
    raw = bytearray(Path(args.input).read_bytes())
    base = pe.OPTIONAL_HEADER.ImageBase
    imports = {}
    for desc in pe.DIRECTORY_ENTRY_IMPORT:
        dll = desc.dll.decode(errors="replace")
        for ent in desc.imports:
            if ent.name:
                imports[ent.address] = (dll, ent.name.decode(errors="replace"))

    # Find executable zero cave, large enough for all unique stubs.
    text = next(s for s in pe.sections if s.Name.rstrip(b"\0") == b".text")
    text_off, text_size = text.PointerToRawData, text.SizeOfRawData
    cave_rel = bytes(raw[text_off:text_off + text_size]).find(b"\0" * 512)
    if cave_rel < 0:
        raise SystemExit(".text 中未找到可用代码空洞")
    cave_off = text_off + cave_rel
    cave_va = base + text.VirtualAddress + cave_rel

    # One stub per API.  Each direct call and import trampoline is redirected to it.
    stubs = {}
    cursor = cave_off
    for iat, (dll, name) in sorted(imports.items()):
        if name not in RET or dll not in TARGET_DLLS:
            continue
        if name in stubs:
            continue
        code = stub(RET[name])
        raw[cursor:cursor + len(code)] = code
        stubs[name] = base + text.VirtualAddress + (cursor - text_off)
        cursor += len(code)
    if cursor - cave_off > 512:
        raise SystemExit("代码空洞不足")

    patched = []
    # Redirect direct `call dword ptr [IAT]` and import trampolines `jmp [IAT]`.
    for s in pe.sections:
        if not (s.Characteristics & 0x20):
            continue
        start, end = s.PointerToRawData, s.PointerToRawData + s.SizeOfRawData
        i = start
        while i + 6 <= end:
            if raw[i] not in (0xFF,):
                i += 1; continue
            op = raw[i + 1]
            if op not in (0x15, 0x25):
                i += 1; continue
            iat = struct.unpack_from("<I", raw, i + 2)[0]
            ent = imports.get(iat)
            if not ent or ent[1] not in stubs or ent[0] not in TARGET_DLLS:
                i += 1; continue
            ins_va = base + next(sec.VirtualAddress + (i - sec.PointerToRawData) for sec in pe.sections if sec.PointerToRawData <= i < sec.PointerToRawData + sec.SizeOfRawData)
            rel = stubs[ent[1]] - (ins_va + 5)
            # Keep trampoline semantics: FF/25 is a tail JMP and must remain a
            # JMP.  Direct call sites (FF/15) become CALLs so their return path
            # remains unchanged.
            raw[i:i + 5] = (b"\xE8" if op == 0x15 else b"\xE9") + struct.pack("<i", rel)
            raw[i + 5] = 0x90
            patched.append({"rva": hex(ins_va - base), "dll": ent[0], "api": ent[1], "kind": "call" if op == 0x15 else "jmp"})
            i += 6

    # Neutralize the known IOC strings without changing file layout.
    iocs = [b"https://mp3.t57.cn:7087/kwlink_d.php?id=207449126", b"www.zhenniu.biz", b"zhenniu.biz", b"www.xzgogo.com", b"xzgogo.com", b"CrossProxy.exe", b"CrossSSOHolder.exe"]
    scrubbed = []
    for term in iocs:
        pos = 0
        while True:
            pos = raw.lower().find(term.lower(), pos)
            if pos < 0: break
            replacement = b"disabled" + b"_" * max(0, len(term) - len("disabled"))
            raw[pos:pos + len(term)] = replacement[:len(term)]
            scrubbed.append({"offset": hex(pos), "value": term.decode(errors="replace")})
            pos += len(term)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(raw)
    report = {"input": str(args.input), "output": str(args.output), "patched_calls": patched, "scrubbed_strings": scrubbed, "cave_rva": hex(cave_va - base)}
    args.output.with_suffix(args.output.suffix + ".json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False, indent=2))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
