#Requires -Version 5.1
[CmdletBinding()]
param(
  [string]$ClientDir = 'E:\game\已加速- CF2.0搭建（使用2012系统）\客户端\10.4CrossFire',
  [string]$DllPath = (Join-Path $PSScriptRoot 'd3dref9自治.dll'),
  [switch]$Launch,
  [int]$WaitSeconds = 60
)

$ErrorActionPreference = 'Stop'
$targetName = 'crossfire'
$launchBat = Join-Path $ClientDir 'A点我启动游戏.bat'

# crossfire.exe 是 x86；远程线程入口地址必须来自同位数 PowerShell。
# 从 64 位 PowerShell 手工运行时自动转到 SysWOW64 的 32 位 PowerShell。
if ([IntPtr]::Size -ne 4) {
  $ps32 = Join-Path $env:WINDIR 'SysWOW64\WindowsPowerShell\v1.0\powershell.exe'
  if (-not (Test-Path -LiteralPath $ps32)) {
    throw '未找到 32 位 PowerShell；x86 crossfire.exe 必须使用 32 位 PowerShell 执行加载脚本。'
  }
  $forward = @('-NoProfile','-ExecutionPolicy','Bypass','-File',$PSCommandPath,
    '-ClientDir',$ClientDir,'-DllPath',$DllPath,'-WaitSeconds',$WaitSeconds)
  if ($Launch) { $forward += '-Launch' }
  & $ps32 @forward
  exit $LASTEXITCODE
}

if (-not (Test-Path -LiteralPath $DllPath)) {
  throw "DLL 不存在：$DllPath`n先运行 .\build.ps1 编译。"
}
if (-not (Test-Path -LiteralPath $ClientDir)) {
  throw "客户端目录不存在：$ClientDir"
}

if (-not ('D3DRef9Injector.Native' -as [type])) {
  Add-Type -TypeDefinition @'
using System;
using System.ComponentModel;
using System.Runtime.InteropServices;

namespace D3DRef9Injector {
  public static class Native {
    [Flags]
    public enum ProcessAccess : uint {
      CreateThread = 0x0002, QueryInformation = 0x0400, VMOperation = 0x0008,
      VMWrite = 0x0020, VMRead = 0x0010, Synchronize = 0x00100000
    }
    [Flags] public enum AllocationType : uint { Commit = 0x1000, Reserve = 0x2000, Release = 0x8000 }
    [Flags] public enum MemoryProtection : uint { ReadWrite = 0x04 }
    [DllImport("kernel32.dll", SetLastError=true)] public static extern IntPtr OpenProcess(ProcessAccess access, bool inherit, int pid);
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool CloseHandle(IntPtr h);
    [DllImport("kernel32.dll", SetLastError=true)] public static extern IntPtr VirtualAllocEx(IntPtr p, IntPtr a, UIntPtr size, AllocationType type, MemoryProtection protect);
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool VirtualFreeEx(IntPtr p, IntPtr a, UIntPtr size, AllocationType type);
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool WriteProcessMemory(IntPtr p, IntPtr a, byte[] data, UIntPtr size, out UIntPtr written);
    [DllImport("kernel32.dll", CharSet=CharSet.Ansi, SetLastError=true)] public static extern IntPtr GetProcAddress(IntPtr module, string name);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode, SetLastError=true)] public static extern IntPtr GetModuleHandle(string name);
    [DllImport("kernel32.dll", SetLastError=true)] public static extern IntPtr CreateRemoteThread(IntPtr p, IntPtr attrs, UIntPtr stack, IntPtr start, IntPtr parameter, uint flags, out uint tid);
    [DllImport("kernel32.dll", SetLastError=true)] public static extern uint WaitForSingleObject(IntPtr h, uint ms);
    [DllImport("kernel32.dll", SetLastError=true)] public static extern bool GetExitCodeThread(IntPtr h, out uint code);
    public static void ThrowLast(string op) { throw new Win32Exception(Marshal.GetLastWin32Error(), op); }
  }
}
'@
}

$p = Get-Process -Name $targetName -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $p -and $Launch) {
  if (-not (Test-Path -LiteralPath $launchBat)) { throw "找不到启动脚本：$launchBat" }
  Start-Process -FilePath $env:ComSpec -ArgumentList @('/d','/c',"`"$launchBat`"") -WorkingDirectory $ClientDir | Out-Null
}
for ($i = 0; -not $p -and $i -lt $WaitSeconds; $i++) {
  Start-Sleep -Seconds 1
  $p = Get-Process -Name $targetName -ErrorAction SilentlyContinue | Select-Object -First 1
}
if (-not $p) { throw "在 $WaitSeconds 秒内未找到 $targetName.exe。可先启动客户端，再重新运行本脚本。" }

# 防止重复加载：模块枚举失败时继续执行，由 LoadLibrary 自身处理。
try {
  if ($p.Modules | Where-Object { $_.FileName -and ([IO.Path]::GetFullPath($_.FileName) -ieq [IO.Path]::GetFullPath($DllPath)) }) {
    Write-Output "已加载：$DllPath (PID $($p.Id))"
    exit 0
  }
} catch { }

$access = [D3DRef9Injector.Native+ProcessAccess]::CreateThread -bor
          [D3DRef9Injector.Native+ProcessAccess]::QueryInformation -bor
          [D3DRef9Injector.Native+ProcessAccess]::VMOperation -bor
          [D3DRef9Injector.Native+ProcessAccess]::VMWrite -bor
          [D3DRef9Injector.Native+ProcessAccess]::Synchronize
$hProcess = [D3DRef9Injector.Native]::OpenProcess($access, $false, $p.Id)
if ($hProcess -eq [IntPtr]::Zero) { [D3DRef9Injector.Native]::ThrowLast('OpenProcess') }
$remote = [IntPtr]::Zero
$hThread = [IntPtr]::Zero
try {
  $bytes = [Text.Encoding]::Unicode.GetBytes($DllPath + [char]0)
  $remote = [D3DRef9Injector.Native]::VirtualAllocEx($hProcess, [IntPtr]::Zero, [UIntPtr]$bytes.Length,
    ([D3DRef9Injector.Native+AllocationType]::Commit -bor [D3DRef9Injector.Native+AllocationType]::Reserve),
    [D3DRef9Injector.Native+MemoryProtection]::ReadWrite)
  if ($remote -eq [IntPtr]::Zero) { [D3DRef9Injector.Native]::ThrowLast('VirtualAllocEx') }
  [UIntPtr]$written = [UIntPtr]::Zero
  if (-not [D3DRef9Injector.Native]::WriteProcessMemory($hProcess, $remote, $bytes, [UIntPtr]$bytes.Length, [ref]$written)) {
    [D3DRef9Injector.Native]::ThrowLast('WriteProcessMemory')
  }
  $loadLibrary = [D3DRef9Injector.Native]::GetProcAddress([D3DRef9Injector.Native]::GetModuleHandle('kernel32.dll'), 'LoadLibraryW')
  if ($loadLibrary -eq [IntPtr]::Zero) { [D3DRef9Injector.Native]::ThrowLast('GetProcAddress(LoadLibraryW)') }
  [uint32]$tid = 0
  $hThread = [D3DRef9Injector.Native]::CreateRemoteThread($hProcess, [IntPtr]::Zero, [UIntPtr]::Zero, $loadLibrary, $remote, 0, [ref]$tid)
  if ($hThread -eq [IntPtr]::Zero) { [D3DRef9Injector.Native]::ThrowLast('CreateRemoteThread') }
  [void][D3DRef9Injector.Native]::WaitForSingleObject($hThread, 10000)
  [uint32]$module = 0
  if (-not [D3DRef9Injector.Native]::GetExitCodeThread($hThread, [ref]$module) -or $module -eq 0) {
    throw "LoadLibraryW 在 PID $($p.Id) 中返回失败。"
  }
  Write-Output "已注入：$DllPath"
  Write-Output "目标：$($p.Path) (PID $($p.Id))"
  Write-Output '加载后按 Home 显示面板。'
}
finally {
  if ($hThread -ne [IntPtr]::Zero) { [void][D3DRef9Injector.Native]::CloseHandle($hThread) }
  if ($remote -ne [IntPtr]::Zero) { [void][D3DRef9Injector.Native]::VirtualFreeEx($hProcess, $remote, [UIntPtr]::Zero, [D3DRef9Injector.Native+AllocationType]::Release) }
  if ($hProcess -ne [IntPtr]::Zero) { [void][D3DRef9Injector.Native]::CloseHandle($hProcess) }
}
