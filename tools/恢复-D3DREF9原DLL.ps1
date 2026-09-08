#Requires -Version 5.1
[CmdletBinding(SupportsShouldProcess)]
param([string]$ClientDir = 'E:\game\已加速- CF2.0搭建（使用2012系统）\客户端\10.4CrossFire')
$ErrorActionPreference = 'Stop'
$live = Join-Path $ClientDir 'D3DREF9.DLL'; $saved = Join-Path $ClientDir 'D3DREF9.original.dll'; $sidecar = Join-Path $ClientDir 'd3d9_early_trace.dll'
if (Get-Process crossfire -ErrorAction SilentlyContinue) { throw 'crossfire.exe 正在运行。先退出客户端，再恢复原 DLL。' }
if (-not (Test-Path -LiteralPath $saved)) { throw "找不到原 DLL 备份：$saved" }
if ($PSCmdlet.ShouldProcess($ClientDir, '恢复原 D3DREF9.DLL 并删除早期追踪 sidecar')) {
  Remove-Item -LiteralPath $live -Force
  Move-Item -LiteralPath $saved -Destination $live
  Remove-Item -LiteralPath $sidecar -Force -ErrorAction SilentlyContinue
  Write-Output "已恢复：$live"
}
