# 实际对局实体/矩阵采样状态

目标：`crossfire.exe` PID `2328`，版本 `1.1.85.7`。

## 已完成

- 已检测到运行中的 `crossfire.exe`、`cshell.dll`、`D3DREF9.DLL`。
- 已保存对局基线：`runtime-diffs/match-baseline/baseline.json`。
- 已对 `crossfire.exe` 和 `cshell.dll` 扫描浮点矩阵候选。
- 已对 `cshell.dll+0x1E8E478` 附近的动态坐标样本连续采样 30 次；该结构包含稳定的三维坐标样式：

```text
cshell.dll+0x1E8E498 ≈ (2008.559, -2356.011, 865.158)
```

坐标随视角/对象状态改变，`w=1.0`，但尚未证明这是实体列表或骨骼数组。

## 当前证据边界

单个坐标样式不能直接推出实体结构；扫描得到的矩阵候选数量较多，必须使用移动目标、多个对象和重复指针链进行交叉验证。当前没有可靠实体步长、阵营偏移、生命值偏移或骨骼索引，因此尚未接入真实自瞄/透视处理器。

样本文件：

- `runtime-diffs/match-baseline/matrix-candidates.json`
- `runtime-diffs/match-baseline/dynamic-matrix-candidates.json`
- `runtime-diffs/match-baseline/cshell_candidate_1e8e478.json`

## 2026-08-31 实际对局连续采样（PID 2328）

- 重新建立基线：`runtime-diffs/match-baseline/baseline.json`，模块版本仍为
  `crossfire.exe/cshell.dll 1.1.85.7`，D3DREF9 开关均为关闭状态。
- 运行 `tools/scan_dynamic_matrices.py`，得到 50 个动态矩阵候选；其中
  `crossfire.exe+0xDB90C4` 一带随视角/位置连续变化，属于相机/投影候选，
  不能直接当作实体数组。
- 新增三帧向量扫描 `tools/scan_dynamic_vectors.py`：`3584` 个变化向量、
  `79` 个窗口；大部分集中在 `crossfire.exe+0xDC2800` 连续渲染数据区。
- 新增地图坐标聚类 `tools/scan_position_clusters.py`：只留下 9 条平滑变化
  候选，主要位于 `cshell.dll+0x1E71000`、`+0x16A53E0` 等相机/变换副本。
- 运行 120 秒连续监控 `tools/live_match_monitor.py`，记录 1184 个样本：
  `runtime-diffs/match-baseline/live-monitor.csv`。`crossfire.exe+0xDB90C4`
  的三维值在采样期间发生大范围变化，证明当前对局有效且相机在更新；
  D3DREF9 全局开关保持全 0，未因采样器产生改动。

### 当前结论

这轮已经完成“实际对局→基线→动态采样→恢复状态”的闭环，但仍未得到可
唯一归因的实体步长、阵营/生命偏移或骨骼指针链。下一轮必须在可见目标移动、
跳跃或死亡的时间窗内做定点采样，并将目标运动轨迹与候选地址一一对应；
仅靠静态矩阵样式会把相机和渲染缓存误判为实体。

## 目标时间窗采样（本轮）

- 三帧动态向量扫描输出 `dynamic-vectors.json`，仅 3 个异常变化项，集中在
  `cshell.dll+0x1EAE830`，表现为一次性 `0→-70180`，不符合实体坐标范围，
  已排除为实体候选。
- 对候选位置连续监控 60 秒，输出 `entity-candidates.csv`（595 样本）。
  `cshell.dll+0x1E71000` 的三元组持续变化；相邻记录 `+0x1E71BA4` 与
  `+0x1E71BB0` 也随场景变化，但没有稳定的重复步长和可识别生命/阵营字段。
- 当前仍只能把 `+0x1E71000` 标记为“变换/坐标候选”，不能标记为实体列表头。

## 自动采样结果

已启动无需回车的 `tools/auto_entity_timeline.py`，自动运行 180 秒并记录
1785 个样本至 `runtime-diffs/match-baseline/auto-entity-timeline.csv`。
本轮候选地址值保持静态，说明目标动作没有映射到当前候选，或目标/对局状态
尚未触发这些结构更新。已补充替代采集说明：`alternative-collection-methods.md`。

## 第二次自动采样与反向指针扫描（2026-08-31）

- 当前 `auto-entity-timeline.csv` 已确认是第二次运行的完整 180 秒结果：PID `2328`，1777 个有效样本，时间跨度 `179.918s`，中位采样间隔 `0.1007s`。
- 本轮只采集了脚本指定的 12 个固定 RVA（各四个 float）及 40 个 D3DREF9 状态位；原 DLL 状态变量在整个窗口内均未变化，故本轮没有开关事件样本。
- 对 `cshell.dll+0x1E71000`（运行期 `0x11EC1000`）执行了只读的全进程 32-bit 反向指针扫描：3946 个可读已提交区、2,571,096,064 字节、0 个精确直接指针命中。这排除了它作为普通对象指针目标的最直接形式，更符合直接全局变换/渲染缓存的特征。
- 为避免后续运行覆盖历史，`auto_entity_timeline.py` 已改为每轮生成带 PID/时间戳的归档 CSV，同时继续更新 `auto-entity-timeline.csv` 为最新结果；本轮原始文件已备份为 `runtime-diffs/match-baseline/auto-entity-timeline-pid2328-1788191166-1788191346.csv`。

分析报告：`outputs/d3dref9自治/auto-entity-timeline-analysis.md`；反向扫描：`runtime-diffs/match-baseline/reverse-pointers.json`。
