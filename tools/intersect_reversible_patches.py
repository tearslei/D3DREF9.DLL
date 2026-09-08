import json,sys
from pathlib import Path
root=Path(sys.argv[1]) if len(sys.argv)>1 else Path('runtime-diffs/batch')
for f in sorted(root.glob('*.json')):
 if f.name=='manifest.json': continue
 try:j=json.loads(f.read_text(encoding='utf-8'))
 except:continue
 ph={x['phase']:x for x in j.get('phases',[])}
 if 'settled' not in ph or 'restored' not in ph: continue
 en={(c['module'],c['rva'],c['new']):c for c in ph['settled']['changes']}
 out=[]
 for c in ph['restored']['changes']:
  # restored should write enabled bytes back to baseline
  key=(c['module'],c['rva'],c['old'])
  if key in en and en[key]['old']==c['new']:
   out.append({'module':c['module'],'rva':c['rva'],'old':c['new'],'enabled':c['old'],'length':c['length']})
 if out:
  print(j['combo'],len(out))
  for c in out[:30]:print(' ',c)
  (root/(f.stem+'_reversible.json')).write_text(json.dumps({'combo':j['combo'],'patches':out},ensure_ascii=False,indent=2),encoding='utf-8')
