#!/usr/bin/env python3
"""Extract reversible D3DREF9 state transitions from a timeline CSV."""
import csv, glob, json, os
from pathlib import Path
base = Path(__file__).resolve().parents[1]
directory = base.parent.parent / 'runtime-diffs' / 'match-baseline'
files = sorted(directory.glob('auto-entity-timeline-pid*.csv'), key=lambda p:p.stat().st_mtime, reverse=True)
if not files: raise SystemExit('no timeline CSV')
src=files[0]; rows=list(csv.DictReader(src.open(encoding='utf-8')))
labels={'1a8d58':'人物透视','1a8dc4':'人物穿墙','1a8cdc':'自动瞄准+自动开枪'}
events=[]
for col in rows[0]:
    if not col.startswith('d3d@'): continue
    key=col[4:]
    changes=[]
    for i in range(1,len(rows)):
        if rows[i][col] != rows[i-1][col]:
            changes.append({'row':i,'offset_sec':round(float(rows[i]['t'])-float(rows[0]['t']),3),'from':rows[i-1][col],'to':rows[i][col]})
    if changes:
        events.append({'rva':'0x'+key,'label':labels.get(key,'unknown'),'changes':changes})
out=directory/'latest-feature-timeline.json'; out.write_text(json.dumps({'source':str(src.resolve()),'pid':int(rows[0]['pid']),'events':events},ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps({'source':str(src),'pid':rows[0]['pid'],'events':events,'output':str(out.resolve())},ensure_ascii=False))
