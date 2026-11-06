from pathlib import Path
import json,statistics
OUT=Path(__file__).parent
j=json.loads((OUT/'profile_equipment.json').read_text())
rows=[]
def p95(v):return sorted(v)[int((len(v)-1)*.95)]
for block in j['blocks']:
    samples=[s['metrics'] for s in j['samples'] if s['block']==block['block']]
    totals={}
    for peer in block['peers']:
        for key,value in peer['counts'].items():totals[key]=totals.get(key,0)+value
    row=dict(block=block['block'],legacy=block['legacy'],seconds=block['seconds'],n=len(samples),median=[statistics.median(v) for v in zip(*samples)],p95=[p95(v) for v in zip(*samples)],counts=totals)
    calls=totals.get('EquipmentUpdateCalls',0);ns=totals.get('EquipmentUpdateNanoseconds',0)
    row['update_mean_microseconds']=ns/calls/1000 if calls else None
    row['update_cpu_ms_per_wall_second']=ns/1e6/block['seconds']
    rows.append(row)
groups=[]
for legacy in [True,False]:
    rs=[r for r in rows if r['legacy']==legacy]
    pooled=[s['metrics'] for s in j['samples'] if j['blocks'][s['block']]['legacy']==legacy]
    groups.append(dict(legacy=legacy,samples=len(pooled),mean_block_medians=[statistics.mean(r['median'][i] for r in rs) for i in range(4)],pooled_median=[statistics.median(v) for v in zip(*pooled)],pooled_p95=[p95(v) for v in zip(*pooled)],local_created=sum(r['counts'].get('LocalCreated',0) for r in rs),local_destroyed=sum(r['counts'].get('LocalDestroyRequested',0) for r in rs),notifications=sum(r['counts'].get('InventoryNotify',0) for r in rs),self_time_microseconds=statistics.mean(r['update_mean_microseconds'] for r in rs)))
result=dict(passed=j['passed'],errors=j['errors'],blocks=rows,groups=groups)
(OUT/'performance_summary.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print(json.dumps(result,indent=2))
