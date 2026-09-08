# CF 实际对局只读采样阶段报告（2026-09-01）

## 运行目标

- 进程：`crossfire.exe`，PID `15552`
- 客户端：x86；`crossfire.exe/cshell.dll` 文件版本记录为 `1.1.85.7`
- 采样原则：只读 `OpenProcess(PROCESS_QUERY_INFORMATION|PROCESS_VM_READ)`；不写游戏内存、不发按键、不注入 DLL。

## 已执行采样

| 采样 | 时长/轮次 | 结果 |
|---|---:|---|
| 固定 RVA 时间线 | 299.965 s，2977 行，间隔中位数 0.1008 s | 30/48 个浮点列发生变化；D3DREF9 的 40 个状态列本轮无变化 |
| 动态矩阵候选 | 两次快照间隔 1 s | 33 个候选（`crossfire.exe` 32，`cshell.dll` 1） |
| 坐标簇候选 | 三次快照、0.4 s 间隔 | 60 行，9 个窗口；最大簇为 `crossfire.exe+0xDC0000`（24 项） |
| 堆区 vec 候选 | 60 s，59 轮 | 2067 个可读区；限制后 5000 个候选，前 500 个均有变化 |
| D3D9 设备指针 | 扫描约 1.33 GB 可读提交区 | 未发现同时满足虚表校验的 `IDirect3DDevice9` 候选 |

## 重要输出

- `runtime-diffs/match-baseline/auto-entity-timeline-pid15552-1788287153.csv`
- `runtime-diffs/match-baseline/latest-entity-sample-quality.json`
- `runtime-diffs/match-baseline/dynamic-matrix-candidates.json`
- `runtime-diffs/match-baseline/position-clusters.json`
- `runtime-diffs/match-baseline/heap-entity-probe-60s.json`
- `runtime-diffs/match-baseline/d3d9-device-candidates.json`
- `runtime-diffs/match-baseline/tcii-runtime-offsets.json`
- `runtime-diffs/match-baseline/tcii-runtime-sections.json`

## 当前可确认事实

1. 当前 PID 在采样期间持续有效，模块基址为：
   - `crossfire.exe = 0x00400000`
   - `cshell.dll = 0x10050000`
   - `D3DREF9.DLL = 0x16E30000`（本次模块枚举记录）
2. 已知时间线中的 `crossfire.exe+0xDB90C4`、`+0xDBFE94`、`+0xDC13E0` 等区域在对局中持续变化，表现出相机/变换候选的数值特征。
3. `cshell.dll` 源码容器中的十六进制 RVA 文本可以映射到当前映像地址。例如 `0x134DE00 → 0x1139DE00`，`0x16A5648 → 0x116F5648`。映射结果已经逐项保存，但这些 RVA 仍未证明为实体、血量、骨骼或处理器入口。
4. D3D9 设备扫描为 0，不代表游戏没有使用 D3D9；当前设备指针可能位于不可读/短生命周期区域，或虚表不在已扫描的模块 `.rdata/.data` 范围。

## 尚不能下的结论

- 不能把任意浮点三元组直接命名为实体坐标、骨骼或世界矩阵。
- 不能把 `crossfire.exe+0xDC0000` 簇直接接入自瞄/透视处理器。
- 本轮没有获得稳定的实体 Root/Count/Stride、骨骼层级、投影矩阵或 D3D9 Draw 调用关联。
- 不能据此宣称现有干净 DLL 的透视、自瞄、自动开枪等处理器已可用。

## 下一轮测试方式（无需严格动作窗口）

保持同一对局或下一局运行，直接启动以下只读脚本即可；脚本会自动开始/结束，不会操作游戏：

```powershell
cd 'C:\Users\15135\Documents\Codex\2026-08-31\jian'
$cfPid = (Get-Process -Name crossfire).Id
$py = 'D:\Programs\Python\Python313\python.exe'
& $py .\outputs\d3dref9自治\tools\auto_entity_timeline.py --pid $cfPid --duration 600 --interval 0.1
& $py .\outputs\d3dref9自治\tools\heap_entity_probe.py --pid $cfPid --duration 120 --interval 1 --max-candidates 10000
& $py .\outputs\d3dref9自治\tools\scan_dynamic_matrices.py
& $py .\outputs\d3dref9自治\tools\scan_position_clusters.py
& $py .\outputs\d3dref9自治\tools\analyze_tcii_runtime_offsets.py --pid $cfPid
```

如果只做一次最有价值的测试，优先保留 `auto_entity_timeline.py` 的 10 分钟结果，并在过程中正常转动视角、移动、开枪、换弹、进入/退出地图；不需要按固定秒数切换。

## 证据等级

- 固定 RVA 连续变化：L1（持续变化）
- 坐标/矩阵候选：L1-L2（需相机、生命周期和渲染交叉验证）
- 实体列表/骨骼/自瞄目标筛选：L0（本轮尚无闭环证据）
- D3D9 绘制入口：L0（设备指针候选为 0）
