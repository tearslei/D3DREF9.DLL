#Requires -Version 5.1
$ErrorActionPreference = 'Stop'
$dir = Split-Path -Parent $MyInvocation.MyCommand.Path
$out = Join-Path $dir 'D3DREF9_early_proxy.dll'
$compiler = Get-Command i686-w64-mingw32-g++.exe -ErrorAction SilentlyContinue
if (-not $compiler) {
  $compiler = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Filter i686-w64-mingw32-g++.exe -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
}
if (-not $compiler) { throw '未找到 i686-w64-mingw32-g++.exe。' }
$exe = if ($compiler.Source) { $compiler.Source } else { $compiler.FullName }
& $exe -std=c++17 -shared -O2 -s (Join-Path $dir 'd3dref9_early_proxy.cpp') (Join-Path $dir 'd3dref9_early_proxy.def') -o $out -static-libgcc -static-libstdc++
if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath $out)) { throw 'early proxy build failed' }
Write-Output "Built: $out"
