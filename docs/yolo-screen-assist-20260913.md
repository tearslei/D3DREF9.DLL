# YOLO 风格屏幕辅助瞄准改造（2026-09-13）

## 变更
- 将目标筛选拆为 `visual_range_px` 检测范围和 `aim_range_px` 磁性锁定范围。
- 对选定骨点计算屏幕坐标误差，通过限幅、平滑和死区调用 `InputRouter::MouseMove`。
- 增加 `fire_confirm_radius_px` 与 `fire_confirm_frames`，未达到确认帧数不发送鼠标点击或瞬狙左键。
- 保留障碍射线、存活和敌我筛选；`memory_aim_enabled=false` 时不写入旧的角度地址，避免与鼠标辅助争抢。
- 普通和瞬狙共享目标选择及平滑路径，开火时序保持原实现（普通85–100ms随机点击，瞬狙3→1链）。

## 默认参数
`visual_range_px=320`、`aim_range_px=120`、`mouse_smoothing=0.82`、`mouse_move_gain=0.95`、`mouse_max_step_px=127`、`fire_confirm_radius_px=8`、`fire_confirm_frames=2`。

## 验证
- `build.ps1`：Win32 Release 两个 DLL target 编译成功。
- `inspect_pe_exports.py`：x86，Functions=1，ordinal 1 导出通过。
- `tests/build/Release/d3dref9_load_probe.exe`：`loaded` 且 `ordinal1 result=1`。
- 已部署至客户端 `E:\game\已加速- CF2.0搭建（使用2012系统）\客户端\10.4CrossFire\D3DREF9.DLL`，并同步配置。
