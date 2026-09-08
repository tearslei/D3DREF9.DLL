#Requires -Version 5.1
[CmdletBinding()]
param([string]$ClientDir)
$ErrorActionPreference='Stop'
if ([string]::IsNullOrWhiteSpace($ClientDir)) {
  $ClientDir = 'E:\game\' + ([char]0x5DF2)+([char]0x52A0)+([char]0x901F) + '- CF2.0' + ([char]0x642D)+([char]0x5EFA) + ([char]0xFF08)+([char]0x4F7F)+([char]0x7528) + '2012' + ([char]0x7CFB)+([char]0x7EDF)+([char]0xFF09) + '\' + ([char]0x5BA2)+([char]0x6237)+([char]0x7AEF) + '\10.4CrossFire'
}
$files=@('crossfire.exe','cshell.dll','D3DREF9.DLL')
foreach($n in $files){
  $p=Join-Path $ClientDir $n
  if(-not(Test-Path -LiteralPath $p -PathType Leaf)){ Write-Output "MISSING=$p"; continue }
  $i=Get-Item -LiteralPath $p
  $h=(Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash
  $v=(Get-Command $p -ErrorAction SilentlyContinue)
  Write-Output ("FILE={0} SIZE={1} SHA256={2}" -f $p,$i.Length,$h)
}
$proc=Get-Process -Name crossfire -ErrorAction SilentlyContinue
if($proc){
  Write-Output "PID=$($proc.Id)"
  try {
    foreach($m in $proc.Modules | Where-Object {$_.ModuleName -in @('crossfire.exe','cshell.dll','D3DREF9.DLL')}){
      Write-Output ("MODULE={0} BASE=0x{1:X8} SIZE=0x{2:X}" -f $m.ModuleName,$m.BaseAddress.ToInt64(),$m.ModuleMemorySize)
    }
  } catch { Write-Output "MODULE_ENUM_ERROR=$($_.Exception.Message)" }
}
Write-Output 'RVA_NOTE=client RVA = module base + configured RVA; inspect config/client_1.1.85.7.json'
