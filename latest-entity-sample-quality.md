# 最新实体时间线采样质量

- 源文件：`C:\Users\15135\Documents\Codex\2026-08-31\jian\runtime-diffs\match-baseline\auto-entity-timeline-pid24204-1788344815.csv`
- PID：`24204`；样本：`6154`；时长：`621.161s`；中位间隔：`0.1005s`
- 配置浮点列：`48`；发生变化：`1`
- D3DREF9 状态列：`40`；发生变化：`0`

## 结论

这份文件在采样时序和数据完整性上正确；但采样器只读取预先指定的固定 RVA，不能据此确认实体列表、骨骼、阵营、生命值或世界矩阵。动态列只是候选，不能直接接入处理器。

## 最大变化候选
- `crossfire.exe@db90c4+3` unique=2 range=0.000..1.000 max_delta=1.000
- `crossfire.exe@db90c4+0` unique=1 range=0.000..0.000 max_delta=0.000
- `crossfire.exe@db90c4+1` unique=1 range=0.000..0.000 max_delta=0.000
- `crossfire.exe@db90c4+2` unique=1 range=0.000..0.000 max_delta=0.000
- `crossfire.exe@dbfe54+0` unique=1 range=0.000..0.000 max_delta=0.000
- `crossfire.exe@dbfe54+1` unique=1 range=0.000..0.000 max_delta=0.000
- `crossfire.exe@dbfe54+2` unique=1 range=0.000..0.000 max_delta=0.000
- `crossfire.exe@dbfe54+3` unique=1 range=0.000..0.000 max_delta=0.000
- `crossfire.exe@dbfe94+0` unique=1 range=0.000..0.000 max_delta=0.000
- `crossfire.exe@dbfe94+1` unique=1 range=0.000..0.000 max_delta=0.000
- `crossfire.exe@dbfe94+2` unique=1 range=0.000..0.000 max_delta=0.000
- `crossfire.exe@dbfe94+3` unique=1 range=0.000..0.000 max_delta=0.000
- `crossfire.exe@dc13e0+0` unique=1 range=0.000..0.000 max_delta=0.000
- `crossfire.exe@dc13e0+1` unique=1 range=0.000..0.000 max_delta=0.000
- `crossfire.exe@dc13e0+2` unique=1 range=0.000..0.000 max_delta=0.000
- `crossfire.exe@dc13e0+3` unique=1 range=0.000..0.000 max_delta=0.000
- `crossfire.exe@dcbb9c+0` unique=1 range=1.000..1.000 max_delta=0.000
- `crossfire.exe@dcbb9c+1` unique=1 range=0.000..0.000 max_delta=0.000
- `crossfire.exe@dcbb9c+2` unique=1 range=1.000..1.000 max_delta=0.000
- `crossfire.exe@dcbb9c+3` unique=1 range=0.000..0.000 max_delta=0.000
