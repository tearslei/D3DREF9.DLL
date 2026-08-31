@echo off
setlocal
cd /d "%~dp0"
if exist "%SystemRoot%\SysWOW64\WindowsPowerShell\v1.0\powershell.exe" (
  "%SystemRoot%\SysWOW64\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -ExecutionPolicy Bypass -File "%~dp0启动-自治DLL.ps1" -Launch
) else (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0启动-自治DLL.ps1" -Launch
)
if errorlevel 1 (
  echo.
  echo 加载失败，窗口将保留以便查看错误。
  pause
)
endlocal
