from pathlib import Path
import json,statistics
OUT=Path(__file__).parent
j=json.loads((OUT/'profile_noctiflores.json').read_text(encoding='utf-8'))
def p95(v):return sorted(v)[int(.95*(len(v)-1))]
rows=[]
for b in j['blocks']:
    values=[s['metrics'] for s in j['samples'] if s['block']==b['block']]
    rows.append(dict(block=b['block'],enabled=b['enabled'],seconds=b['seconds'],n=len(values),median=[statistics.median(v) for v in zip(*values)],p95=[p95(v) for v in zip(*values)],components=[r['components'] for r in b['peers']],particles=[r['particles'] for r in b['peers']],plants=[len(r['plants']) for r in b['peers']]))
groups=[]
for enabled in [False,True]:
    selected=[r for r in rows if r['enabled']==enabled]
    values=[s['metrics'] for s in j['samples'] if j['blocks'][s['block']]['enabled']==enabled]
    groups.append(dict(enabled=enabled,n=len(values),mean_block_medians=[statistics.mean(r['median'][i] for r in selected) for i in range(4)],pooled_p95=[p95(v) for v in zip(*values)]))
r=dict(passed=j['passed'],blocks=rows,groups=groups,viewport_sizes=j.get('viewport_sizes'),quality=j.get('quality'))
(OUT/'performance_summary.json').write_text(json.dumps(r,indent=2),encoding='utf-8');print(json.dumps(groups,indent=2))
