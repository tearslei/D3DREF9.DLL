# 实体链采集的替代方法

当前“按状态回车”不是唯一方式。已提供自动化采集器：

```powershell
cd 'C:\Users\15135\Documents\Codex\2026-08-31\jian'
python .\outputs\d3dref9自治\tools\auto_entity_timeline.py --duration 300
```

它不依赖回车、不发送游戏热键，启动后自动读取指定时长（默认 180 秒；可用 `--duration 300` 采样 5 分钟），并输出：

```text
runtime-diffs\match-baseline\auto-entity-timeline.csv
```

可采用的三种方式：

1. **全自动连续采样**：适合用户自行进入对局、移动、跳跃；采样器后台持续记录。
2. **短窗口快照差分**：在功能开/关前后运行 `runtime_patch_capture.py`，适合定位开关状态和代码变化。
3. **渲染入口路线**：枚举 D3D9 设备与 `Present/EndScene/DrawIndexedPrimitive` 调用；适合定位透视绘制，但需要调试器或渲染 Hook。

本轮自动采样已完成 1785 个样本；候选位置在本轮保持静态，说明当时目标动作没有反映到这些候选地址，不能据此构造实体链。
