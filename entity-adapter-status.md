# 实体/坐标适配器（1.1.85.7）

## 已落地

- `include/entity_adapter.h` / `src/entity_adapter.cpp` 已加入 CMake。
- 实体根：`cshell.dll + 0x166AD00` 解引用后 `+0x210`。
- 槽位表：`root + 0x14 + (slot-1) * 0xD80`。
- 模式：`cshell.dll + 0x166ACE0`；源码映射 0/8/15/16 槽位。
- 本地槽位：`*( *(cshell+0x166AD00) + 0x208 ) + 1`。
- 敌方判断：8 人按槽位分队，16 人排除本地，15 人按 `entity+0x123BC` 生化状态比较。
- 敌人生存：`entity + 0x204` 字节非零。
- 本地坐标：`*( *(players)+0x30 ) + 0x6C/0x70/0x74`，按源码 X/Z/Y 顺序映射。
- 坐标/骨骼：坐标表指针就绪后，`coord + 0x0C + part*0x40`，X/Z/Y 位于 `+0/+0x10/+0x20`；瞄准五部位映射为 `6/5/4/3/2`。
- `WorldToScreen` 已实现 D3DX 行向量等价数学；矩阵和 viewport 由后续 D3D9 入口提供。
- 每 250ms 输出只读诊断：`%TEMP%\d3dref9_entities.csv`（ASCII 文件名，避免系统代码页导致路径乱码）。

## 尚未宣称完成

源码没有给出 `dx_pos` 的初始化赋值；现在适配器会在 `crossfire.exe + 0x23567F`（绝对地址 `0x63567F`）签名匹配 `89 8D 20 FF FF FF` 时安装受保护的 relay，按实体对象过滤并捕获 ECX 坐标表；不匹配则跳过。D3D9 状态桥会在 `D3DXCreateFontA` 首次收到设备后读取 view/projection/viewport，并对五个部位进行屏幕投影。上述链路仍需实际对局确认 `pos_valid=1`、`bone_mask!=0` 和 `screenValid`，确认后才进入目标筛选和绘制。

## 下一次运行验证

1. 退出 CF 后部署新壳：
   ```powershell
   cd 'C:\Users\15135\Documents\Codex\2026-08-31\jian\outputs\d3dref9自治'
   .\部署直接替换壳到客户端.ps1 -Apply
   ```
2. 启动 CF，进入实际对局 10 秒以上。
3. 退出 CF，检查：
   ```powershell
   Import-Csv "$env:TEMP\d3dref9_entities.csv" | Select-Object -Last 20
   ```
4. 只有 `entity_addr/alive/enemy` 稳定且 `pos_valid=1` 后，才进入矩阵来源和 D3D9 绘制入口接线。

## 当前验证命令（新壳）

启动并进入对局后退出 CF，再执行：

```powershell
$t = [IO.Path]::GetTempPath()
Get-Content (Join-Path $t 'd3dref9_entity_hook.log') -Tail 40
Get-Content (Join-Path $t 'd3dref9_render.log') -Tail 40
Import-Csv (Join-Path $t 'd3dref9_entities.csv') |
  Select-Object -Last 30 tick,coord_table,entity_slot,entity_addr,alive,enemy,pos_valid,bone_mask,screen_valid,screen_x,screen_y
```

期望观测：`event=installed`、`event=capture`、`event=device_captured`、`event=draw_indexed_primitive`；CSV 中 `coord_table` 非零后，`pos_valid`/`bone_mask` 才会反映坐标表读取结果。
