# 当前加载状态说明

> **2026-09-02 更新**：当前客户端已改为直接加载本工程生成的
> `D3DREF9.DLL`（仅 ordinal 1），不再使用下文旧的
> `D3DREF9.original.dll`/`d3d9_early_trace.dll` 代理链。下文历史记录仅供追溯。

当前运行时观测：PID `27512` 仅加载客户端目录中的 `D3DREF9.DLL`
（基址 `0x60E50000`，大小 `102400`）；已出现 `installed`、`font_hook_installed`、
`device_captured` 和每秒 `heartbeat`。大厅状态没有实体坐标样本，进入地图后再读取
`%TEMP%\d3dref9_entities.csv`。

当前临时追踪代理不是干净版处理器 DLL。它的设计是：

```text
D3DREF9.DLL (代理，SHA256 E30F...)
  ├─ ordinal 1 → D3DREF9.original.dll (原外挂，SHA256 E73F...)
  └─ LoadLibrary → d3d9_early_trace.dll
```

因此进程中同时看到 `D3DREF9.DLL` 和 `D3DREF9.original.dll` 是正常的；实际游戏功能仍由原 DLL 提供，表现为“旧版”是预期结果。这个代理只用于记录 D3D9 创建链，不包含实体、透视、自瞄等游戏处理器。

用下面命令可输出进程实际加载路径、基址、大小和 SHA-256（避免 PowerShell 的 `$PID` 只读变量）：

```powershell
& 'C:\Users\15135\Documents\Codex\2026-08-31\jian\outputs\d3dref9自治\tools\report_active_d3dref9.ps1'
```

当前追踪日志位置：

```text
C:\Windows\Temp\d3d9_early_trace.log
C:\Users\15135\AppData\Local\Temp\d3dref9_early_proxy.log
```

目前普通代理启动已经成功加载追踪器并安装 `Direct3DCreate9` 钩子，但尚未出现 `CreateDevice` 行。这说明设备创建发生在追踪器加载前，或客户端使用了另一个创建入口（例如 `Direct3DCreate9Ex`）。新增 `launch_suspended_d3d9_trace.ps1` 会在主线程恢复前注入追踪器；追踪器自身保留后台重试，以覆盖 `d3d9.dll` 稍后加载的情况。下一步需要使用该启动方式采到创建阶段，再定位现有设备的虚表。

实际验证表明，该客户端还会把“进程启动前注入的额外 DLL”判定为客户端损坏，即使磁盘上的 `D3DREF9.DLL` 哈希完全正确。因此 `launch_suspended_d3d9_trace.ps1` 在此客户端上不适合继续使用；应改用正常启动后的只读采样（`runtime_patch_capture.py` / `auto_entity_timeline.py`），或在隔离副本中做调试追踪。
