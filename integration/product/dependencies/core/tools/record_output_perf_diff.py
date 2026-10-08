#!/usr/bin/env python3
"""Export only this core task's record additions, preserving concurrent windows."""
import argparse
import difflib
import hashlib
import json
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
a=p.parse_args();core=a.root.resolve();ev=core/'evidence/output_perf_v1'
titles={
 '00-当前状态.md':['HanIME通用输出性能候选完成（2026-09-13）'],
 '01-工程日志.md':['2026-09-13 — 核心通用输出复用／冷访问优化交审','2026-09-13 — 核心通用输出性能任务开工／调优前预算'],
 '03-待验证事项.md':['CORE_VISIBLE_OUTPUT_PERF_V1_0接收与设备门（2026-09-13）']}
diff=[];result={}
for name,heads in titles.items():
    before=(ev/'record_before'/name).read_text();current=(core.parent/name).read_text();blocks=[];other=current
    for title in heads:
        marker='## '+title+'\n';start=current.index(marker);end=current.find('\n## ',start+len(marker))
        if end<0: end=len(current)
        block=current[start:end].rstrip()+'\n\n';blocks.append(block)
        assert marker not in before,'not a new task record'
        other=other.replace(current[start:end],'',1)
    line_end=before.index('\n')+1
    projected=before[:line_end]+'\n'+''.join(blocks)+before[line_end:].lstrip('\n')
    diff.extend(difflib.unified_diff(before.splitlines(True),projected.splitlines(True),
        fromfile='before_task/'+name,tofile='this_task_only/'+name))
    result[name]=dict(current_sha256=hashlib.sha256(current.encode()).hexdigest(),
                     task_added_titles=heads,other_window_deltas_retained=other.strip()!=before.strip())
name='02-设计决策.md'
result[name]=dict(edited_by_this_task=False,
    equal_to_start_snapshot=(core.parent/name).read_bytes()==(ev/'record_before'/name).read_bytes())
(ev/'WORKSPACE_RECORDS.diff').write_text(''.join(diff))
(ev/'WORKSPACE_RECORD_SCOPE.json').write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print('PASS scoped 00/01/03 additions exported; 02 not edited; no records written')
