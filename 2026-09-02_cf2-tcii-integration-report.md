# CF2/TCII 处理器整合报告（2026-09-02）

## 结果

- 已按 `TCII变态版本(加追敌和跳舞).e` 的 1.1.85.7 常量更新实体链和功能处理器。
- 已重新构建 Win32/x86 两个目标，并将直接替换版部署到客户端。
- 客户端启动冒烟通过：`D3DREF9.DLL` 成功加载，D3D9 设备和坐标 relay 均安装。

## 本次代码修正

1. 本地人物读取改为 `*(玩家数组)+0x30`，不再把 `+0x208` 槽位字段误当作变换指针。
2. 敌人坐标/骨骼保持源码链：`OBJECT+0x14+(slot-1)*0xD80` 与坐标表 `+0x0C+part*0x40`。
3. 子弹穿墙改为源码三个字段（1392/1512/1644）和数值 256。
4. 刀枪爆头改为 95 个武器记录、步长 156、字段 `+84=1`。
5. 旧版不掉血改为 `base+466` 单 dword 写入，并纳入关闭恢复表。
6. 两键优化进程加入 TCII 同名辅助进程优先级处理。

## 构建与部署证据

```text
Machine=0x014C Magic=0x010B Functions=1 Names=0
ordinal 1: RVA 0xCC40
SHA256 (构建/客户端): 2686069959B39A2779FBF7971E9B30916FCB4D3B7F1FC07A9DA5D92375CA1C2C
原 DLL 备份 `D3DREF9.before-ordinal1.bak`（等同用户的 `D3DREF9wg.DLL`）：E73FEC5C4692D06B136BE3D451A88BEE05921E0076E260C969F3C97C2A905A5A
客户端加载: PID=8820, D3DREF9.DLL, BASE=0x60E50000, SIZE=102400
```

## 运行验证

已观察 `event=installed`、`event=device_captured`，进程在大厅持续运行未闪退。实体 `pos_valid/bone_mask/screen_valid` 需要在实际地图中产生坐标表回调后确认；大厅阶段为空是预期行为。

进入地图停留 10 秒后退出，再执行：

```powershell
$t=[IO.Path]::GetTempPath()
Get-Content (Join-Path $t 'd3dref9_entity_hook.log') -Tail 40
Get-Content (Join-Path $t 'd3dref9_render.log') -Tail 40
Import-Csv (Join-Path $t 'd3dref9_entities.csv') |
  Select-Object -Last 30 tick,coord_table,entity_slot,entity_addr,alive,enemy,pos_valid,bone_mask,screen_valid,screen_x,screen_y
```

## 回滚

客户端原文件保存在 `D3DREF9.before-ordinal1.bak`；需要恢复时运行 `tools/restore_original_d3dref9.ps1`。
