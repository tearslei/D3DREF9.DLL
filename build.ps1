#Requires -Version 5.1
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $MyInvocation.MyCommand.Path
$cmake=Get-Command cmake -ErrorAction SilentlyContinue
if($cmake){
  & $cmake.Source -S $root -B (Join-Path $root 'build') -A Win32
  & $cmake.Source --build (Join-Path $root 'build') --config Release
} else {
  $gxx=Get-Command i686-w64-mingw32-g++.exe -ErrorAction SilentlyContinue
  if(-not $gxx){
    $pkg=Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages" -Filter 'i686-w64-mingw32-g++.exe' -Recurse -ErrorAction SilentlyContinue | Select-Object -First 1
    if($pkg){$gxx=$pkg}
  }
  if(-not $gxx){ throw '未找到 cmake 或 i686-w64-mingw32-g++；请安装 CMake/Visual Studio Build Tools 或 LLVM-MinGW。' }
  $src=Get-ChildItem (Join-Path $root 'src\*.cpp') | ForEach-Object FullName
  $compiler=if($gxx.Source){$gxx.Source}else{$gxx.FullName}
  & $compiler -std=c++17 -shared -O2 -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN "-I$(Join-Path $root 'include')" @src -o (Join-Path $root 'd3dref9自治.dll') -luser32 -lgdi32 -static-libgcc -static-libstdc++
}
$built=Get-ChildItem (Join-Path $root 'build') -Filter 'd3dref9自治.dll' -Recurse | Select-Object -First 1
if(-not $built -and (Test-Path (Join-Path $root 'd3dref9自治.dll'))){$built=Get-Item (Join-Path $root 'd3dref9自治.dll')}
if($built){Write-Output "已生成: $($built.FullName)"}
