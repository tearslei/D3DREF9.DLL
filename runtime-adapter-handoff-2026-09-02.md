# Runtime adapter handoff (2026-09-02)

Applied fixes to the x86 1.1.85.7 adapter:

- Entity alive semantics now match TCII `取敌人生存`: `entity + 0x204 == 0` means alive.
- Team byte at `entity + 0x400` is used in 8-player modes; slot fallback remains for other modes.
- Aim-part mapping matches source: head=5, neck=6, chest=4, waist=3, butt=2.
- Added guarded source-compatible `IsVisible` call (`crossfire + 0x11AEF38` object slot, `crossfire + 0x5EE560` call) to target acquisition.
- Instant sniper mouse trigger no longer depends on stale `TargetingReady` (which is intentionally false while normal auto-aim is paused in sniper mode); `Run()` performs the target check.

Build/deploy:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\部署直接替换壳到客户端.ps1 -Apply
```

Artifacts:
- `D3DREF9.DLL` (Win32, ordinal 1 only), SHA-256 `C1B7438B0E565CA2BE1A96EE6C9AAD7843F5D083DA706AF1603D068F9E7ABBA5`.
- Deployed to `E:\game\已加速- CF2.0搭建（使用2012系统）\客户端\10.4CrossFire\D3DREF9.DLL` with backup `D3DREF9.before-ordinal1.bak`.

Static verification: `Machine=0x014C Magic=0x010B ExportBase=1 Functions=1 Names=0`.
Runtime match validation is still required (`%TEMP%\\d3dref9_entities.csv`, `d3dref9_entity_hook.log`, `d3dref9_render.log`).
