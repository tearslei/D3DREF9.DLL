# 实现状态（当前构建）

## 已完成

| 模块 | 状态 | 说明 |
|---|---|---|
| DLL worker / loader-lock 修复 | 已完成 | DllMain 只创建后台线程，不在加载锁内访问游戏对象 |
| Home 面板 | 已完成 | 320×420、约 45% 透明、隐藏不停止功能、不重置状态 |
| 普通自瞄 | 已接入 | 存活/敌我/微型范围/身体骨点/准心附近/障碍射线筛选，写入瞄准角度 |
| 普通自动开火 | 已接入 | 物理左键门控，约 130 ms 一次独立 LEFTDOWN→LEFTUP |
| 瞬狙 | 已接入 | L 切换；物理右键按住持续触发、松开立即停止；五部位最近点；无目标不发序列；冷却恢复状态 |
| 普通部位切换 | 已接入 | F10+6 循环颈部 → 头部 → 胸部 |
| Alt+Z | 已接入 | 自瞄/自动开火总开关；不再保留 Alt+1 |
| 键盘宏 | 已迁移 | W+F、W+C、W+左Alt、X/V；`W+Ctrl` 已删除；generation 取消与统一释放 |
| 延迟启动 | 已接入 | 两键优化在加载约 150 秒后开启；房间保持和摔不掉血自动开启且不显示面板 |
| 删除项 | 已完成 | 无限背包、Alt+1、F6、P 均不再注册；旧版不掉血已恢复为 F7+5 默认开启 |
| 旧版不掉血 | 已接入 | `*(cshell+0x16F2C04)+468=0` 写入循环；F7+5 切换；启动默认开启且不显示面板 |
| 摔不掉血 | 已接入 | 启动默认开启；F9+V 切换；同时覆盖 TCII 新型 +1320/+1456 与旧型 +1560/+1692 布局 |
| 直接替换壳 | 已完成 | Win32/x86；仅 ordinal 1 导出；保留原文件备份 |

## 目标筛选链

1. 读取实体快照。
2. 过滤存活实体和敌我关系。
3. 投影五个身体骨点，要求身体代理或任意骨点进入“微型”准心范围。
4. 进行障碍射线判断；入口异常时 fail-closed。
5. 按准心距离排序并短时锁定。
6. 将选中骨点转换为弧度并写入瞄准角度。
7. 普通模式按 130 ms 点击；瞬狙模式按配置时序执行。

## 构建和静态验证

```powershell
cd 'C:\Users\15135\Documents\Codex\2026-08-31\jian\outputs\d3dref9自治'
cmake --build .\\build --config Release --target d3dref9_autonomy d3dref9_direct -- /m:2
python .\\tools\\inspect_pe_exports.py .\\D3DREF9.DLL
.\\tests\\build\\Release\\d3dref9_load_probe.exe .\\D3DREF9.DLL
```

预期：MSVC Win32/x86 构建成功；PE 为 `Machine=0x014C`、`Functions=1 Names=0`、存在 ordinal 1；LoadLibrary/ordinal-1 冒烟通过。

## 仍需实机确认

- 实际对局中实体坐标、骨骼、矩阵和障碍射线日志是否持续有效。
- 普通 130 ms 点击与瞬狙右键序列在目标客户端输入层的表现。
- W+F 等宏在不同地图/窗口焦点下的节拍。
- 重新进图、退图、回大厅后的生命周期稳定性。

诊断日志：`%TEMP%\\d3dref9_input.log`、`d3dref9_handlers.log`、`d3dref9_entities.csv`、`d3dref9_render.log`。



### 2026-09-08 门控修正
- 普通自瞄/自动开火改用 `GetAsyncKeyState(VK_LBUTTON)` 作为物理左键门控，注入的 LEFTDOWN/LEFTUP 不会激活。
- 无物理左键时记录 `aim_gate value=7`，用于确认门控已关闭。
- 修正启动状态：`AimAutoFire` 的 `defaultOn` 已改为 `false`；未按 `左Alt+Z` 前不会运行普通自瞄或自动开火。
- 2026-09-08 重新构建并部署客户端 `D3DREF9.DLL`，SHA-256：`8D8DB2D5DF137E2A46F02383619F34C91DFC68E90CE4C7288F7608AC679DDB7A`。
- 瞬狙准心改为中型范围（`instant_range_divisor=8`，普通模式仍为 `16`）。`W+Ctrl`/Ctrl 单次循环宏已删除。
- 面板总按键由 `Home` 改为 `F9+H`；`Home` 不再被 DLL 注册或拦截。

### 2026-09-10 实机坐标链修正
- 实机读取确认：`cshell.dll` 基址为 `0x10050000`，`cshell+0x166AD00` 得到玩家根表；坐标 Hook 地址 `crossfire+0x23567F` 的 6 字节签名与 TCII 源码一致。
- 旧实现错误地从 `entity+0x2098` 推断坐标，导致 `d3dref9_entities.csv` 中 `pos_valid/bone_mask` 全为 0；瞄准角度虽被写入，但目标坐标不可靠。
- 已改回 TCII 源链：Hook 捕获 `坐标指针[1]` → `数据指针` → `*(数据指针+(slot-1)*4)` → `+12+64*部位`，坐标字段为 `X@0/Z@16/Y@32`。
- 已启用 Hook 初始化与切图重试。新 DLL 尚未部署到正在运行的客户端；退出 `crossfire.exe` 后再部署。

### 2026-09-10 FPS 影响修正
- 观察到旧客户端 DLL 的坐标 Hook 回调已超过 734,000 次；旧回调在游戏线程中执行 `VirtualQuery`、16 槽扫描和每千次文件日志，造成明显渲染线程竞争。
- 新版本回调只做两个原子指针发布；坐标校验和槽位匹配移到 DLL worker，实体坐标块改为每槽一次批量读取，worker 周期由 10ms 调整为 20ms。
- 新构建 DLL：`AD628B321F498A80F89BD57F3FACF355829BC0FEC36CE73EA0E95D9BADCBDB89`。

### 2026-09-09 瞬狙切枪链与 Alt+Z 双向切换
- 瞬狙每次有效射击后恢复为 TCII 链：等待约 19–21 ms → `3` 按下 45 ms → 等待 100 ms → `1` 按下 25 ms。
- `3→1` 完成后再等待 `post_switch_cooldown_ms=300`，之后才恢复瞬狙前原本开启的普通自瞄/自动开火状态。
- `Alt+Z` 由 `FeatureManager::Toggle` 双向切换：第一次开启，第二次关闭；普通模式仍必须物理按住左键才会运行。
- 新增配置项：`switch3_*`、`switch1_*`、`post_switch_cooldown_ms`。

