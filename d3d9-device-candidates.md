# D3D9 设备候选（只读扫描）

- PID：`23764`；扫描：`1292611584` 字节 / `2531` 个可读提交区。
- D3D9 模块基址：`0x69F00000`；候选数：`0`。

|设备指针存储地址|虚表|Present (17)|EndScene (42)|DrawIndexedPrimitive (82)|
|---:|---:|---:|---:|---:|

该结果仅定位 COM 设备候选；还需在下一步记录 EndScene/DrawIndexedPrimitive 调用和渲染状态，才能将透视绘制接入。
