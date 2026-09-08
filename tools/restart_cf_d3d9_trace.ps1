#Requires -Version 5.1
[CmdletBinding()]
param([string]$ClientDir)
$ErrorActionPreference = 'Stop'
$toolDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$originalHash = 'E73FEC5C4692D06B136BE3D451A88BEE05921E0076E260C969F3C97C2A905A5A'
$proc = Get-Process crossfire -ErrorAction SilentlyContinue | Select-Object -First 1
if ([string]::IsNullOrWhiteSpace($ClientDir)) {
  $candidate = Get-ChildItem -LiteralPath 'E:\game' -Filter 'D3DREF9.original.dll' -File -Recurse -ErrorAction SilentlyContinue |
    Where-Object { (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash -eq $originalHash } | Select-Object -First 1
  if (-not $candidate) {
    $candidate = Get-ChildItem -LiteralPath 'E:\game' -Filter 'D3DREF9.DLL' -File -Recurse -ErrorAction SilentlyContinue |
      Where-Object { (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash -eq $originalHash -and (Test-Path -LiteralPath (Join-Path $_.DirectoryName 'crossfire.exe')) } | Select-Object -First 1
  }
  if ($candidate) { $ClientDir = Split-Path -Parent $candidate.FullName }
}
if ([string]::IsNullOrWhiteSpace($ClientDir)) { throw 'ClientDir was not supplied and crossfire.exe could not be located under E:\game.' }
if ($proc) { $proc | Stop-Process -Force }
Start-Sleep -Milliseconds 800
& (Join-Path $toolDir 'deploy_d3d9_early_trace.ps1') -ClientDir $ClientDir
$bat = Get-ChildItem -LiteralPath $ClientDir -Filter '*.bat' -File -ErrorAction SilentlyContinue |
  Where-Object { $_.Name -match '启动|start|launch|点我' } | Select-Object -First 1
if (-not $bat) { $bat = Get-ChildItem -LiteralPath $ClientDir -Filter '*.bat' -File -ErrorAction SilentlyContinue | Select-Object -First 1 }
if (-not $bat) { throw "No launcher .bat found in $ClientDir" }
Start-Process -FilePath $env:ComSpec -ArgumentList @('/d','/c',"`"$($bat.FullName)`"") -WorkingDirectory $ClientDir
Write-Output 'CF started with the early D3D9 trace proxy. Join a match, then report back.'
