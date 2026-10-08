#!/usr/bin/env python3
"""Check fixed gates and compare every phase, without turning Host ns into S3 ms."""
import argparse
import json
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--evidence',type=Path,required=True)
a=p.parse_args(); ev=a.evidence.resolve(); matrix=ev/'final_matrix'; san=ev/'final_matrix_san'
names=['baseline','off','output','anchors','balanced','extra']
data={n:json.loads((matrix/n/'METRICS.json').read_text()) for n in names}
fields=['bytes','reads','seeks','steps','work','local_work','budget_port_calls','budget_port_bytes']
semantic=['ITEM','MEMBER','DOCUMENT','PAGING','STATE']
def equal_results(x,y):
    for k in semantic: assert x.get(k,[])==y.get(k,[]),k
def costs(d): return {x['phase']:x for x in d['COST']}
def total(d): return {k:sum(x[k] for x in d['COST']) for k in fields}
def components(d):
    m=d['MEMORY'][0]
    fixed=sum(m[k] for k in ['extension_owner','extension_impl','extension_composition_buffers',
        'extension_rank_arrays','extension_page','extension_output','shared_fixed','shared_page_payload','shared_page_metadata'])
    for c in d['CACHE_MEMORY']:
        if c['case']==m['case']: fixed+=sum(c.get(k,0) for k in ['fixed','data_payload','proof_payload','data_metadata','proof_metadata','anchor_bytes'])
    if d.get('OUTPUT_MEMORY'):
        o=d['OUTPUT_MEMORY'][0];fixed+=sum(o[k] for k in ['fixed','metadata','text','forms'])
    # OutputReader is already inside the owning Impl. Do not add its sizeof.
    return fixed
def submission(d):
    return sum(d['SUBMISSION_MEMORY'][0][k] for k in ['owner','impl','capture','core_impl','members',
        'request_names','resolver','member_text','total_text','owner_token','receipt'])
base=costs(data['baseline']); result={}; adverse={}
for n,d in data.items():
    equal_results(data['baseline'],d); assert list(costs(d))==list(base) and len(base)==355
    result[n]=dict(total=total(d),selected_components=components(d),submission=submission(d),
                   diagnostic_runtime=d['MEMORY'][0]['sizeof_runtime'])
    adverse[n]=[{"phase":phase,**{k:{'before':base[phase][k],'after':c[k]} for k in fields if c[k]>base[phase][k]}}
                for phase,c in costs(d).items() if any(c[k]>base[phase][k] for k in fields)]
for phase,c in costs(data['off']).items():
    assert all(c[k]==base[phase][k] for k in fields),'default off behavior/cost changed'
b=result['balanced']['total']
assert b['bytes']<=3547416 and b['reads']<=3471 and b['work']<=7447494
current=costs(data['balanced'])
assert sum(x['bytes'] for phase,x in current.items() if phase.startswith('cold_ui/') and phase.endswith('_activation'))<=15400
for phase in ['cold_ui/xiaohe/visible_text','segments_xh/xiaohe/remaining_le_visible_text','segments_py/full_pinyin/remaining_le_visible_text']:
    assert current[phase]['reads']<=18 and current[phase]['bytes']<=18432
memory_delta=components(data['balanced'])-components(data['baseline'])
memory_delta+=submission(data['balanced'])-submission(data['baseline'])
memory_delta+=data['balanced']['MEMORY'][0]['sizeof_runtime']-data['baseline']['MEMORY'][0]['sizeof_runtime']
assert memory_delta<=0,'same-budget comparison exceeded listed simultaneous components'
reuse_base=json.loads((matrix/'baseline_reuse/METRICS.json').read_text())
reuse=json.loads((matrix/'balanced_reuse/METRICS.json').read_text())
equal_results(reuse_base,reuse)
for c in reuse['COST']:
    if 'hot_text' in c['phase'] or 'cross_scheme_text' in c['phase']:
        assert c['reads']==c['bytes']==c['seeks']==0,'fully resident output still did I/O'
for label,reference in [('balanced',data['balanced']),('balanced_reuse',reuse)]:
    instrumented=json.loads((san/label/'METRICS.json').read_text());equal_results(reference,instrumented)
    assert list(costs(reference))==list(costs(instrumented))
    for phase,c in costs(reference).items():
        assert all(c[k]==costs(instrumented)[phase][k] for k in fields),'sanitized deterministic cost drift'
phases=[{'phase':phase,**{n:{k:costs(d)[phase][k] for k in fields} for n,d in data.items()}} for phase in base]
revisit=[{'phase':phase,'baseline':{k:costs(reuse_base)[phase][k] for k in fields},
          'balanced':{k:c[k] for k in fields}} for phase,c in costs(reuse).items()]
summary=dict(status='PASS',profiles=result,phases=355,revisit_phases=len(revisit),
             gates='Original DESIGN.md gates, not changed after experiments',
             selected_simultaneous_delta_bytes=memory_delta,
             memory_scope='Listed owners/caches/output pool/extension buffers plus runtime delta and simultaneously alive Submission. Not allocator controls, all malloc, complete call-chain stack or device peak.',
             sanitizer='AppleClang address/undefined, strict warnings; leak detection off',
             device='10MHz daily performance FAIL / reliability OPEN; no new device/GCC run',
             adverse_phase_counts={n:len(rows) for n,rows in adverse.items()})
for filename,content in [('SUMMARY.json',summary),('PHASE_COMPARISON.json',phases),('ADVERSE_PHASES.json',adverse),('REVISIT_COMPARISON.json',revisit)]:
    (ev/filename).write_text(json.dumps(content,ensure_ascii=False,indent=2)+'\n')
print(json.dumps(summary,ensure_ascii=False,indent=2))
