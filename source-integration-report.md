# TCII 2.0 源码整合报告

## 输入
- 源码目录：`E:\game\辅助\2.0 TCII全套开源`
- 已解密密码：`7`
- 主逻辑：`TCII变态版本(加追敌和跳舞).e`
- 客户端：x86，`crossfire.exe/cshell.dll 1.1.85.7`

## 已提取证据
- 11 个易语言工程/模块均已导出到 `source-dumps/`。
- 生成功能索引：`source-feature-index.md/json`。
- 生成功能写入表：`source-patch-table.json`（29 个相关子程序，292 条写入/Hook 记录）。
- 生成热键交叉表：`source-hotkey-table.md/json`。
- 关键来源行：初始化地址在 `source-handler-blocks.md` 的“等待初始化”；D3D9 绘制入口为 `myDrawIndexedPrimitive_x`。

## 已接入自治 DLL
- `src/game_handlers.cpp`：按源码适配层接入版本检查、模块地址解析、VirtualQuery/VirtualProtect 保护、状态写入与日志。
- 已接入：人物透视、旧版不掉血、摔不掉血、无后坐力、零秒换弹、子弹穿墙、刀枪爆头、第三人称、空格连跳、人物穿墙、无限背包、房间保持、进程优化。
- `FeatureManager` 的 Toggle/Set 已连接到处理器。
- 处理器日志：`%TEMP%\d3dref9自治-handlers.log`。
- 配置：`config/client_1.1.85.7.json`。

## 未接入
- 实体枚举、骨骼读取、世界/投影矩阵、D3D9 VTable Hook、DrawIndexedPrimitive 模型识别、自瞄目标筛选、自动开枪、瞬狙目标点选择。
- 这些依赖运行期对象/Device 指针和渲染线程时序，静态源码常量不足以保证可用。

## 验证
```text
build.ps1 -> 成功
PE Machine=0x14c, OptionalHeader=0x10b
导出：d3dref9_initialize、d3dref9_shutdown
```

当前构建是“源码适配处理器版”，不是已验证的完整成品；部署前请使用副本并以日志确认模块就绪及每次状态转换。
