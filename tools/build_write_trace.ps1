$ErrorActionPreference='Stop'
$root=Split-Path -Parent $MyInvocation.MyCommand.Path
$c=Get-Command i686-w64-mingw32-g++.exe -ErrorAction SilentlyContinue
if($c){$compiler=$c.Source}else{$x=Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Filter 'i686-w64-mingw32-g++.exe' -Recurse -ErrorAction SilentlyContinue|Select-Object -First 1;$compiler=$x.FullName}
if(-not $compiler){throw 'compiler not found'}
$out=Join-Path $root 'd3dref9_write_trace_new.dll'
& $compiler -std=c++17 -shared -O2 -DWIN32_LEAN_AND_MEAN (Join-Path $root 'write_trace.cpp') -o $out -lkernel32
if($LASTEXITCODE -ne 0){throw "compiler failed: $LASTEXITCODE"}
Write-Output $out
