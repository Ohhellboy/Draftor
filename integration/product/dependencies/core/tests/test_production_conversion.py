#!/usr/bin/env python3
"""Small rejection/independence checks around actual-source conversion."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import production_resources as p

def rejects(fn):
    try:fn()
    except ValueError:return
    raise AssertionError('operation should reject')

root=Path(__file__).resolve().parents[1]
src=root/'resources/production/source_v1_0.zip'
reg=root/'registry/production'
builder=Path(sys.argv[1]).resolve()
z,lex=p.source(src)
m=p.check_registry(reg,lex)
assert len(lex)==m['adopted_keys']==242793
assert {r['original_text'] for r in lex if not r['source_record_refs']}=={'候选词','草稿机'}
print('PASS C1 actual source / approved registry / explicit product provenance')
with tempfile.TemporaryDirectory(prefix='hanime-conversion-guards-') as tmp:
    t=Path(tmp);copy=t/'registry';shutil.copytree(reg,copy)
    bad=dict(m,namespace='0x54465249414c0001');p.dump(copy/'REGISTRY.json',bad)
    rejects(lambda:p.check_registry(copy,lex))
    p.dump(copy/'REGISTRY.json',m)
    altered=[dict(lex[0],stable_lexeme_key='SLK1:'+'1'*64)]+lex[1:]
    rejects(lambda:p.check_registry(copy,altered))
    assert (copy/'lexemes.tsv.gz').read_bytes()==(reg/'lexemes.tsv.gz').read_bytes()
    print('PASS C2 trial namespace / changed identity rejected, registry unchanged')
    # Separate explicit adoption must not replace an existing authority.
    rejects(lambda:p.adopt(src,copy))
    print('PASS C3 create-only adoption rejects existing registry')
    o,pending=p.outputs(z,lex)
    keyed={r['original_text']:r['stable_lexeme_key'] for r in lex}
    fa=o[keyed['发']]
    assert sum(r[0]=='opencc-s2t-1.4.2' and bool(r[2]) and not r[4] for r in fa)==2
    assert any(r[0]=='relation:R028' and r[4] for r in o[keyed['面条']])
    assert sum(r['kind']=='FORM_NO_EXACT_KEY' for r in pending)==11
    assert len({keyed[x] for x in ['机','機','发','發','髮']})==5
    print('PASS C4 conditions/multiple paths/Blocked/unmapped forms, no text identity merge')
    # Deliberately tiny build-side subset, NOT another registry. Both full keys
    # already exist in R1. It has a full-pinyin-only record and no Xiaohe input.
    norm=t/'full-only.txt';out=t/'full-only'
    lines=['HANIME_REGISTERED_V1 '+str(p.NS)]
    for text in ['时','時']:
        k=keyed[text]
        lines.append(f'L {k} 1 {p.token(text)} {p.token("existing R1 identity")} {p.token("independence test subset")} 0')
        lines.append(f'C {k} full_pinyin shi 0')
    norm.write_text('\n'.join(lines)+'\n')
    subprocess.run([str(builder),str(norm),str(out)],check=True)
    assert (out/'full_pinyin.hnm').exists() and not (out/'xiaohe.hnm').exists()
    print('PASS C5 full-pinyin-only build accepted; no intermediate Xiaohe requirement')
z.close()
print('CONVERSION GUARDS 5/5')
