import subprocess,sys,time,pathlib
root=pathlib.Path('outputs/d3dref9自治/tools').resolve(); sys.path.insert(0,str(root)); import runtime_patch_capture as r
seen=set(); print('waiting for a new crossfire.exe...',flush=True)
while True:
 out=subprocess.check_output(['powershell','-NoProfile','-Command','(Get-Process -Name crossfire -ErrorAction SilentlyContinue | Select-Object -First 1 -Expand Id)'],text=True).strip()
 if out.isdigit():
  pid=int(out)
  if pid not in seen:
   try: mods=r.modules(pid)
   except OSError: time.sleep(1); continue
   names={m['name'] for m in mods}
   if {'d3dref9.dll','cshell.dll'}.issubset(names):
    seen.add(pid); print(f'new target pid={pid}; waiting 20s before state-reader trace',flush=True); time.sleep(20)
    cmd=[sys.executable,str(root/'trace_state_reads.py'),'--pid',str(pid),'--rva','0x1A8D58','--seconds','180','--out',str(pathlib.Path('runtime-diffs/state-read-trace-f76.json').resolve())]
    heap=[sys.executable,str(root/'heap_entity_probe.py'),'--pid',str(pid),'--duration','180','--interval','0.5','--out',str(pathlib.Path('runtime-diffs/heap-entity-probe-f76.json').resolve())]
    p1=subprocess.Popen(cmd,cwd=str(pathlib.Path.cwd())); p2=subprocess.Popen(heap,cwd=str(pathlib.Path.cwd()))
    p1.wait(); p2.wait(); print('state-reader and heap trace finished; waiting for next restart',flush=True)
 time.sleep(2)
