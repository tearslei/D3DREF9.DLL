# 2026-09-08 CF2 自瞄/宏当前构建报告

## 结论

当前 Win32/x86 直接替换壳已重新编译并部署。普通模式的自动开火配置为 130 ms 独立点击；瞬狙只接受物理右键；F6、Alt+1、P 不再注册；客户端配置已同步到 `config\\d3dref9自治.ini`。

## 本次继续工作

1. 清理 `README.md`、`使用说明.md`、`IMPLEMENTATION_STATUS.md` 和热键矩阵中的旧按键与旧 1600 ms 描述。
2. 移除遗留的隐藏旧不掉血写入循环 `TickStableNoDamage()`；当前仅保留显式的摔不掉血功能。
3. 修正直接替换部署脚本，使 DLL 和配置文件一起部署，并保留配置回滚副本。
4. 重新构建自治 DLL 和 ordinal-1 直接替换壳。
5. 验证 PE 导出、绝对路径 LoadLibrary 冒烟和部署后文件哈希。

## 关键实现

- 普通模式：Alt+Z 打开总开关，物理左键按住才进入目标筛选；每隔约 130 ms 发送一次 `LEFTDOWN -> LEFTUP`。
- 瞬狙模式：L 切换，物理右键触发；筛选头/颈/胸/中腰/臀部最近有效点；无有效目标不发送鼠标序列。
- 目标筛选：存活、敌我、准心附近、五骨点身体代理、障碍射线 fail-closed、最近目标短锁定。
- 宏：generation 取消旧动作，结束时只释放 DLL 自己按下的键；保留 W+F、W+C、W+左Alt、Ctrl 单次循环、X/V。
- Home：320×420、约 45% 透明、隐藏不停止功能。

## 验证证据

构建：

```powershell
cmake --build .\\build --config Release --target d3dref9_autonomy d3dref9_direct -- /m:2
```

PE 检查：

```text
Machine=0x014C Magic=0x010B ExportBase=1 Functions=1 Names=0
ordinal 1: RVA 0xD570
```

绝对路径 LoadLibrary（避免相对路径误加载 System32 同名 DLL）返回 0，部署后两份 DLL SHA-256 一致。客户端配置验证：

```text
auto_fire_interval_ms=130
trigger_mouse=right
```

## 尚未完成的实机门

仍需在实际对局中确认实体坐标、骨骼、矩阵、障碍射线和输入节拍。诊断日志位于 `%TEMP%\\d3dref9_*.log` 与 `d3dref9_entities.csv`。

