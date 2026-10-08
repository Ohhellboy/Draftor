#!/usr/bin/env python3
"""Independent Host reference from frozen source, not converted manifest.
Full vectors/sorts are TEST-ONLY, never linked into the runtime.
"""
import csv
import gzip
import io
import json
from pathlib import Path
import sys
import zipfile
from collections import defaultdict

def generate(src, out, reverse=False):
    out.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(src) as z:
        def table(name):
            b=z.read('HANIME_Production_Resource_Handoff_v1.0/data/'+name+'.tsv.gz')
            return list(csv.DictReader(io.StringIO(gzip.decompress(b).decode()),delimiter='\t'))
        lex=table('lexemes')
        lex.sort(key=lambda r:r['stable_lexeme_key'],reverse=reverse)
        by_key={r['stable_lexeme_key']:(i+1,r) for i,r in enumerate(lex)}
        by_text={r['original_text']:(i+1,r['stable_lexeme_key']) for i,r in enumerate(lex)}
        with (out/'outputs.txt').open('w') as f:
            for original,form,inv,status,text in [
                ('机','relation:R002','E','Exact','機'),('机器','relation:R004','E','Exact','機器'),
                ('发','opencc-s2t-1.4.2','P','Multiple',''),('后','relation:R011','E','Condition',''),
                ('面条','relation:R028','E','Blocked',''),('才','relation:R036','P','Denied',''),
                ('才','relation:R036','E','Condition',''),('人','product-default-traditional','P','Blocked',''),
                ('人','not-in-this-package','E','Fallback','人'),
                ('时','m6-traditional','E','Exact','時'),('時','m6-traditional','E','Exact','時')]:
                ref,k=by_text[original]
                f.write(f'{ref} {k} {form} {inv} {status} {text.encode().hex() or "-"}\n')
        with (out/'lexemes.txt').open('w') as f:
            for i,r in enumerate(lex):
                f.write(f"{i+1} {r['stable_lexeme_key']} {r['base_weight']} {r['original_text'].encode().hex()}\n")
        stats={}
        for scheme in ['full_pinyin','xiaohe']:
            rows=table(scheme+'_mappings')
            groups=defaultdict(list)
            for i,r in enumerate(rows):
                k=r['stable_lexeme_key'];ref,l=by_key[k]
                groups[r['full_code' if scheme=='full_pinyin' else 'code']].append((ref,int(r['scheme_weight']),i,k,int(l['base_weight'])))
            with (out/(scheme+'_raw.txt')).open('w') as f:
                for code in sorted(groups):
                    for ref,w,i,k,base in sorted(groups[code],key=lambda v:(v[0],-v[1],v[2])):
                        f.write(f'{code} {ref} {w} {i} {k}\n')
            # All ranges for the five largest fanouts, first/last and longest
            # codes plus identities relevant to shared forms / multiple readings.
            codes=set(sorted(groups,key=lambda c:(-len(groups[c]),c))[:5])
            codes.update([min(groups),max(groups),max(groups,key=len)])
            codes.update(c for c in ['ren','rf','ji','wei','ww','jiqi','fa','shi','ui','wen','wf'] if c in groups)
            with (out/(scheme+'_rank.txt')).open('w') as f:
                for code in sorted(codes):
                    grouped={}
                    for ref,w,i,k,base in groups[code]:
                        if k not in grouped:grouped[k]=[ref,base,w,1]
                        else:
                            grouped[k][2]=max(grouped[k][2],w);grouped[k][3]+=1
                    result=sorted(((v[1]+v[2],k,v[0],v[3]) for k,v in grouped.items()),key=lambda v:(-v[0],v[1]))
                    f.write(f'{code} {len(result)}\n')
                    for score,k,ref,paths in result:f.write(f'{k} {ref} {score} {paths}\n')
            stats[scheme]=dict(raw_rows=len(rows),distinct_codes=len(groups),rank_queries=sorted(codes),
                ranked_items=sum(len({r[3] for r in groups[c]}) for c in codes),max_fanout=max(map(len,groups.values())))
    (out/'oracle.json').write_text(json.dumps(stats,indent=2)+'\n')
    print('ORACLE',json.dumps(stats))

if __name__=='__main__':generate(Path(sys.argv[1]),Path(sys.argv[2]),len(sys.argv)>3 and sys.argv[3]=='B')
