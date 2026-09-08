# 干净版 DLL 处理器适配表

客户端：`crossfire.exe/cshell.dll 1.1.85.7`（Win32/x86）

该表区分“已验证的 D3DREF9 状态变量”和“尚未定位的游戏处理器”。

- `state verified`：热键切换和值恢复已在运行时验证。
- `processor unresolved`：实体、骨骼、矩阵、武器或碰撞处理器尚未从 `cshell.dll` 定位。
- `patch_bytes.status=not observed`：切换期间未观察到 `VirtualProtect/WriteProcessMemory`，说明原 DLL 主要依赖内部状态分支。

机器可读版本：`feature-adapter-table.json`。
