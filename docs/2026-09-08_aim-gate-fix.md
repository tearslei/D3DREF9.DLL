# 2026-09-08 普通模式门控修正

## 行为

- `左Alt+Z`：只切换“普通自瞄/自动开火”总开关，不再有 F7+Q/W 的隐式开关。
- 普通模式：总开关打开 **且真实物理左键仍处于按下状态** 时，才筛选目标、写瞄准角度并按约 130 ms 发送一次独立 `LEFTDOWN → LEFTUP`。
- 释放物理左键立即停止普通自瞄和自动开火；DLL 自己注入的点击不会重新满足门控。
- 瞬狙模式仍由物理右键触发，和普通左键门控分离。

## 障碍判断

配置保持：

```ini
visibility_required=true
visibility_fail_closed=true
```

目标筛选顺序为存活/敌我 → 准心微型窗口 → 五骨点 → `IsVisible` 射线；射线入口无效或异常时拒绝目标。日志 `aim_gate`：`5` 表示候选骨点均被阻挡，`6` 表示有身体候选但没有可见骨点，`7` 表示普通模式因物理左键未按下而关闭。

## 验证

- `cmake --build build --config Release --target d3dref9_autonomy d3dref9_direct -- /m:2` 通过。
- ordinal-1 PE 检查：Win32，`Functions=1 Names=0`。
- 已部署到客户端 `E:\game\已加速- CF2.0搭建（使用2012系统）\客户端\10.4CrossFire\D3DREF9.DLL`。
