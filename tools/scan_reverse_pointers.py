#!/usr/bin/env python3
"""Read-only reverse-pointer scan for a 32-bit local CF process.

Given a module RVA that holds a dynamic transform/coordinate candidate, find
all 32-bit pointers in the target process that refer directly to it.  No
injection, suspend, writes, or input synthesis is used.
"""
from __future__ import annotations
import argparse, ctypes, json, struct, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
import runtime_hotkey_batch as b
import runtime_patch_capture as r

MEM_COMMIT=0x1000
PAGE_GUARD=0x100
PAGE_NOACCESS=0x01
READABLE={0x02,0x04,0x08,0x10,0x20,0x40,0x80}
CHUNK=1024*1024


def label(addr:int, mods:list[dict], mbi) -> str:
    for m in mods:
        if m['base'] <= addr < m['base']+m['size']:
            return f"{m['name']}+0x{addr-m['base']:X}"
    typ={0x20000:'private',0x40000:'mapped',0x1000000:'image'}.get(int(mbi.Type),'other')
    return f'{typ}@0x{addr:X}'

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--module',default='cshell.dll')
    ap.add_argument('--rva',default='0x1e71000',type=lambda x:int(x,0))
    ap.add_argument('--max-results',default=5000,type=int)
    ap.add_argument('--out',type=Path,default=Path('runtime-diffs/match-baseline/reverse-pointers.json'))
    ns=ap.parse_args()
    pid=b.find_pid()
    if not pid: raise SystemExit('crossfire.exe not found')
    mods=r.modules(pid); target_mod=next((m for m in mods if m['name']==ns.module.lower()),None)
    if not target_mod: raise SystemExit(f'module not loaded: {ns.module}')
    target=target_mod['base']+ns.rva; needle=struct.pack('<I',target)
    hp=b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION|b.PROCESS_VM_READ,False,pid)
    if not hp: raise OSError(ctypes.get_last_error(),'OpenProcess')
    hits=[]; regions=0; bytes_scanned=0; cur=0x10000
    max_addr=0x100000000
    mbi=r.MEMORY_BASIC_INFORMATION()
    try:
        while cur < max_addr:
            n=r.kernel32.VirtualQueryEx(hp,ctypes.c_void_p(cur),ctypes.byref(mbi),ctypes.sizeof(mbi))
            if not n or not mbi.RegionSize: break
            base=int(mbi.BaseAddress or 0); size=int(mbi.RegionSize); end=base+size
            protect=int(mbi.Protect)
            if mbi.State==MEM_COMMIT and not (protect & PAGE_GUARD) and (protect & 0xff) in READABLE:
                regions+=1; offset=0; tail=b''
                while offset<size:
                    want=min(CHUNK,size-offset)
                    data=r.read_region(hp,base+offset,want)
                    if data:
                        blob=tail+data; blob_base=base+offset-len(tail)
                        start=0
                        while len(hits)<ns.max_results:
                            p=blob.find(needle,start)
                            if p<0: break
                            addr=blob_base+p
                            if addr%4==0:
                                hits.append({'address':f'0x{addr:08X}','label':label(addr,mods,mbi),'region_base':f'0x{base:08X}','region_size':size,'protect':protect,'type':int(mbi.Type)})
                            start=p+1
                        tail=blob[-3:]
                        bytes_scanned+=len(data)
                    offset+=want
                    if len(hits)>=ns.max_results: break
            cur=end if end>cur else cur+0x1000
            if len(hits)>=ns.max_results: break
        for hit in hits:
            a=int(hit['address'],16); before=max(0,a-0x40)
            raw=r.read_region(hp,before,0xC0)
            words=[]
            for off in range(0,len(raw)-3,4):
                v=struct.unpack_from('<I',raw,off)[0]
                if target-0x2000 <= v <= target+0x2000:
                    words.append({'offset':f'{off-0x40:+#x}','value':f'0x{v:08X}','relative_to_target':v-target})
            hit['nearby_target_pointers']=words
            hit['surrounding_hex']=raw.hex()
        out={'pid':pid,'target':{'module':target_mod['name'],'base':f"0x{target_mod['base']:08X}",'rva':f'0x{ns.rva:X}','address':f'0x{target:08X}'},'read_only':True,'regions_scanned':regions,'bytes_scanned':bytes_scanned,'direct_pointer_hit_count':len(hits),'hits':hits}
        ns.out.parent.mkdir(parents=True,exist_ok=True); ns.out.write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
        print(json.dumps({k:out[k] for k in ('pid','target','regions_scanned','bytes_scanned','direct_pointer_hit_count')},ensure_ascii=False))
        print('output=',ns.out.resolve())
    finally:
        b.k32.CloseHandle(hp)
if __name__=='__main__': main()
