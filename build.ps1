#Requires -Version 5.1
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $MyInvocation.MyCommand.Path
$cmake=Get-Command cmake -ErrorAction SilentlyContinue
if(-not $cmake){ throw '未找到 cmake；请安装 Visual Studio 2022 Build Tools（Desktop C++）或 CMake 后重试。' }
& $cmake.Source -S $root -B (Join-Path $root 'build') -A Win32
& $cmake.Source --build (Join-Path $root 'build') --config Release
$built=Get-ChildItem (Join-Path $root 'build') -Filter 'd3dref9自治.dll' -Recurse | Select-Object -First 1
if($built){Write-Output "已生成: $($built.FullName)"}
