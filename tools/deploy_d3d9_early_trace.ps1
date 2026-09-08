#Requires -Version 5.1
[CmdletBinding(SupportsShouldProcess)]
param([string]$ClientDir)
$ErrorActionPreference = 'Stop'
$toolDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$originalHash = 'E73FEC5C4692D06B136BE3D451A88BEE05921E0076E260C969F3C97C2A905A5A'
if ([string]::IsNullOrWhiteSpace($ClientDir)) {
  $candidate = Get-ChildItem -LiteralPath 'E:\game' -Filter 'D3DREF9.DLL' -File -Recurse -ErrorAction SilentlyContinue |
    Where-Object { (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash -eq $originalHash -and (Test-Path -LiteralPath (Join-Path $_.DirectoryName 'crossfire.exe')) } | Select-Object -First 1
  if ($candidate) { $ClientDir = Split-Path -Parent $candidate.FullName }
}
if ([string]::IsNullOrWhiteSpace($ClientDir)) { throw 'ClientDir was not supplied and crossfire.exe could not be located under E:\game.' }
$proxy = Join-Path $toolDir 'D3DREF9_early_proxy.dll'
$tracer = Join-Path $toolDir 'd3d9_early_trace.dll'
$live = Join-Path $ClientDir 'D3DREF9.DLL'
$saved = Join-Path $ClientDir 'D3DREF9.original.dll'
$sidecar = Join-Path $ClientDir 'd3d9_early_trace.dll'
if (Get-Process crossfire -ErrorAction SilentlyContinue) { throw 'crossfire.exe is running; exit it before deployment.' }
foreach ($f in @($proxy, $tracer, $live)) { if (-not (Test-Path -LiteralPath $f)) { throw "Missing file: $f" } }
$hash = (Get-FileHash -LiteralPath $live -Algorithm SHA256).Hash
if ($hash -ne $originalHash) {
  if (-not (Test-Path -LiteralPath $saved)) { throw "Unexpected original D3DREF9 hash: $hash" }
  $savedHash = (Get-FileHash -LiteralPath $saved -Algorithm SHA256).Hash
  if ($savedHash -ne $originalHash) { throw "Backup D3DREF9 hash mismatch: $savedHash" }
}
if ($PSCmdlet.ShouldProcess($ClientDir, 'Install temporary early D3D9 trace proxy')) {
  if ($hash -eq $originalHash) { Move-Item -LiteralPath $live -Destination $saved }
  else { Remove-Item -LiteralPath $live -Force }
  Copy-Item -LiteralPath $proxy -Destination $live
  Copy-Item -LiteralPath $tracer -Destination $sidecar
  # The tracer may keep the append handle open until the old process exits;
  # preserve the log and use the new PID/timestamp lines as the run boundary.
  Write-Output "DEPLOYED=$live"
  Write-Output "BACKUP=$saved"
}
