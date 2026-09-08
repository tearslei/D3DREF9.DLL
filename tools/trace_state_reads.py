#!/usr/bin/env python3
"""External x86/WOW64 hardware-watchpoint tracer for a known D3DREF9 state.
Attaches as a debugger, sets DR0 read/write watchpoints on all target threads,
and records EIP/thread/time for each hit. It does not patch target memory.
"""
from __future__ import annotations
import argparse,ctypes,ctypes.wintypes as wt,json,sys,time
from pathlib import Path
HERE=Path(__file__).resolve().parent;sys.path.insert(0,str(HERE))
import runtime_patch_capture as rpc

k=ctypes.WinDLL('kernel32',use_last_error=True)
DBG_CONTINUE=0x00010002; EXC_SINGLE_STEP=0x80000004; CREATE_THREAD=2; CREATE_PROCESS=3; EXIT_THREAD=4; EXIT_PROCESS=5; EXCEPTION_DEBUG=1
THREAD_SET_CONTEXT=0x10; THREAD_GET_CONTEXT=0x8; THREAD_QUERY_INFORMATION=0x40; THREAD_SUSPEND_RESUME=0x2
CONTEXT_DEBUG=0x00010010
class WOW64_FLOAT(ctypes.Structure):
 _fields_=[('ControlWord',wt.DWORD),('StatusWord',wt.DWORD),('TagWord',wt.DWORD),('ErrorOffset',wt.DWORD),('ErrorSelector',wt.DWORD),('DataOffset',wt.DWORD),('DataSelector',wt.DWORD),('RegisterArea',wt.BYTE*80),('Cr0NpxState',wt.DWORD)]
class WOW64_CONTEXT(ctypes.Structure):
 _fields_=[('ContextFlags',wt.DWORD),('Dr0',wt.DWORD),('Dr1',wt.DWORD),('Dr2',wt.DWORD),('Dr3',wt.DWORD),('Dr6',wt.DWORD),('Dr7',wt.DWORD),('FloatSave',WOW64_FLOAT),('SegGs',wt.DWORD),('SegFs',wt.DWORD),('SegEs',wt.DWORD),('SegDs',wt.DWORD),('Edi',wt.DWORD),('Esi',wt.DWORD),('Ebx',wt.DWORD),('Edx',wt.DWORD),('Ecx',wt.DWORD),('Eax',wt.DWORD),('Ebp',wt.DWORD),('Eip',wt.DWORD),('SegCs',wt.DWORD),('EFlags',wt.DWORD),('Esp',wt.DWORD),('SegSs',wt.DWORD),('ExtendedRegisters',wt.BYTE*512)]
k.OpenThread.argtypes=[wt.DWORD,wt.BOOL,wt.DWORD]; k.OpenThread.restype=wt.HANDLE
k.CloseHandle.argtypes=[wt.HANDLE]
k.Wow64GetThreadContext.argtypes=[wt.HANDLE,ctypes.POINTER(WOW64_CONTEXT)]; k.Wow64GetThreadContext.restype=wt.BOOL
k.Wow64SetThreadContext.argtypes=[wt.HANDLE,ctypes.POINTER(WOW64_CONTEXT)]; k.Wow64SetThreadContext.restype=wt.BOOL
k.DebugActiveProcess.argtypes=[wt.DWORD]; k.DebugActiveProcess.restype=wt.BOOL
k.DebugActiveProcessStop.argtypes=[wt.DWORD]; k.DebugActiveProcessStop.restype=wt.BOOL
k.WaitForDebugEvent.argtypes=[ctypes.c_void_p,wt.DWORD]; k.WaitForDebugEvent.restype=wt.BOOL
k.ContinueDebugEvent.argtypes=[wt.DWORD,wt.DWORD,wt.DWORD]; k.ContinueDebugEvent.restype=wt.BOOL
k.CreateToolhelp32Snapshot.argtypes=[wt.DWORD,wt.DWORD]; k.CreateToolhelp32Snapshot.restype=wt.HANDLE
TH32CS_SNAPTHREAD=0x4
class THREADENTRY32(ctypes.Structure):
 _fields_=[('dwSize',wt.DWORD),('cntUsage',wt.DWORD),('th32ThreadID',wt.DWORD),('th32OwnerProcessID',wt.DWORD),('tpBasePri',wt.LONG),('tpDeltaPri',wt.LONG),('dwFlags',wt.DWORD)]
k.Thread32First.argtypes=[wt.HANDLE,ctypes.POINTER(THREADENTRY32)]; k.Thread32First.restype=wt.BOOL
k.Thread32Next.argtypes=[wt.HANDLE,ctypes.POINTER(THREADENTRY32)]; k.Thread32Next.restype=wt.BOOL

def set_watch(tid,addr):
 h=k.OpenThread(THREAD_GET_CONTEXT|THREAD_SET_CONTEXT|THREAD_QUERY_INFORMATION,False,tid)
 if not h:return False
 try:
  c=WOW64_CONTEXT();c.ContextFlags=CONTEXT_DEBUG
  if not k.Wow64GetThreadContext(h,ctypes.byref(c)):return False
  c.Dr0=addr;c.Dr6=0
  # local DR0 enabled; RW=read/write (3), LEN=4 bytes (3)
  c.Dr7=(c.Dr7 & ~0x000F0001) | 0x000F0001
  return bool(k.Wow64SetThreadContext(h,ctypes.byref(c)))
 finally:k.CloseHandle(h)

def thread_ids(pid):
 h=k.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0); out=[]
 if h==wt.HANDLE(-1).value:return out
 try:
  e=THREADENTRY32();e.dwSize=ctypes.sizeof(e)
  if k.Thread32First(h,ctypes.byref(e)):
   while True:
    if e.th32OwnerProcessID==pid:out.append(int(e.th32ThreadID))
    if not k.Thread32Next(h,ctypes.byref(e)):break
 finally:k.CloseHandle(h)
 return out

def main():
 ap=argparse.ArgumentParser();ap.add_argument('--pid',type=int,required=True);ap.add_argument('--module',default='d3dref9.dll');ap.add_argument('--rva',type=lambda x:int(x,0),default=0x1a8d58);ap.add_argument('--seconds',type=float,default=120);ap.add_argument('--max-events',type=int,default=20000);ap.add_argument('--out',type=Path,default=Path('runtime-diffs/state-read-trace.json'));ns=ap.parse_args()
 mods=rpc.modules(ns.pid);m=next((x for x in mods if x['name']==ns.module.lower()),None)
 if not m:raise SystemExit(f'module not loaded: {ns.module}')
 addr=m['base']+ns.rva; events=[]; attached=False; evbuf=ctypes.create_string_buffer(176); start=time.time(); configured=set()
 if not k.DebugActiveProcess(ns.pid):raise OSError(ctypes.get_last_error(),'DebugActiveProcess')
 attached=True
 try:
  for tid in thread_ids(ns.pid):
   if set_watch(tid,addr):configured.add(tid)
  while time.time()-start<ns.seconds and len(events)<ns.max_events:
   if not k.WaitForDebugEvent(evbuf,1000):continue
   raw=evbuf.raw; code=int.from_bytes(raw[0:4],'little'); pid=int.from_bytes(raw[4:8],'little');tid=int.from_bytes(raw[8:12],'little')
   if code==EXCEPTION_DEBUG:
    ex=int.from_bytes(raw[12:16],'little')
    if ex==EXC_SINGLE_STEP:
     h=k.OpenThread(THREAD_GET_CONTEXT|THREAD_SET_CONTEXT|THREAD_QUERY_INFORMATION,False,tid); eip=0; dr6=0
     if h:
      try:
       c=WOW64_CONTEXT();c.ContextFlags=CONTEXT_DEBUG
       if k.Wow64GetThreadContext(h,ctypes.byref(c)):eip=int(c.Eip);dr6=int(c.Dr6);c.Dr6=0;k.Wow64SetThreadContext(h,ctypes.byref(c))
      finally:k.CloseHandle(h)
     events.append({'t':time.time(),'thread':tid,'eip':f'0x{eip:08X}','dr6':f'0x{dr6:X}'})
   elif code==CREATE_THREAD:
    set_watch(tid,addr);configured.add(tid)
   k.ContinueDebugEvent(pid,tid,DBG_CONTINUE)
 finally:
  if attached:k.DebugActiveProcessStop(ns.pid)
 out={'pid':ns.pid,'target':{'module':m['name'],'base':f'0x{m["base"]:08X}','rva':f'0x{ns.rva:X}','address':f'0x{addr:08X}'},'seconds':time.time()-start,'configured_threads':sorted(configured),'event_count':len(events),'events':events,'read_only_target_memory':True,'note':'DR0 hardware read/write watchpoint; EIP identifies state readers/writers. Resolve EIP against module bases after capture.'}
 ns.out.parent.mkdir(parents=True,exist_ok=True);ns.out.write_text(json.dumps(out,ensure_ascii=False,indent=2),encoding='utf-8');print(json.dumps({'pid':ns.pid,'address':hex(addr),'configured_threads':len(configured),'events':len(events),'output':str(ns.out.resolve())},ensure_ascii=False))
if __name__=='__main__':main()
