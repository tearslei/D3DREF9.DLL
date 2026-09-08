#Requires -Version 5.1
[CmdletBinding()]
param([string]$ClientDir)
$ErrorActionPreference='Stop'
if([IntPtr]::Size -ne 4){
  $ps32=Join-Path $env:WINDIR 'SysWOW64\WindowsPowerShell\v1.0\powershell.exe'
  $forward=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$PSCommandPath)
  if(-not [string]::IsNullOrWhiteSpace($ClientDir)){$forward+=@('-ClientDir',$ClientDir)}
  & $ps32 @forward; exit $LASTEXITCODE
}
if([string]::IsNullOrWhiteSpace($ClientDir)) {
  $candidate=Get-ChildItem -LiteralPath 'E:\game' -Filter 'D3DREF9.DLL' -File -Recurse -ErrorAction SilentlyContinue | Where-Object {
    (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash -eq 'E73FEC5C4692D06B136BE3D451A88BEE05921E0076E260C969F3C97C2A905A5A'
  } | Select-Object -First 1
  if($candidate){$ClientDir=$candidate.DirectoryName}
}
if([string]::IsNullOrWhiteSpace($ClientDir)){throw 'ClientDir not supplied and D3DREF9.original.dll was not found under E:\game.'}
$exe=Join-Path $ClientDir 'crossfire.exe'
if(!(Test-Path -LiteralPath $exe)){throw "crossfire.exe not found: $exe"}
$bat=Get-ChildItem -LiteralPath $ClientDir -Filter '*.bat' -File | Where-Object {
  (Get-Content -LiteralPath $_.FullName -Encoding Default -ErrorAction SilentlyContinue) -match '(?i)crossfire\.exe'
} | Select-Object -First 1
$line=''; if($bat){$line=Get-Content -LiteralPath $bat.FullName -Encoding Default | Where-Object {$_ -match '(?i)crossfire\.exe'} | Select-Object -First 1}
$args=''; if($line){$args=($line -replace '^.*?(?i:crossfire\.exe)\s*','').Trim()}
$trace=Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) 'd3d9_early_trace.dll'
if(!(Test-Path -LiteralPath $trace)){throw "Missing tracer: $trace"}
Get-Process crossfire -ErrorAction SilentlyContinue | Stop-Process -Force
Add-Type @'
using System; using System.Text; using System.Runtime.InteropServices;
public static class SuspendedNative {
 [StructLayout(LayoutKind.Sequential, CharSet=CharSet.Unicode)] public struct SI { public int cb; public string r1,r2,r3; public int x,y,w,h; public int c,d; public IntPtr sIn,sOut,sErr; }
 [StructLayout(LayoutKind.Sequential)] public struct PI { public IntPtr hProcess,hThread; public int pid,tid; }
 [Flags] public enum PA:uint { CreateThread=2,Query=0x400,VMOperation=8,VMWrite=0x20,Synchronize=0x100000 }
 [Flags] public enum AT:uint { Commit=0x1000,Reserve=0x2000,Release=0x8000 }
 [DllImport("kernel32",CharSet=CharSet.Unicode,SetLastError=true)] public static extern bool CreateProcess(string app,string cmd,IntPtr pa,IntPtr ta,bool inherit,uint flags,string env,string cwd,ref SI si,out PI pi);
 [DllImport("kernel32",SetLastError=true)] public static extern IntPtr VirtualAllocEx(IntPtr p,IntPtr a,IntPtr n,AT t,uint prot);
 [DllImport("kernel32",SetLastError=true)] public static extern bool WriteProcessMemory(IntPtr p,IntPtr a,byte[] b,IntPtr n,out IntPtr w);
 [DllImport("kernel32",CharSet=CharSet.Ansi,SetLastError=true)] public static extern IntPtr GetProcAddress(IntPtr m,string n);
 [DllImport("kernel32",CharSet=CharSet.Unicode,SetLastError=true)] public static extern IntPtr GetModuleHandle(string n);
 [DllImport("kernel32",SetLastError=true)] public static extern IntPtr CreateRemoteThread(IntPtr p,IntPtr a,IntPtr s,IntPtr start,IntPtr param,uint f,out uint id);
 [DllImport("kernel32",SetLastError=true)] public static extern uint WaitForSingleObject(IntPtr h,uint ms);
 [DllImport("kernel32",SetLastError=true)] public static extern uint ResumeThread(IntPtr h);
 [DllImport("kernel32",SetLastError=true)] public static extern bool CloseHandle(IntPtr h);
}
'@
$si=New-Object SuspendedNative+SI; $si.cb=[Runtime.InteropServices.Marshal]::SizeOf($si); $pi=New-Object SuspendedNative+PI
$cmd='"'+$exe+'"'; if($args){$cmd+=' '+$args}
if(!( [SuspendedNative]::CreateProcess($exe,$cmd,[IntPtr]::Zero,[IntPtr]::Zero,$false,4,$null,$ClientDir,[ref]$si,[ref]$pi))){throw "CreateProcess failed: $([Runtime.InteropServices.Marshal]::GetLastWin32Error())"}
$remote=[IntPtr]::Zero; try {
  $data=[Text.Encoding]::Unicode.GetBytes($trace+[char]0)
  $remote=[SuspendedNative]::VirtualAllocEx($pi.hProcess,[IntPtr]::Zero,[IntPtr]$data.Length,([SuspendedNative+AT]::Commit -bor [SuspendedNative+AT]::Reserve),4)
  if(!$remote){throw 'VirtualAllocEx failed'}; [IntPtr]$written=[IntPtr]::Zero
  if(![SuspendedNative]::WriteProcessMemory($pi.hProcess,$remote,$data,[IntPtr]$data.Length,[ref]$written)){throw 'WriteProcessMemory failed'}
  $ll=[SuspendedNative]::GetProcAddress([SuspendedNative]::GetModuleHandle('kernel32.dll'),'LoadLibraryW'); [uint32]$tid=0
  $rt=[SuspendedNative]::CreateRemoteThread($pi.hProcess,[IntPtr]::Zero,[IntPtr]::Zero,$ll,$remote,0,[ref]$tid); if(!$rt){throw 'CreateRemoteThread failed'}
  [void][SuspendedNative]::WaitForSingleObject($rt,10000); [void][SuspendedNative]::CloseHandle($rt)
  [void][SuspendedNative]::ResumeThread($pi.hThread)
  Write-Output "Started suspended/injected PID=$($pi.pid)"; Write-Output "Tracer=$trace"
} finally { if($pi.hThread){[void][SuspendedNative]::CloseHandle($pi.hThread)}; if($pi.hProcess){[void][SuspendedNative]::CloseHandle($pi.hProcess)} }
