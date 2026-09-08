# 2026-09-07 实际对局自瞄链修复报告

## 结论

本轮修复了普通自瞄/自动开火没有进入目标链的三个根因：本地槽位 `0` 被误判为失败、骨骼坐标读取依赖未触发的 inline callback、D3D9 矩阵读取入口被完全停用。另修正了角度写入单位：游戏字段是弧度，不是度。

## 实测证据

- 运行进程：`crossfire.exe` PID `17452`。
- `cshell.dll+0x166AD00 = 0x11703F58`。
- `playersRoot+0x208 = 0`，按源码语义表示本地槽位 `1`。
- 实体记录：`playersRoot+0x210 + (slot-1)*0xD80`。
- 坐标链：`record+0x00 -> renderObject+0x2098 -> coordinate+0x0C+part*0x40`。
- D3D9 设备链：`crossfire.exe+0x11BF438 -> wrapper -> IDirect3DDevice9`；当前局读到 wrapper `0x15DCC018`、device `0x147666A0`、视口 `1280×720`。
- 运行时投影样本已将敌方槽位 9–11 的骨点投影到有效屏幕坐标。

## 源码改动

1. `src/entity_adapter.cpp`
   - `LocalSlot()` 接受 raw index `0` 并返回 slot `1`。
   - `ReadPosition()` 改用直接实体坐标链，不再等待 coordinate inline hook。
   - 本地存活/观战字段改为读取当前人物对象的 `+1320`/`+24892`。
2. `src/render_adapter.cpp`
   - `Start()` 改为被动读取已创建的 D3D9 device。
   - 不安装 `D3DXCreateFontA` 跳板，不修改 D3D9 VTable。
   - `ReadFrame()` 增加设备重置异常保护和矩阵有限值校验。
3. `src/game_handlers.cpp`
   - `AimTarget()` 使用正确的 X/深度/垂直轴和弧度写入 `CurrentPlayer()+3464/+3468`。
   - 自动开火使用 `mouse_event`，并增加 `aim_gate`、`aim_target`、`aim_write`、`auto_fire_sent` 诊断事件。
4. `src/input_router.cpp`
   - 瞬狙鼠标按键改用 legacy `mouse_event` 路径。

## 构建与验证

```powershell
cmake --build build --config Release --target d3dref9_autonomy d3dref9_direct
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
python .\tools\inspect_pe_exports.py .\D3DREF9.DLL
```

结果：Win32 Release 构建成功；`D3DREF9.DLL` 为 PE32，ordinal 1 存在；LoadLibrary + ordinal-1 冒烟测试退出码为 `0`。

新 DLL：`D3DREF9.DLL`，SHA-256 `227AF20940206711789B68F9145EF0750262460DC88F39FF5BA61E4D0D0B6D65`。

## 部署边界

当前游戏 PID `17452` 仍在运行，客户端现有 DLL 未替换，仍是稳定版本：

`E:\game\已加速- CF2.0搭建（使用2012系统）\客户端\10.4CrossFire\D3DREF9.DLL`

因此本轮构建不会影响当前对局；退出游戏后再替换并重新启动，才会加载新链路。运行时证据保存于 `runtime-diffs\match-baseline\aim-runtime-probe-current.json` 和 `autoaim-fix-build.json`。

