# D3D9 早期设备追踪器

`d3d9_early_trace.dll` 只记录 `Direct3DCreate9 → IDirect3D9::CreateDevice`，输出设备对象和 `Present/EndScene/DrawIndexedPrimitive` 虚表地址；不覆盖客户端原 DLL。

## 使用

1. 先彻底关闭正在运行的 `crossfire.exe`。
2. 从 x86 PowerShell 执行：

```powershell
& 'C:\Users\15135\Documents\Codex\2026-08-31\jian\outputs\d3dref9自治\启动-自治DLL.ps1' -Launch -DllPath 'C:\Users\15135\Documents\Codex\2026-08-31\jian\outputs\d3dref9自治\tools\d3d9_early_trace.dll'
```

3. 登录至实际对局后查看：`C:\Windows\Temp\d3d9_early_trace.log`。

有效日志包含 `CreateDevice ... device=... vtbl=... Present17=... EndScene42=... DIP82=...`。该记录用于下一步将 D3D9 绘制追踪接入干净 DLL。
