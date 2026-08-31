#Requires -Version 5.1
[CmdletBinding(SupportsShouldProcess)]
param(
  [string]$ClientDir = 'E:\game\已加速- CF2.0搭建（使用2012系统）\客户端\10.4CrossFire',
  [string]$Candidate = (Join-Path $PSScriptRoot 'D3DREF9_精简候选.dll'),
  [switch]$Apply
)
$ErrorActionPreference='Stop'
$target=Join-Path $ClientDir 'D3DREF9.DLL'
$backup=Join-Path $ClientDir 'D3DREF9.DLL.original.bak'
if(-not (Test-Path -LiteralPath $Candidate)){throw "找不到候选 DLL：$Candidate"}
if(-not (Test-Path -LiteralPath $ClientDir)){throw "客户端目录不存在：$ClientDir"}
if(Get-Process crossfire -ErrorAction SilentlyContinue){throw 'crossfire.exe 仍在运行；退出游戏后再部署。'}
if(-not $Apply){
  Write-Output "预览模式："
  Write-Output "候选：$Candidate"
  Write-Output "目标：$target"
  Write-Output "备份：$backup"
  Write-Output '确认无误后追加 -Apply 执行。'
  exit 0
}
if(Test-Path -LiteralPath $target){Copy-Item -LiteralPath $target -Destination $backup -Force}
Copy-Item -LiteralPath $Candidate -Destination $target -Force
Write-Output "已部署精简候选：$target"
Write-Output "原文件备份：$backup"
