# F7+6 人物透视运行时差分（修正版）

- PID: `12648`
- 采样文件：`C:\Users\15135\Documents\Codex\2026-08-31\jian\runtime-diffs\diff_12648_1788176160.json`
- 总变化段：`262 段`

## 已直接验证

- `d3dref9.dll+0x1A8D58`：`00 → 01`。运行时模块基址为 `0x16FB0000`，因此本次绝对地址为 `0x17158D58`。这与静态 `[0x101A8D58]` 对应关系一致。
- 因目标 PE 使用异常/混淆节名，执行属性应按 `IMAGE_SCN_MEM_EXECUTE (0x20000000)` 判断；按正确标志重新分类后，本次 `crossfire.exe` 183 段、`cshell.dll` 35 段均位于可执行映射区。
- `d3dref9.dll` 的其余变化位于 `.data`，主要是 UI/对象运行时状态。

## 代表性代码区变化

### crossfire.exe
- `crossfire.exe+0xdb071c`：`20130a` → `a03710`
- `crossfire.exe+0xdbce7c`：`6802` → `580d`
- `crossfire.exe+0xdbd180`：`4892` → `f097`
- `crossfire.exe+0xdc7ca0`：`307c89c36a6d9443f161833e` → `3a2b88c3891e90c264171e3f`
- `crossfire.exe+0xdc7cc4`：`8eafe142ade38a43556729` → `ea1925c3a0ed4d43699a1a`
- `crossfire.exe+0xdc7ce8`：`e894bb42ff10dcc2467f0e3f` → `5cc9e1425959c542d9a670c0`
- `crossfire.exe+0xdc7d0c`：`da0293c30beab5c265735d40` → `8004b540385033c35b1a3abf`
- `crossfire.exe+0xdc7d30`：`43420fc310b931c37ed132c1` → `5de6bdc2d38f6643ad4e1bc0`
- `crossfire.exe+0xdc7d4b`：`1b` → `19`
- `crossfire.exe+0xdc7d54`：`04ceebc31accf141b8a523c1` → `22fde8426aa01b423f4925c0`
- `crossfire.exe+0xdc7d6f`：`1b` → `19`
- `crossfire.exe+0xdc7d78`：`56d783c3fe47b343303567c1` → `974b96c29b862cc3b859173f`
- `crossfire.exe+0xdc7d93`：`1b` → `19`
- `crossfire.exe+0xdc7d9c`：`32568142699d1643f66076c1` → `c64b8fc3e908994100443f3f`
- `crossfire.exe+0xdc7db7`：`1b` → `19`
- `crossfire.exe+0xdc7dc0`：`162838c38fffb64320f10b` → `7c7613c3773811c32cf3c1`
- `crossfire.exe+0xdc7ddb`：`15` → `14`
- `crossfire.exe+0xdc7de4`：`500403c20072f03e354dc63f` → `62465cc392a2bd423405d9bf`
- `crossfire.exe+0xdc7dff`：`15` → `14`
- `crossfire.exe+0xdc7e08`：`c922c7c3fae816c39ed1b340` → `b8119e415f9d2743711485c0`
- 该模块本次可执行映射变化总数：`183`。
### cshell.dll
- `cshell.dll+0x1621f5b`：`e0a330` → `209e27`
- `cshell.dll+0x1666ce4`：`07f2b4` → `b7e5b8`
- `cshell.dll+0x166ad24`：`0a7b42` → `fa823e`
- `cshell.dll+0x16e27f8`：`8054df43` → `de693044`
- `cshell.dll+0x16f27fc`：`00000000` → `67916d3d`
- `cshell.dll+0x16f2c88`：`17030000334b00009062` → `b300000063950000c0ac`
- `cshell.dll+0x16f3a28`：`07f2b4` → `b7e5b8`
- `cshell.dll+0x16f3a30`：`2400bb47350000005701` → `a681bb47300000000202`
- `cshell.dll+0x16f3a48`：`02f2b4` → `b2e5b8`
- `cshell.dll+0x1742f88`：`21feffff21fe` → `31fdffff31fd`
- `cshell.dll+0x1743858`：`cfedb4052beeb4050777b4` → `9ce5b805e3e1b805c520b8`
- `cshell.dll+0x1ad4e20`：`a401000068010000a40100006801` → `9402000058020000940200005802`
- `cshell.dll+0x1ca3ba1`：`72624290b56542280166` → `01664290306b4200576b`
- `cshell.dll+0x1e8029c`：`90e8b4` → `d5d5b8`
- `cshell.dll+0x1e802b1`：`020000d3edb4` → `040000a0e5b8`
- `cshell.dll+0x1e8035c`：`000000` → `202020`
- `cshell.dll+0x1ea8e70`：`6c4633ecde` → `e1339286df`
- `cshell.dll+0x1ea8ffb`：`00fd5f` → `402d70`
- `cshell.dll+0x1eac35e`：`f3` → `f2`
- `cshell.dll+0x1ead07a`：`e7` → `e6`
- 该模块本次可执行映射变化总数：`35`。

## 证据边界

本次结果证明 F7+6 已触发原 DLL 的开关，并且游戏映射区发生了运行时变化；但这些变化来自约 6 分钟间隔的全页快照，包含游戏自身动画、网络、解密/自修改和场景更新，不能单独归因于人物透视。当前没有可靠证据能把某一段字节唯一标记为 ESP 补丁。

要完成“处理器 → 目标地址/补丁字节”，需要在同一帧窗口内做短间隔基线/切换，或对 `VirtualProtect`、`WriteProcessMemory` 以及 d3dref9 的绘制/读取循环设置断点并记录调用栈。