#!/usr/bin/env python3
"""Read-only heap candidate sampler for CF entity/transform evidence."""
from __future__ import annotations
import argparse,ctypes,json,math,struct,sys,time
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import runtime_hotkey_batch as b, runtime_patch_capture as r
MEM_COMMIT=0x1000; PAGE_GUARD=0x100; READABLE={2,4,8,16,32,64,128}; PRIVATE=0x20000; MAPPED=0x40000

def regions(hp,mods):
 out=[]; cur=0x10000; mbi=r.MEMORY_BASIC_INFORMATION(); blocked=[(m['base'],m['base']+m['size']) for m in mods]
 while cur<0x100000000:
  n=r.kernel32.VirtualQueryEx(hp,ctypes.c_void_p(cur),ctypes.byref(mbi),ctypes.sizeof(mbi))
  if not n or not mbi.RegionSize: break
  a=int(mbi.BaseAddress); sz=int(mbi.RegionSize); prot=int(mbi.Protect)&0xff; typ=int(mbi.Type)
  if mbi.State==MEM_COMMIT and not(prot&PAGE_GUARD) and prot in READABLE and typ in (PRIVATE,MAPPED):
   # skip regions overlapping loaded images
   if not any(a<e and a+sz>s for s,e in blocked): out.append((a,sz,typ))
  cur=max(cur+0x1000,a+sz)
 return out

def vec(data,off):
 try: v=struct.unpack_from('<4f',data,off)
 except struct.error:return None
 if not all(math.isfinite(x) and abs(x)<20000 for x in v): return None
 if max(abs(x) for x in v[:3])<100: return None
 return v

def main():
 ap=argparse.ArgumentParser(); ap.add_argument('--pid',type=int,default=0); ap.add_argument('--duration',type=float,default=180); ap.add_argument('--interval',type=float,default=.5); ap.add_argument('--max-candidates',type=int,default=200000); ap.add_argument('--out',type=Path,default=Path('runtime-diffs/match-baseline/heap-entity-probe.json')); ns=ap.parse_args()
 pid=ns.pid or b.find_pid(); hp=b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION|b.PROCESS_VM_READ,False,pid)
 if not hp: raise OSError('OpenProcess failed')
 mods=r.modules(pid); regs=regions(hp,mods); print(f'pid={pid} private/mapped regions={len(regs)}',flush=True)
 candidates={}; scanned=0
 # initial discovery
 for base,sz,_ in regs:
  raw=r.read_region(hp,base,sz); scanned+=len(raw)
  for off in range(0,len(raw)-16,4):
   if len(candidates)>=ns.max_candidates: break
   v=vec(raw,off)
   if v is None: continue
   addr=base+off
   # retain only records with another plausible vec3 within 0x80 bytes
   near=False
   for no in range(max(0,off-0x80),min(len(raw)-16,off+0x84),4):
    if no!=off and vec(raw,no) is not None: near=True; break
   if near: candidates[addr]={'first':list(v),'samples':0,'changed':0,'max_delta':0.0,'last':list(v)}
  if len(candidates)>=ns.max_candidates: break
 print(f'discovered={len(candidates)} initial_bytes={scanned}',flush=True)
 start=time.time(); end=start+ns.duration; samples=0; next_discover=start+10
 while time.time()<end and candidates:
  if time.time()>=next_discover and len(candidates)<ns.max_candidates:
   # Re-discover newly allocated heap objects after entering a match.
   for base,sz,_ in regions(hp,mods):
    raw=r.read_region(hp,base,sz)
    for off in range(0,len(raw)-16,4):
     if len(candidates)>=ns.max_candidates: break
     if base+off in candidates: continue
     v=vec(raw,off)
     if v is None: continue
     near=any(no!=off and vec(raw,no) is not None for no in range(max(0,off-0x80),min(len(raw)-16,off+0x84),4))
     if near: candidates[base+off]={'first':list(v),'samples':0,'changed':0,'max_delta':0.0,'last':list(v)}
   next_discover+=10
  for addr,c in list(candidates.items()):
   raw=r.read_region(hp,addr,16); v=vec(raw,0) if len(raw)==16 else None
   if v is None: continue
   old=c['last']; delta=max(abs(v[i]-old[i]) for i in range(3)); c['samples']+=1; c['changed']+=delta>0.05; c['max_delta']=max(c['max_delta'],delta); c['last']=list(v)
  samples+=1; time.sleep(max(.05,ns.interval))
 ranked=sorted(candidates.items(), key=lambda kv:(kv[1]['changed'],kv[1]['max_delta']), reverse=True)
 out={'pid':pid,'duration':time.time()-start,'sample_rounds':samples,'regions':len(regs),'initial_bytes_scanned':scanned,'candidate_count':len(candidates),'read_only':True,'top_candidates':[{'address':f'0x{a:08X}',**c} for a,c in ranked[:500]]}
 ns.out.parent.mkdir(parents=True,exist_ok=True); ns.out.write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8'); print(f'complete samples={samples} output={ns.out.resolve()} top_changed={sum(c[1]["changed"]>0 for c in ranked[:500])}',flush=True); b.k32.CloseHandle(hp)
if __name__=='__main__':main()
