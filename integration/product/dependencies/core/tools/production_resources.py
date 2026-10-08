#!/usr/bin/env python3
"""Host-only explicit registry admission OR read-only conversion. Never runtime.

adopt is a separate, authorized, create-only registry operation. Ordinary builds
use convert, which requires the frozen registry and cannot allocate identities.
All upstream rows remain in the frozen source ZIP; trace maps give exact rows.
"""
import argparse
import csv
import gzip
import hashlib
import io
import json
from pathlib import Path
import re
import resource
import subprocess
import zipfile
from convert_step2f import validate_tables, AVAIL

SOURCE_SHA = '4c0ebab903f19d2d81bd0a1142e363ff0eadd69c3d7dfc4e9db0085e33faf920'
NS = 0x48414e494d450001
PREFIX = 'HANIME_Production_Resource_Handoff_v1.0/'

def require(ok, message):
    if not ok:
        raise ValueError(message)

def sha(data):
    return hashlib.sha256(data).hexdigest()

def canonical(obj):
    return json.dumps(obj, ensure_ascii=False, sort_keys=True, separators=(',', ':')).encode()

def dump(path, obj):
    path.write_text(json.dumps(obj, ensure_ascii=False, sort_keys=True, indent=2)+'\n')

def gz_table(path, fields, rows):
    with path.open('wb') as raw, gzip.GzipFile(filename='', mode='wb', fileobj=raw, mtime=0) as gz:
        with io.TextIOWrapper(gz, encoding='utf-8', newline='') as f:
            w = csv.writer(f, delimiter='\t', lineterminator='\n')
            w.writerow(fields)
            w.writerows(rows)

def rows(z, name):
    data = z.read(PREFIX+name)
    if name.endswith('.gz'):
        data = gzip.decompress(data)
    return list(csv.DictReader(io.StringIO(data.decode('utf-8')), delimiter='\t'))

def source(path):
    require(sha(path.read_bytes()) == SOURCE_SHA, 'source ZIP is not frozen v1.0')
    z = zipfile.ZipFile(path)
    require(z.testzip() is None, 'source CRC')
    lex = rows(z, 'data/lexemes.tsv.gz')
    require(len(lex) == 242793, 'source lexeme count')
    seen = set()
    for r in lex:
        k = r['stable_lexeme_key']
        require(re.fullmatch(r'SLK1:[0-9a-f]{64}', k) and k != 'SLK1:'+'0'*64 and k not in seen, 'duplicate/invalid key')
        seen.add(k)
        require(r['admission_disposition'] == 'ADMIT' and r['default_output_rule'] == 'ORIGINAL_SOURCE_TEXT', 'source admission')
        require(r['identity_policy_id'] in ('HANIME_ORTHOGRAPHIC_COMMIT_ENTRY_V1', 'EXPLICIT_PRODUCT_BACKCOMPAT'), 'unknown identity policy')
        require(r['source_record_refs'] or (r['admission_reason'] == 'EXPLICIT_PRODUCT_BACKCOMPAT' and
                r['authority_sources'] == 'HANIME_PRODUCT_ADMISSION_POLICY_V1'), 'missing provenance')
        require(bytes.fromhex(r['original_utf8_hex']).decode() == r['original_text'], 'UTF-8 mismatch')
        require(r['record_kind'] == ('character' if len(r['original_text']) == 1 else 'word'), 'Unicode kind')
        require(-(2**31) <= int(r['base_weight']) < 2**31, 'base weight range')
    return z, lex

def registry_row(r):
    return [r['stable_lexeme_key'], r['original_text'], r['record_kind'], r['identity_policy_id'], sha(canonical(r))]

REG_FIELDS = ['stable_key', 'original_text', 'kind', 'inherited_policy', 'source_row_sha256']

def adopt(src, directory):
    z, lex = source(src)
    require(not directory.exists(), 'registry is create-only; existing authority must not be overwritten')
    directory.mkdir(parents=True)
    gz_table(directory/'lexemes.tsv.gz', REG_FIELDS, sorted(registry_row(r) for r in lex))
    dump(directory/'REGISTRY.json', dict(registry_revision='R1', namespace=f'0x{NS:016x}',
        maintainer='HANIME_Independent_Core', source_zip_sha256=SOURCE_SHA,
        adopted_keys=len(lex), table_sha256=sha((directory/'lexemes.tsv.gz').read_bytes()),
        authority='User-authorized bulk continuation of traceable existing full keys, 2026-09-10',
        scope='One central HanIME registry; no claim of global namespace uniqueness',
        policy='Original orthographic entries retained. No text-based merges, new keys, or trial state migration.'))
    z.close()
    print('ADOPT PASS R1 keys=242793 namespace='+hex(NS))

def check_registry(directory, lex):
    m = json.loads((directory/'REGISTRY.json').read_text())
    require(m['namespace'] == f'0x{NS:016x}' and m['source_zip_sha256'] == SOURCE_SHA, 'registry authority/source mismatch')
    require(sha((directory/'lexemes.tsv.gz').read_bytes()) == m['table_sha256'], 'registry table hash')
    with gzip.open(directory/'lexemes.tsv.gz', 'rt') as f:
        actual = list(csv.reader(f, delimiter='\t'))
    require(actual == [REG_FIELDS] + sorted(registry_row(r) for r in lex), 'registry does not authorize these exact source rows')
    return m

def outputs(z, lex):
    tables = {n: rows(z, 'semantic_regression/'+n+'.tsv') for n in
              ('forms', 'identity_evidence', 'relations', 'output_candidates', 'same_text_identity_cases', 'pending_items')}
    c = json.loads(z.read(PREFIX+'semantic_regression/relation_semantics_contract_v2.json'))
    validate_tables(tables, c)
    forms = {r['handoff_form_key']: r for r in tables['forms']}
    known = {r['stable_lexeme_key']: r for r in lex}
    by_key = {}
    pending = []
    def put(k, form, text, condition='', availability=7, blocked=False, basis=''):
        by_key.setdefault(k, []).append((form, text, condition, availability, int(blocked), basis))
    for f in forms.values():
        k = f['m4_stable_lexeme_key']
        if k:
            require(k in known and known[k]['original_text'] == f['text'], 'semantic identity mismatch')
        else:
            pending.append(dict(kind='FORM_NO_EXACT_KEY', form=f['handoff_form_key'], text=f['text']))
    for r in tables['relations']:
        k = forms[r['source_form_key']]['m4_stable_lexeme_key']
        if not k:
            pending.append(dict(kind='OWNER_NO_EXACT_KEY', relation=r['legacy_relation_id']))
            continue
        blocked = r['review_status'] == 'user_standard_choice_needed'
        for form in ['relation:'+r['legacy_relation_id']] + [s for s in r['profile_keys'].split('|') if s]:
            put(k, form, r['target_text'], r['runtime_condition'], AVAIL[r['availability']], blocked, r['relation_edge_id'])
        if blocked:
            pending.append(dict(kind='PROFILE_REVIEW', relation=r['legacy_relation_id']))
    # Frozen compatibility is an explicit form, never a product default.
    # Where the reviewed sample gives conditions, retain every matching edge;
    # an alias must not bypass ambiguity, admission or invocation permissions.
    for r in rows(z, 'data/existing_output_attachments.tsv'):
        k = r['owner_stable_lexeme_key']
        matched = [e for e in tables['relations'] if e['source_text'] == r['source_text'] and e['target_text'] == r['target_text']]
        if matched:
            for e in matched:
                put(k, 'm6-traditional', r['target_text'], e['runtime_condition'], AVAIL[e['availability']], e['review_status'] == 'user_standard_choice_needed', e['relation_edge_id'])
        else:
            put(k, 'm6-traditional', r['target_text'], availability=6, basis='Frozen M6 output, not full language review')
    # For the four equal-output groups the original traditional identity also
    # explicitly has this unchanged compatibility form. Never merge its key.
    for r in rows(z, 'data/same_text_output_groups.tsv'):
        k = r['source_stable_lexeme_key']
        if not any(o[0] == 'm6-traditional' for o in by_key.get(k, [])):
            put(k, 'm6-traditional', known[k]['original_text'], availability=6, basis='Original identity / equal payload only')
    # Tool observations lacking an admitted route stay known but Blocked.
    for r in tables['output_candidates']:
        k = forms[r['source_form_key']]['m4_stable_lexeme_key']
        if k and r['candidate_role'] == 'TOOL_OUTPUT_OBSERVATION' and not any(o[0] == r['profile_key'] for o in by_key.get(k, [])):
            put(k, r['profile_key'], '', blocked=True, basis='Observation alone does not authorize output')
    return by_key, pending

def token(s):
    return s.encode('utf-8').hex() or '-'

def convert(src, directory, builder, out, reverse=False):
    z, lex = source(src)
    registry = check_registry(directory, lex)
    relations, pending = outputs(z, lex)
    out.mkdir(parents=True, exist_ok=True)
    lex.sort(key=lambda r: r['stable_lexeme_key'], reverse=reverse)
    refs = {r['stable_lexeme_key']: i+1 for i,r in enumerate(lex)}
    weights = {r['stable_lexeme_key']: int(r['base_weight']) for r in lex}
    gz_table(out/'runtime_keys.tsv.gz', ['runtime_ref','stable_key','source_record','source_row_sha256'],
             ([i+1,r['stable_lexeme_key'],r['source_record_id'],sha(canonical(r))] for i,r in enumerate(lex)))
    mapping_counts = {}
    with (out/'build_input.txt').open('w') as f:
        f.write('HANIME_REGISTERED_V1 '+str(NS)+'\n')
        for r in lex:
            k = r['stable_lexeme_key']
            f.write('L '+' '.join([k, '1' if r['record_kind']=='character' else '2', token(r['original_text']),
                token(r['identity_policy_id']), token(r['source_record_id']), r['base_weight']])+'\n')
            local = [('source-preserve',r['original_text'],'',7,0,'Original registered text'),
                     ('product-default-traditional','','',7,1,'Product profile not selected')] + relations.get(k,[])
            for form,text,condition,availability,blocked,basis in local:
                f.write('O '+' '.join([k,token(form),token(text),token(condition),str(availability),str(blocked),token(basis)])+'\n')
        for scheme,name,code_field in [('full_pinyin','full_pinyin_mappings','full_code'),('xiaohe','xiaohe_mappings','code')]:
            table = rows(z, 'data/'+name+'.tsv.gz')
            trace = []
            for ordinal,r in enumerate(table):
                k = r['stable_lexeme_key']; weight = int(r['scheme_weight']); code = r[code_field]
                require(k in refs and int(r['base_weight']) == weights[k], 'mapping authority/weight mismatch')
                require(re.fullmatch('[a-z]+',code) and -(2**31)<=weight<2**31, 'invalid mapping')
                f.write('C '+' '.join([k,scheme,code,str(weight)])+'\n')
                trace.append([ordinal,refs[k],k,code,weight,r['mapping_source_record']])
            gz_table(out/(scheme+'_trace.tsv.gz'), ['ordinal','runtime_ref','stable_key','code','scheme_weight','source_record'], trace)
            mapping_counts[scheme] = len(table)
    subprocess.run([str(builder.resolve()),str(out/'build_input.txt'),str(out)],check=True)
    # These are generated in this call, not user data. Keep compact summary and
    # exact normalized digest; the source/registry/traces reproduce the big files.
    normalized_sha = sha((out/'build_input.txt').read_bytes())
    built = json.loads((out/'builder_manifest.json').read_text())
    built.pop('identity_trace', None)
    dump(out/'builder_summary.json', built)
    (out/'build_input.txt').unlink()
    (out/'builder_manifest.json').unlink()
    dump(out/'manifest.json', dict(source_sha256=SOURCE_SHA, registry=registry,
        normalized_input_sha256=normalized_sha,
        order='key-descending' if reverse else 'key-ascending', mapping_counts=mapping_counts,
        keys=len(lex), output_relation_count=2*len(lex)+sum(map(len,relations.values())),
        pending=pending, pending_source_tables=['data/pending_scheme_mappings.tsv.gz','data/full_pinyin_alignment_issues.tsv.gz','pending/PENDING_ITEMS.tsv'],
        trace='runtime_keys + per-scheme ordinal trace join frozen source; complete source provenance is not copied into runtime candidates',
        default='ORIGINAL_SOURCE_TEXT', profile='NOT_SELECTED',
        dedup='string payload sharing only; display groups by ref; distinct keys never merged',
        files={p.name:sha(p.read_bytes()) for p in sorted(out.iterdir()) if p.suffix in ('.hns','.hnm','.gz')}))
    z.close()
    print('CONVERT PASS registered='+str(len(lex))+' mappings='+str(mapping_counts)+' order='+('B' if reverse else 'A'))
    print('HOST_BUILD_RSS native_ru_maxrss converter='+str(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss)+
          ' builder_child='+str(resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss)+
          ' (Darwin bytes; Linux KiB; separate process peaks, NOT runtime memory)')

if __name__ == '__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('operation',choices=['adopt','convert'])
    p.add_argument('--source',type=Path,required=True)
    p.add_argument('--registry',type=Path,required=True)
    p.add_argument('--builder',type=Path)
    p.add_argument('--out',type=Path)
    p.add_argument('--reverse',action='store_true')
    a=p.parse_args()
    try:
        if a.operation=='adopt': adopt(a.source,a.registry)
        else:
            require(a.builder and a.out,'convert needs --builder and --out')
            convert(a.source,a.registry,a.builder,a.out,a.reverse)
    except (ValueError,KeyError,OSError,subprocess.CalledProcessError) as e:
        p.exit(1,'REJECT '+str(e)+'\n')
