#Requires -Version 5.1
[CmdletBinding(SupportsShouldProcess)]
param([string]$ClientDir)
$ErrorActionPreference = 'Stop'
$originalHash = 'E73FEC5C4692D06B136BE3D451A88BEE05921E0076E260C969F3C97C2A905A5A'
if ([string]::IsNullOrWhiteSpace($ClientDir)) {
  $candidate = Get-ChildItem -LiteralPath 'E:\game' -Filter 'D3DREF9.DLL' -File -Recurse -ErrorAction SilentlyContinue |
    Where-Object { (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash -eq $originalHash -and (Test-Path -LiteralPath (Join-Path $_.DirectoryName 'crossfire.exe')) } | Select-Object -First 1
  if ($candidate) { $ClientDir = Split-Path -Parent $candidate.FullName }
}
if ([string]::IsNullOrWhiteSpace($ClientDir)) { throw 'ClientDir was not supplied and crossfire.exe could not be located under E:\game.' }
$live = Join-Path $ClientDir 'D3DREF9.DLL'
# Deployment scripts use the explicit ordinal-1 suffix.  Accept the legacy
# names as fallbacks so rollback remains idempotent across earlier runs.
$savedCandidates = @(
  (Join-Path $ClientDir 'D3DREF9.before-ordinal1.bak'),
  (Join-Path $ClientDir 'D3DREF9.original.dll'),
  (Join-Path (Split-Path -Parent $ClientDir) 'D3DREF9wg.DLL')
)
$saved = $savedCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
$sidecar = Join-Path $ClientDir 'd3d9_early_trace.dll'
if (Get-Process crossfire -ErrorAction SilentlyContinue) { throw 'crossfire.exe is running; exit it before restore.' }
if (-not $saved) { throw "Original backup not found. Checked: $($savedCandidates -join '; ')" }
if ($PSCmdlet.ShouldProcess($ClientDir, 'Restore original D3DREF9')) {
  if (Test-Path -LiteralPath $live) { Remove-Item -LiteralPath $live -Force }
  Copy-Item -LiteralPath $saved -Destination $live -Force
  Remove-Item -LiteralPath $sidecar -Force -ErrorAction SilentlyContinue
  Write-Output "RESTORED=$live"
}
