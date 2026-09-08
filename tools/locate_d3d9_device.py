#!/usr/bin/env python3
"""Read-only D3D9 device locator for the local x86 CF process."""
from __future__ import annotations
import ctypes, json, struct, sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
import runtime_hotkey_batch as b
import runtime_patch_capture as r

MEM_COMMIT=0x1000; PAGE_GUARD=0x100; READABLE={0x02,0x04,0x08,0x10,0x20,0x40,0x80}; CHUNK=1024*1024

def sections(hp,base):
 h=r.read_region(hp,base,0x1000); e=struct.unpack_from('<I',h,0x3c)[0]
 ns=struct.unpack_from('<H',h,e+6)[0]; opt=struct.unpack_from('<H',h,e+20)[0]; o=e+24+opt
 out=[]
 for i in range(ns):
  q=o+i*40; name=h[q:q+8].split(b'\0')[0].decode(errors='ignore'); vs,va,raw=struct.unpack_from('<III',h,q+8)
  out.append({'name':name,'start':base+va,'end':base+va+max(vs,raw)})
 return out

def main():
 pid=b.find_pid();
 if not pid:raise SystemExit('crossfire.exe not found')
 r.LIST_MODULES=tuple(set(r.LIST_MODULES+('d3d9.dll',)))
 mods=r.modules(pid); d3d=next((m for m in mods if m['name']=='d3d9.dll'),None)
 if not d3d:raise SystemExit('d3d9.dll not loaded')
 hp=b.k32.OpenProcess(b.PROCESS_QUERY_INFORMATION|b.PROCESS_VM_READ,False,pid)
 if not hp:raise OSError(ctypes.get_last_error(),'OpenProcess')
 try:
  ss=sections(hp,d3d['base']); text=next(s for s in ss if s['name']=='.text'); rdata=next((s for s in ss if s['name']=='.rdata'),None) or next((s for s in ss if s['name']=='.data'),None)
  if not rdata: raise SystemExit('d3d9 vtable data section unavailable')
  def in_text(x):return text['start']<=x<text['end']
  candidates=[]; scanned=0; regions=0; cur=0x10000; mbi=r.MEMORY_BASIC_INFORMATION()
  while cur<0x100000000:
   n=r.kernel32.VirtualQueryEx(hp,ctypes.c_void_p(cur),ctypes.byref(mbi),ctypes.sizeof(mbi))
   if not n or not mbi.RegionSize:break
   base=int(mbi.BaseAddress or 0); size=int(mbi.RegionSize); protect=int(mbi.Protect); end=base+size
   if mbi.State==MEM_COMMIT and not(protect&PAGE_GUARD) and (protect&0xff) in READABLE:
    regions+=1
    for off in range(0,size,CHUNK):
     raw=r.read_region(hp,base+off,min(CHUNK,size-off));scanned+=len(raw)
     upto=len(raw)-3
     for j in range(0,upto,4):
      vt=struct.unpack_from('<I',raw,j)[0]
      if not(rdata['start']<=vt<rdata['end']):continue
      table=r.read_region(hp,vt,83*4)
      if len(table)<83*4:continue
      entries=struct.unpack('<83I',table)
      needed=(0,1,2,16,17,41,42,82)
      if all(in_text(entries[k]) for k in needed):
       storage=base+off+j
       item={'storage':f'0x{storage:08X}','vtable':f'0x{vt:08X}','methods':{str(k):f'0x{entries[k]:08X}' for k in needed},'region_type':int(mbi.Type),'region_base':f'0x{base:08X}'}
       if item not in candidates:candidates.append(item)
   cur=end if end>cur else cur+0x1000
  out={'pid':pid,'d3d9':{'base':f"0x{d3d['base']:08X}",'size':d3d['size'],'text':text,'rdata':rdata},'read_only':True,'regions_scanned':regions,'bytes_scanned':scanned,'device_pointer_candidates':candidates}
  outp=Path('runtime-diffs/match-baseline/d3d9-device-candidates.json');outp.write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8')
  lines=['# D3D9 设备候选（只读扫描）','',f'- PID：`{pid}`；扫描：`{scanned}` 字节 / `{regions}` 个可读提交区。',f'- D3D9 模块基址：`0x{d3d["base"]:08X}`；候选数：`{len(candidates)}`。','', '|设备指针存储地址|虚表|Present (17)|EndScene (42)|DrawIndexedPrimitive (82)|','|---:|---:|---:|---:|---:|']
  for x in candidates:lines.append(f'|`{x["storage"]}`|`{x["vtable"]}`|`{x["methods"]["17"]}`|`{x["methods"]["42"]}`|`{x["methods"]["82"]}`|')
  lines+=['','该结果仅定位 COM 设备候选；还需在下一步记录 EndScene/DrawIndexedPrimitive 调用和渲染状态，才能将透视绘制接入。']
  Path('outputs/d3dref9自治/d3d9-device-candidates.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
  print(json.dumps({'pid':pid,'bytes_scanned':scanned,'candidates':len(candidates),'output':str(outp.resolve())},ensure_ascii=False))
 finally:b.k32.CloseHandle(hp)
if __name__=='__main__':main()
