# 实际对局采样报告（2026-08-31）

## 环境

- 进程：`crossfire.exe` PID `2328`
- 版本：`crossfire.exe/cshell.dll 1.1.85.7`
- D3DREF9 基址：运行时枚举，不使用固定绝对地址
- 采样模式：只读 `ReadProcessMemory`，未写入游戏进程，未发送热键

## 已执行

1. 重建对局基线：`runtime-diffs/match-baseline/baseline.json`
2. 动态矩阵扫描：`matrix-candidates.json`、`dynamic-matrix-candidates.json`
3. 三帧动态向量扫描：`dynamic-vectors.json`
4. 平滑地图坐标聚类：`position-clusters.json`、`world-coords.json`
5. 120 秒连续状态监控：`live-monitor.csv`（本轮 1184 行样本）
6. 单独复测 `F7+6` 与 `F7+8`，两者均为 `00→01→00`，最终状态恢复关闭

## 关键观察

`crossfire.exe+0xDB90C4` 一带包含随视角/位置连续变化的三维值，适合作为
相机/投影候选。`cshell.dll+0x1E71000`、`+0x1E8E498` 存在同源变换副本；
这些地址在当前采样中没有呈现可重复实体步长，因此尚不能接入 ESP 或自瞄。

## 未完成项

仍缺少实体列表头指针、实体步长、阵营/生命偏移、骨骼索引和渲染调用链。要
继续定位，需要在对局内让一个可见目标执行“静止→移动→跳跃/死亡”，同时由
操作者记录时间点；采样器再按时间点提取候选变化，才能排除相机和 UI 缓存。

## 目标时间窗结果

本轮在目标执行动作期间完成三帧扫描和 60 秒定点监控。`dynamic-vectors.json`
只出现 `cshell.dll+0x1EAE830` 的一次性异常值（超出地图坐标范围），已排除。
`entity-candidates.csv` 记录了 `+0x1E71000`、`+0x1E71BA4`、`+0x1E71BB0`
等候选的连续变化，但尚未观察到重复实体步长或生命/阵营字段。故实体指针链
仍未确认，干净 DLL 处理器暂不接入这些地址。

## 可复现命令

```powershell
cd 'C:\Users\15135\Documents\Codex\2026-08-31\jian'
python .\outputs\d3dref9自治\tools\capture_match_baseline.py
python .\outputs\d3dref9自治\tools\scan_dynamic_vectors.py
python .\outputs\d3dref9自治\tools\scan_position_clusters.py
python .\outputs\d3dref9自治\tools\live_match_monitor.py
# 按“静止/移动/跳跃/死亡”四个时间窗同步采样
python .\outputs\d3dref9自治\tools\capture_entity_timeline.py
```
