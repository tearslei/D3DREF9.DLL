# 当前逆向证据矩阵

## 证据等级

|等级|含义|
|---|---|
|L0|单次观察或尚未观察|
|L1|多次重复出现|
|L2|输入变化后稳定变化|
|L3|可逆干预后结果同步变化|
|L4|生命周期一致|
|L5|独立证据交叉验证|

## 当前结论

|对象|等级|已知事实|仍缺什么|
|---|---:|---|---|
|F7+6 人物透视状态|L2|`D3DREF9+0x1A8D58` 出现 `00→01→00`|状态读取点、渲染调用链|
|F7+8 无后坐力状态|L2|`D3DREF9+0x1A8D60` 出现 `00→01→00`|武器处理器、参数和调用约定|
|固定 RVA 浮点候选|L1|180 秒连续采样且有动态变化|结构语义、指针根、生命周期|
|`crossfire+0xDC7C9C` 附近|L1|连续浮点组变化|实体/相机/缓存的区分实验|
|`crossfire+0xDD2164` 附近|L1|连续坐标样式变化|重复运行与对象生命周期|
|D3D9 绘制入口|L0|尚未得到 `CreateDevice`/设备虚表|隔离调试副本或离线渲染证据|
|实体列表、骨骼、矩阵|L0|尚无根指针、步长、索引和矩阵来源|可重复动作窗口与结构扫描|
|真实游戏处理器|L0|开关变量静态引用已找到|动态读取点、调用栈、参数|

## 结论

RTTI、断言字符串、ReClass、数据断点、调用栈和 VTable 追踪都是可用方法，但它们必须落到本客户端实际二进制和运行时证据上。当前 CF 不是已确认的 Unreal/Unity 目标，不能直接套用 `GObjects`、`Names` 或 `Il2CppDumper`；应优先按原生 x86 + Direct3D9 + 自定义对象布局处理。

当前问题不是“没有源码就绝对无法分析”，而是尚未建立：

```text
状态变量 → 读取点 → 处理器 → 游戏对象/渲染入口 → 可观察结果
```

在达到 L3/L4 之前，不把任何动态地址标记为 HP、实体、骨骼、矩阵或后坐力。

## Entity Candidate

|字段|当前值|
|---|---|
|Candidate|尚未确认|
|Evidence Level|L0|
|Root / Count / Stride|未知|
|Lifecycle|未完成出生→持续→死亡→回收窗口|
|Cross-module references|未建立|
|Observed events|当前一次采集在退场后固定候选静态|
|Confidence|低|

## Transform Candidate

|字段|当前值|
|---|---|
|Candidate|`crossfire.exe+0xDB90C4`、`cshell.dll+0x1E71000` 等仅为候选|
|Evidence Level|L1|
|Entity correlation|未证明|
|Camera correlation|部分候选随视角/场景变化|
|World/local behavior|未验证|
|Matrix layout|单帧启发式候选，不确认|
|Confidence|低|

## Render Candidate

|字段|当前值|
|---|---|
|Candidate|未确认|
|Evidence Level|L0|
|Frame correlation|未建立|
|Resource correlation|未建立|
|Object correlation|未建立|
|State correlation|仅有 F7+6 状态位，无绘制调用相关性|
|Confidence|低|

## State Handler

|Feature|State variable|Reader|Caller|Call chain|Observed side effect|Confidence|
|---|---|---|---|---|---|---|
|人物透视|`D3DREF9+0x1A8D58`|待硬件读断点|已静态定位 3 个内部引用|待读取调用链|仅确认 `00→01→00`|L2|
|无后坐力|`D3DREF9+0x1A8D60`|待硬件读断点|已静态定位 3 个内部引用|待读取调用链|仅确认 `00→01→00`|L2|
|摔不掉血|`D3DREF9+0x1A8C24`|未观察到|已静态定位 4 个内部引用|需受伤/坠落窗口|未观察到状态变化|L0|

## 下一阶段 Gate

1. **Gate A — State Reader**：运行 `tools/trace_state_reads.py`，对 `D3DREF9+0x1A8D58` 设置 DR0 读写断点，触发一次 F7+6，导出 EIP/线程时间线。
2. **Gate B — Object Lifecycle**：在真实对局保持可见目标，运行 `tools/heap_entity_probe.py`，验证 Root/Count/Stride 与出生/死亡事件。
3. **Gate C — Transform**：对通过 Gate B 的对象候选验证骨骼槽位和 4×4 世界变换。
4. **Gate D — Rendering Correlation**：在设备实际创建后重新扫描 D3D9 虚表，并将 Draw 调用时间线与 F7+6 及对象生命周期关联。
