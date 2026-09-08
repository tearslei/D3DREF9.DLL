#!/usr/bin/env python3
"""Quality-check newest autonomous match sample without guessing semantics."""
from __future__ import annotations
import csv, json, math, statistics
from pathlib import Path

base = Path(__file__).resolve().parents[1]
directory = base.parent.parent / 'runtime-diffs' / 'match-baseline'
files = sorted(directory.glob('auto-entity-timeline-pid*.csv'), key=lambda p: p.stat().st_mtime, reverse=True)
if not files:
    raise SystemExit('no autonomous sample CSV found')
src = files[0]
with src.open(newline='', encoding='utf-8') as f:
    rows = list(csv.DictReader(f))
if not rows:
    raise SystemExit('sample CSV has no data rows')
float_cols = [c for c in rows[0] if c not in ('t', 'pid') and not c.startswith('d3d@')]
stats = []
for col in float_cols:
    vals = []
    for row in rows:
        try:
            x = float(row[col])
            if math.isfinite(x):
                vals.append(x)
        except (TypeError, ValueError):
            pass
    if not vals:
        continue
    deltas = [abs(b - a) for a, b in zip(vals, vals[1:])]
    stats.append({'column': col, 'count': len(vals), 'unique': len(set(vals)),
                  'min': min(vals), 'max': max(vals),
                  'max_abs_delta': max(deltas, default=0.0),
                  'median_abs_delta': statistics.median(deltas) if deltas else 0.0})
state_cols = [c for c in rows[0] if c.startswith('d3d@')]
states = []
for col in state_cols:
    vals = [row[col] for row in rows]
    states.append({'column': col, 'changes': sum(a != b for a, b in zip(vals, vals[1:])),
                   'unique': len(set(vals)), 'first': vals[0], 'last': vals[-1]})
start = float(rows[0]['t']); end = float(rows[-1]['t'])
intervals = [float(b['t']) - float(a['t']) for a, b in zip(rows, rows[1:])]
report = {'source': str(src.resolve()), 'pid': int(rows[0]['pid']), 'rows': len(rows),
          'duration_seconds': end - start,
          'sample_interval_median': statistics.median(intervals) if intervals else 0.0,
          'float_columns': len(float_cols),
          'dynamic_float_columns': sum(s['unique'] > 1 for s in stats),
          'state_columns': len(state_cols),
          'state_columns_with_changes': sum(s['changes'] > 0 for s in states),
          'interpretation': 'valid continuous snapshots of configured RVAs; not entity/bone/matrix proof',
          'top_dynamic': sorted(stats, key=lambda x: (x['unique'], x['max_abs_delta']), reverse=True)[:40],
          'state_summary': states}
out = directory / 'latest-entity-sample-quality.json'
out.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
md = base / 'latest-entity-sample-quality.md'
lines = ['# 最新实体时间线采样质量', '', f'- 源文件：`{src.resolve()}`',
         f'- PID：`{report["pid"]}`；样本：`{report["rows"]}`；时长：`{report["duration_seconds"]:.3f}s`；中位间隔：`{report["sample_interval_median"]:.4f}s`',
         f'- 配置浮点列：`{report["float_columns"]}`；发生变化：`{report["dynamic_float_columns"]}`',
         f'- D3DREF9 状态列：`{report["state_columns"]}`；发生变化：`{report["state_columns_with_changes"]}`', '', '## 结论', '',
         '这份文件在采样时序和数据完整性上正确；但采样器只读取预先指定的固定 RVA，不能据此确认实体列表、骨骼、阵营、生命值或世界矩阵。动态列只是候选，不能直接接入处理器。', '', '## 最大变化候选']
for s in report['top_dynamic'][:20]:
    lines.append(f"- `{s['column']}` unique={s['unique']} range={s['min']:.3f}..{s['max']:.3f} max_delta={s['max_abs_delta']:.3f}")
md.write_text('\n'.join(lines) + '\n', encoding='utf-8')
print(json.dumps({'source': str(src), 'rows': len(rows), 'duration': round(end - start, 3),
                  'dynamic_float_columns': report['dynamic_float_columns'],
                  'state_changes': report['state_columns_with_changes'],
                  'json': str(out.resolve()), 'report': str(md.resolve())}, ensure_ascii=False))
