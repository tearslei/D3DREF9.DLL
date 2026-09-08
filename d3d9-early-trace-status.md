# D3D9 早期追踪：已准备，等待下一次 CF 启动

## 已验证

- `D3DREF9_early_proxy.dll`：Win32/x86（`Machine=0x014C`、`PE32=0x010B`）。
- 代理的导出目录只有 **ordinal 1**（无名称），与原 `D3DREF9.DLL` 的导出形式一致。
- ordinal 1 用尾跳转转发至同目录备份的 `D3DREF9.original.dll` ordinal 1，不假定其参数、返回值或调用约定。
- 当前运行中客户端的 `d3d9!Direct3DCreate9` 前 16 字节为：
  `8B FF 55 8B EC 51 8B 4D 08 33 C0 50 50 89 45 FC`。
  早期追踪器保存的前 5 字节是完整指令边界（`mov edi,edi; push ebp; mov ebp,esp`），可安全安装其记录型入口。

## 此阶段做什么

临时代理在客户端加载原 `D3DREF9` 时**同步**启动记录器（先于 ordinal 1 被调用）：

```text
D3DREF9.DLL（临时代理，ordinal 1）
  → D3DREF9.original.dll（原始字节副本，ordinal 1）
  → d3d9_early_trace.dll
  → Direct3DCreate9 / CreateDevice
  → Present(17) / EndScene(42) / DrawIndexedPrimitive(82) 地址
```

它只记录 D3D9 创建设备信息；还没有改写 `FeatureManager` 的游戏处理器。

## 执行与回滚

当前客户端曾经使用旧版代理时，原 ordinal 会被转发到 `D3DREF9.original.dll`，因此游戏功能表现仍然是原外挂，这是预期行为；早期记录器还必须在 ordinal 1 之前加载。关闭并重新开始一次客户端后，运行：

```powershell
& 'C:\Users\15135\Documents\Codex\2026-08-31\jian\outputs\d3dref9自治\tools\重启CF并开始D3D9追踪.ps1'
```

进入一次实际对局后，日志应出现：

```text
CreateDevice self=... device=... vtbl=... Present17=... EndScene42=... DIP82=...
```

随后用下列脚本恢复原文件：

```powershell
& 'C:\Users\15135\Documents\Codex\2026-08-31\jian\outputs\d3dref9自治\tools\恢复-D3DREF9原DLL.ps1'
```
