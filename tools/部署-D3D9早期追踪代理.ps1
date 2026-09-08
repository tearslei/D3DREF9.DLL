#Requires -Version 5.1
[CmdletBinding(SupportsShouldProcess)]
param(
  [string]$ClientDir = 'E:\game\已加速- CF2.0搭建（使用2012系统）\客户端\10.4CrossFire'
)
$ErrorActionPreference = 'Stop'
$toolDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$proxy = Join-Path $toolDir 'D3DREF9_early_proxy.dll'
$tracer = Join-Path $toolDir 'd3d9_early_trace.dll'
$live = Join-Path $ClientDir 'D3DREF9.DLL'
$saved = Join-Path $ClientDir 'D3DREF9.original.dll'
$sidecar = Join-Path $ClientDir 'd3d9_early_trace.dll'
if (Get-Process crossfire -ErrorAction SilentlyContinue) { throw 'crossfire.exe 正在运行。先退出客户端，再部署早期追踪代理。' }
foreach ($f in @($proxy,$tracer,$live)) { if (-not (Test-Path -LiteralPath $f)) { throw "缺少文件：$f" } }
if (Test-Path -LiteralPath $saved) { throw "已存在备份：$saved`n请先运行 恢复-D3DREF9原DLL.ps1，或人工确认该备份后再继续。" }
$hash = (Get-FileHash -LiteralPath $live -Algorithm SHA256).Hash
if ($hash -ne 'E73FEC5C4692D06B136BE3D451A88BEE05921E0076E260C969F3C97C2A905A5A') { throw "原 DLL 哈希不匹配：$hash" }
if ($PSCmdlet.ShouldProcess($ClientDir, '备份原 D3DREF9.DLL 并部署仅记录型早期 D3D9 追踪代理')) {
  Move-Item -LiteralPath $live -Destination $saved
  Copy-Item -LiteralPath $proxy -Destination $live
  Copy-Item -LiteralPath $tracer -Destination $sidecar
  Remove-Item 'C:\Windows\Temp\d3d9_early_trace.log' -Force -ErrorAction SilentlyContinue
  Remove-Item 'C:\Windows\Temp\d3dref9_early_proxy.log' -Force -ErrorAction SilentlyContinue
  Write-Output "已部署：$live"
  Write-Output "原文件：$saved"
  Write-Output '现在正常启动 CF，进入对局后读取 C:\Windows\Temp\d3d9_early_trace.log。'
}
