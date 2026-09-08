#Requires -Version 5.1
[CmdletBinding()]
param(
  [string]$ClientDir = 'E:\game\已加速- CF2.0搭建（使用2012系统）\客户端\10.4CrossFire',
  [int]$WaitSeconds = 120,
  [int]$ModuleWaitSeconds = 30
)
$ErrorActionPreference = 'Stop'
# crossfire.exe is Win32.  Windows PowerShell x64 can only expose a partial
# module list for a 32-bit target, so transparently re-run this probe in the
# matching SysWOW64 host.
if ([IntPtr]::Size -ne 4 -and -not $env:D3DREF9_VERIFY_X86) {
  $ps32 = Join-Path $env:WINDIR 'SysWOW64\WindowsPowerShell\v1.0\powershell.exe'
  if (Test-Path -LiteralPath $ps32) {
    $env:D3DREF9_VERIFY_X86 = '1'
    & $ps32 -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath `
      -ClientDir $ClientDir -WaitSeconds $WaitSeconds -ModuleWaitSeconds $ModuleWaitSeconds
    exit $LASTEXITCODE
  }
}
$target = [IO.Path]::GetFullPath((Join-Path $ClientDir 'D3DREF9.DLL'))
$deadline = (Get-Date).AddSeconds($WaitSeconds)
$p = $null
while (-not $p -and (Get-Date) -lt $deadline) {
  $p = Get-Process -Name crossfire -ErrorAction SilentlyContinue | Select-Object -First 1
  if (-not $p) { Start-Sleep -Milliseconds 500 }
}
if (-not $p) { throw "在 $WaitSeconds 秒内未找到 crossfire.exe；请先启动客户端。" }
Write-Output "PID=$($p.Id)"
try {
  $m = @()
  $moduleDeadline = (Get-Date).AddSeconds($ModuleWaitSeconds)
  while (-not $m -and (Get-Date) -lt $moduleDeadline) {
    # The loader may publish crossfire.exe before D3DREF9.DLL is mapped.
    $p = Get-Process -Id $p.Id -ErrorAction Stop
    $m = @($p.Modules | Where-Object { $_.ModuleName -match '(?i)^D3DREF9\.DLL$' })
    if (-not $m) { Start-Sleep -Milliseconds 500 }
  }
  if (-not $m) { throw '进程模块列表中未找到 D3DREF9.DLL' }
  foreach ($x in $m) {
    Write-Output "MODULE=$($x.FileName) BASE=0x$('{0:X8}' -f $x.BaseAddress.ToInt64()) SIZE=$($x.ModuleMemorySize)"
    if ([IO.Path]::GetFullPath($x.FileName) -ine $target) {
      throw "加载路径不是客户端目录：$($x.FileName)"
    }
  }
} catch {
  throw "模块枚举失败或路径不匹配：$($_.Exception.Message)"
}
Write-Output 'D3DREF9 client-directory load: OK'
