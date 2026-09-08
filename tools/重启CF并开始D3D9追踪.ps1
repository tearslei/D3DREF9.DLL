#Requires -Version 5.1
[CmdletBinding()]
param(
  [string]$ClientDir = 'E:\game\已加速- CF2.0搭建（使用2012系统）\客户端\10.4CrossFire'
)
$ErrorActionPreference = 'Stop'
$toolDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# The D3D9 factory has already run in an existing process.  Restarting is
# deliberate here: it puts the recorder in place before the first factory call.
Get-Process crossfire -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 800
& (Join-Path $toolDir 'deploy_d3d9_early_trace.ps1') -ClientDir $ClientDir
$bat = Join-Path $ClientDir 'A点我启动游戏.bat'
if (-not (Test-Path -LiteralPath $bat)) { throw "找不到启动脚本：$bat" }
Start-Process -FilePath $env:ComSpec -ArgumentList @('/d','/c',"`"$bat`"") -WorkingDirectory $ClientDir
Write-Output 'CF 已通过早期记录代理启动。进入实际对局后运行：'
Write-Output "Get-Content 'C:\Windows\Temp\d3d9_early_trace.log' -Tail 50"
