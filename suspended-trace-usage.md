# 最早阶段 D3D9 追踪启动

当前代理会转发原 DLL，所以游戏表现仍是原版功能；需要在进程恢复前加载追踪器，才能避免错过 `Direct3DCreate9/CreateDevice`。

```powershell
& 'C:\Users\15135\Documents\Codex\2026-08-31\jian\outputs\d3dref9自治\tools\launch_suspended_d3d9_trace.ps1'
```

该脚本从 `A点我启动游戏.bat` 读取启动参数，创建挂起的 `crossfire.exe`，先注入 `d3d9_early_trace.dll`，再恢复主线程。进入对局后查看：

```powershell
Get-Content 'C:\Windows\Temp\d3d9_early_trace.log' -Tail 80
```

只关注新 PID 对应的 `Direct3DCreate9`、`CreateDevice` 行。完成追踪后恢复：

```powershell
& 'C:\Users\15135\Documents\Codex\2026-08-31\jian\outputs\d3dref9自治\tools\restore_original_d3dref9.ps1' -ClientDir 'E:\game\已加速- CF2.0搭建（使用2012系统）\客户端\10.4CrossFire'
```
