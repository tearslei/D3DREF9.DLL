#!/usr/bin/env python3
"""Statically map original-DLL feature states to code sites that consume them."""
from __future__ import annotations
import json
from collections import defaultdict
from pathlib import Path
import capstone, pefile
from capstone.x86 import X86_OP_MEM

DLL=Path(r'E:\game\已加速- CF2.0搭建（使用2012系统）\客户端\10.4CrossFire\D3DREF9.DLL')
OUT=Path('runtime-diffs/match-baseline/original-feature-processor-xrefs.json')
REPORT=Path('outputs/d3dref9自治/original-feature-processor-xrefs.md')
FEATURES={
 '人物透视':0x1A8D58,'无后坐力':0x1A8D60,'零秒换弹':0x1A8A90,'子弹穿墙':0x1A8A98,
 '刀枪爆头':0x1A8A9C,'第三人称':0x1A8DF4,'空格连跳':0x1A8DBC,'人物穿墙':0x1A8DC4,
 '蹲键遁地/瞬移通地':0x1A8F20,'旧不掉血':0x1A8A88,'摔不掉血':0x1A8C24,'无限背包':0x1A8DE0,
 '房间挂房不卡':0x1A8D50,'刷无线电':0x1A8F28,
}
pe=pefile.PE(str(DLL),fast_load=False); image_base=pe.OPTIONAL_HEADER.ImageBase
text=next(s for s in pe.sections if s.Name.rstrip(b'\0')==b'.text')
data=text.get_data(); va=image_base+text.VirtualAddress
md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);md.detail=True
wanted={image_base+rva:(name,rva) for name,rva in FEATURES.items()}
hits=defaultdict(list)
for ins in md.disasm(data,va):
 for op in ins.operands:
  if op.type==X86_OP_MEM and op.mem.disp in wanted:
   name,rva=wanted[op.mem.disp]
   hits[name].append({'va':f'0x{ins.address:08X}','rva':f'0x{ins.address-image_base:X}','bytes':ins.bytes.hex(),'mnemonic':ins.mnemonic,'op_str':ins.op_str})
# Context recovered by an exact short forward disassembly from each reference.
def context(addr):
 off=addr-va
 return [f'0x{i.address:08X}: {i.mnemonic} {i.op_str}'.rstrip() for i in md.disasm(data[off:off+0x60],addr)][:12]
result={'sample':str(DLL),'sha256':None,'image_base':f'0x{image_base:08X}','features':{}}
for name,rva in FEATURES.items():
 hs=hits[name]
 result['features'][name]={'state_rva':f'0x{rva:X}','state_va':f'0x{image_base+rva:08X}','xref_count':len(hs),'xrefs':[dict(x,context=context(int(x['va'],16))) for x in hs]}
OUT.parent.mkdir(parents=True,exist_ok=True); OUT.write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
lines=['# 原 D3DREF9 功能状态 → 处理器代码引用','',f'- 样本：`{DLL}`','- 方法：Capstone x86 静态扫描 `.text` 中对各状态全局变量的绝对内存引用。','- 这些地址是原 DLL 内部引用点，不是 `crossfire.exe/cshell.dll` 的游戏处理器 RVA。','', '|功能|状态 RVA|代码引用数|候选引用 VA|','|---|---:|---:|---|']
for name,rva in FEATURES.items():
 hs=hits[name]; shown=', '.join(x['va'] for x in hs[:10]) or '—'
 lines.append(f'|{name}|`0x{rva:X}`|{len(hs)}|`{shown}`|')
lines += ['','## 下一步解释','', '每项的完整指令上下文已保存 JSON。应先锁定同时消费多个游戏开关的长期运行线程/渲染循环，再对这些引用点做运行时断点，记录其调用栈、寄存器与目标模块访问；只有得到实体/武器对象与模块 RVA 后，才能把处理器写入干净 DLL。']
REPORT.write_text('\n'.join(lines)+'\n',encoding='utf-8')
print(json.dumps({n:len(hits[n]) for n in FEATURES},ensure_ascii=False))
print('json=',OUT.resolve());print('report=',REPORT.resolve())
