#Requires -Version 5.1
[CmdletBinding()]
param([int]$TargetPid)
$ErrorActionPreference='Stop'
if (-not $TargetPid) { $TargetPid = (Get-Process crossfire -ErrorAction Stop | Select-Object -First 1 -ExpandProperty Id) }
$p = Get-Process -Id $TargetPid -ErrorAction Stop
$rows = @($p.Modules | Where-Object { $_.ModuleName -match '(?i)^d3dref9(\.original)?\.dll$|^d3d9_early_trace\.dll$' } | ForEach-Object {
  $h = if (Test-Path -LiteralPath $_.FileName) { (Get-FileHash -LiteralPath $_.FileName -Algorithm SHA256).Hash } else { 'unreadable' }
  [PSCustomObject]@{ Module=$_.ModuleName; Path=$_.FileName; Base=('0x{0:X8}' -f $_.BaseAddress.ToInt64()); Size=$_.ModuleMemorySize; SHA256=$h }
})
$rows | Format-Table -AutoSize
$out = Join-Path $PSScriptRoot '..\active-d3dref9-report.json'
@{ pid=$TargetPid; process=$p.Path; modules=$rows; generated=(Get-Date).ToString('o') } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $out -Encoding UTF8
Write-Output "Report: $out"
