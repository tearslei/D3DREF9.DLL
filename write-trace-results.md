# 写入 API 追踪结果

目标进程：`crossfire.exe` PID `2328`

## 追踪器

已构建并注入：

`tools/d3dref9_write_trace_new.dll`

追踪器对 `D3DREF9.DLL` 的 `GetProcAddress` IAT 做了拦截，并尝试捕获：

- `VirtualProtect`
- `WriteProcessMemory`
- `memcpy`
- `memmove`
- `RtlMoveMemory`

日志：

`C:\Windows\Temp\d3dref9_write_trace.log`

## 结果

启动后日志显示：

```text
IAT patched realGP=21B31540
scan patched VP=0 WPM=0 RtlMove=0
trace started pid=2328
```

随后触发 `F7+6`、`F7+8` 各一次开启和关闭，日志没有新增 `VP`、`WPM`、`memcpy` 或 `memmove` 记录。

## 推断

这些功能切换时没有调用目标进程写入 API，也没有在 `cshell.dll` 代码段写入补丁字节；已观察到的是 D3DREF9 内部状态变量变化，例如：

```text
F7+6 -> D3DREF9.DLL+0x1A8D58: 00→01→00
F7+8 -> D3DREF9.DLL+0x1A8D60: 00→01→00
```

因此原 DLL 更可能采用“每帧处理器读取开关变量并执行逻辑”的方式，而不是按键时修改 `cshell.dll` 指令。要还原这些处理器，应继续追踪 D3D9 绘制调用、实体读取循环和开关变量交叉引用，而不是只等待 `WriteProcessMemory`。
