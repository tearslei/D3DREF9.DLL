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


### 2026-09-11 部署与冒烟验证（待实战对局）
- 已确认 `crossfire.exe` 退出后，将坐标链修复构建部署到客户端正式文件 `E:\game\已加速- CF2.0搭建（使用2012系统）\客户端\10.4CrossFire\D3DREF9.DLL`。
- 新正式 DLL SHA-256：`532A101C3C451D916E4458581EE33599787920FF472E538F30B83757F51BD195`。
- 原正式 DLL 已备份为同目录 `D3DREF9.DLL.before-coordinate-capture-20260911-222609.bak`，SHA-256：`453E42998B4F8C9CA009A4D2166C11127E573B5A1C71C9E4D0AB947DE1CF75BC`。
- 启动冒烟中确认坐标 Hook 安装：`event=installed target=0x0063567f relay=0x17200000`；D3D9 设备包装器正常创建。
- 本次启动约 18 秒后客户端自行退出，日志只包含大厅/初始化快照：`local_ok=1` 但 `entity_slot=0`、`pos_valid=0`，没有 `capture`、`aim_target`、`aim_write`、`auto_fire_sent`。因此尚未获得实际对局坐标证据，不能据此判断修复后的瞄准链是否已在对局生效。
- 下一步：保持服务器和客户端在线，进入实际对局后再读取 `%TEMP%\d3dref9_entity_hook.log`、`%TEMP%\d3dref9_entities.csv`、`%TEMP%\d3dref9_handlers.log`，重点核对 `capture`、`pos_valid/bone_mask`、`aim_target`、`aim_write`、`auto_fire_sent`。

### 2026-09-11 实际对局实时证据
- PID `20360` 在 22:48:39 启动，实际对局快照已出现 `mode=26`、`local_slot=1`、`local_alive=1`、`spectating=1`。
- 新坐标链已成功捕获多个槽位：例如 slot 1/9/10/11/12/13/14/15/16，坐标指针均为有效地址；实体快照出现 `pos_valid=1`、`bone_mask=31`。
- 目标筛选已运行：日志出现 55 次 `aim_target`，目标槽位包含 9、10、11、12、13、14、15；同时出现 40 次 `aim_write`，写入地址 `0x599a2b48`（玩家角度字段）。
- 当前窗口未出现 `auto_fire_sent`；`aim_gate` 主要为 4/5（候选点/障碍可见性筛选状态），没有证据表明普通模式实际发送了 130 ms 点击。进程随后退出，需下一次在保持左键按住的可控测试中确认普通自动开火。

### 2026-09-11 瞬狙开火延迟调整
- 新增 `instant_sniper.fire_extra_delay_ms` 配置项，默认 `70`。
- 时序由 `右键按下 → 15–30 ms → 左键开火` 改为 `右键按下 → 15–30 ms → 额外 70 ms → 左键开火`，总开镜后等待约 `85–100 ms`。
- 已重新编译、通过 ordinal-1 PE 验证并部署到客户端。
- 新 DLL SHA-256：`4E5771849490CBDCBBFDA456E0DA3C3FC2BB4FEF180A793A902BD3FA08742596`。
- 客户端配置已备份为 `config\d3dref9自治.ini.before-fire-extra-delay-20260911-230405.bak`。

### 2026-09-11 瞬狙先瞄准后开火修正
- 调整 `InstantSniper::Run()`：不再要求未开镜状态先获取目标；先按下右键并等待原 scope_delay，再在开镜状态重新筛选目标并写入瞄准角度，之后再等待 `fire_extra_delay_ms=70` 才发送左键。
- 这样瞬狙流程明确变为：右键激活 → 开镜后自瞄 → 等待 70 ms → 自动开火。
- 新构建 SHA-256：`18E0BBB73BADC326DEBB47D8AFD41D5B02FCBE26B23569EFCB64626559B5A7DC`。
- 当前 `crossfire.exe` PID 8464 正在运行，已将新 DLL 暂存为客户端 `D3DREF9.DLL.next`；退出游戏后再替换正式 DLL，避免覆盖正在使用的文件。
