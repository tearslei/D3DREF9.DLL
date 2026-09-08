#Requires -Version 5.1
[CmdletBinding(SupportsShouldProcess)]
param(
  [string]$ClientDir = 'E:\game\已加速- CF2.0搭建（使用2012系统）\客户端\10.4CrossFire',
  [string]$Shell,
  [switch]$Apply
)
$ErrorActionPreference = 'Stop'
if ([string]::IsNullOrWhiteSpace($Shell)) { $Shell = Join-Path $PSScriptRoot 'D3DREF9.DLL' }
$target = Join-Path $ClientDir 'D3DREF9.DLL'
$backup = Join-Path $ClientDir 'D3DREF9.before-ordinal1.bak'
$configSource = Join-Path $PSScriptRoot 'config\d3dref9自治.ini'
$configDir = Join-Path $ClientDir 'config'
$configTarget = Join-Path $configDir 'd3dref9自治.ini'
$configBackup = Join-Path $configDir 'd3dref9自治.before-current.bak'

if (-not (Test-Path -LiteralPath $Shell -PathType Leaf)) { throw "Shell not found: $Shell" }
if (-not (Test-Path -LiteralPath $ClientDir -PathType Container)) { throw "ClientDir not found: $ClientDir" }
if (-not (Test-Path -LiteralPath $configSource -PathType Leaf)) { throw "Config not found: $configSource" }
$info = & python (Join-Path $PSScriptRoot 'tools\inspect_pe_exports.py') $Shell
$infoText = $info -join [Environment]::NewLine
if ($LASTEXITCODE -ne 0 -or ($infoText -notmatch 'Machine=0x014C') -or ($infoText -notmatch 'Functions=1 Names=0') -or ($infoText -notmatch 'ordinal 1:')) {
  throw "The shell failed the ordinal-1 PE check.`n$($info -join [Environment]::NewLine)"
}

if (-not $Apply) {
  Write-Output "预览模式："
  Write-Output "源文件：$Shell"
  Write-Output "目标文件：$target"
  Write-Output "回滚备份：$backup"
  Write-Output "配置源：$configSource"
  Write-Output "配置目标：$configTarget"
  Write-Output '确认客户端已退出后追加 -Apply 执行。'
  exit 0
}

if (Get-Process -Name crossfire -ErrorAction SilentlyContinue) { throw 'crossfire.exe is running; exit it before deployment.' }

if ($PSCmdlet.ShouldProcess($target, 'Deploy ordinal-1 shell')) {
  # Keep the first/original backup immutable across repeated builds.  If the
  # user already stored D3DREF9wg.DLL one directory above, prefer that copy.
  if (-not (Test-Path -LiteralPath $backup)) {
    $userBackup = Join-Path (Split-Path -Parent $ClientDir) 'D3DREF9wg.DLL'
    if (Test-Path -LiteralPath $userBackup) { Copy-Item -LiteralPath $userBackup -Destination $backup -Force }
    elseif (Test-Path -LiteralPath $target) { Copy-Item -LiteralPath $target -Destination $backup -Force }
  }
  Copy-Item -LiteralPath $Shell -Destination $target -Force
  New-Item -ItemType Directory -Path $configDir -Force | Out-Null
  if ((Test-Path -LiteralPath $configTarget) -and -not (Test-Path -LiteralPath $configBackup)) { Copy-Item -LiteralPath $configTarget -Destination $configBackup -Force }
  Copy-Item -LiteralPath $configSource -Destination $configTarget -Force
  Write-Output "DEPLOYED=$target"
  Write-Output "BACKUP=$backup"
  Write-Output "CONFIG_DEPLOYED=$configTarget"
  Write-Output "CONFIG_BACKUP=$configBackup"
}
