#!/usr/bin/env python3
"""Regenerate the fixed format4 release from the supplied format3 predecessor.
This is optional; installing/building Alpha 2 does not regenerate dictionaries.
"""
from pathlib import Path
import argparse,shutil,json,struct,sys
sys.path.insert(0,str(Path(__file__).resolve().parent/'resources'))
import structural as g
from short_text import NAMES,EXT,parse_shared,make_short,compare_all
def copy_exact(path,src):
 path.parent.mkdir(parents=True,exist_ok=True)
 if path.exists():raise RuntimeError('Output already exists: '+str(path))
 shutil.copyfile(src,path)
g.link_exact=copy_exact
def main():
    if not __debug__:raise SystemExit('Do not use Python -O')
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--input-format3',type=Path,required=True)
    ap.add_argument('--output',type=Path,required=True)
    args=ap.parse_args();src=args.input_format3.resolve()
    if args.output.exists():raise SystemExit('Choose a new output directory')
    manifest = json.loads((src / 'RESOURCE_MANIFEST.json').read_text())
    source_rows = {row['name']: row for row in manifest['files']}
    pintexts = {name: (src / 'pins' / file).read_text() for name, file in
                [('release', 'release_pin.hpp'), ('block', 'block_release_pin.hpp'), ('wubi', 'wubi_release_v1.hpp')]}
    pins = {n: g.pin_section(pintexts['wubi' if n.startswith('wubi') else 'block' if n.endswith('_base') else 'release'],
                            'block_' + n[:-5] if n in ('xiaohe_base', 'full_pinyin_base') else n) for n in NAMES}
    old = (src / 'shared.hns').read_bytes()
    assert g.sha(old) == g.pin_digest(pins['shared'], 'file_sha256')
    assert g.pin_number(pins['shared'], 'resource_format') == 3
    oi = parse_shared(old, g)
    assert oi['shared_bundle_id'] == 17962683822279056272
    semantics = g.validate_shared(old, oi)
    assert (semantics['all_runtime_refs'], semantics['all_strings'], semantics['all_relations']) == (242979, 243106, 486143)
    print('validated measured format3 source, complete default/general semantics', flush=True)
    new = make_short(old, oi, g)
    ni = parse_shared(new, g)
    equality = compare_all(old, oi, new, ni, g)
    print('generated and compared all format4 identities/defaults/general records', flush=True)
    changed = {'shared': new}
    newproof, newroot, newsum = {}, {}, {'shared': g.sha(new)}
    newproof['shared'], newroot['shared'] = g.packed_proof(new)
    schemes, encoded = {}, set()
    for n in ('xiaohe', 'full_pinyin', 'wubi86'):
        original = (src / (n + EXT[n])).read_bytes()
        assert g.sha(original) == g.pin_digest(pins[n], 'file_sha256')
        refs, summary = g.scheme_refs(original, n, semantics['all_runtime_refs'])
        encoded |= refs
        copy = bytearray(original)
        struct.pack_into('<Q', copy, 16, ni['shared_bundle_id'])
        copy = bytes(copy)
        assert copy[:16] == original[:16] and copy[24:] == original[24:]
        newrefs, newsummary = g.scheme_refs(copy, n, semantics['all_runtime_refs'])
        assert refs == newrefs and summary == newsummary
        schemes[n] = summary
        changed[n] = copy
        newsum[n] = g.sha(copy)
        newproof[n], newroot[n] = g.legacy_proof(copy)
        print('preserved scheme business body and rebuilt binding/proof', n, flush=True)
    arms = {}
    for arm in ('SHORT_TEXT_PROTO',):
        dst = args.output.resolve()
        short = arm == 'SHORT_TEXT_PROTO'
        rows, armpins = [], {}
        for n in NAMES:
            for suffix in (EXT[n], '.hnv'):
                filename = n + suffix
                target = dst / filename
                if short and n in changed:
                    data = changed[n] if suffix == EXT[n] else newproof[n]
                    g.write_exact(target, data)
                    digest = newsum[n] if suffix == EXT[n] else g.sha(data)
                    row = {'name': filename, 'bytes': len(data), 'sha256': digest.hex(), 'storage': 'generated'}
                else:
                    assert (src / filename).stat().st_size == source_rows[filename]['bytes']
                    g.link_exact(target, src / filename)
                    row = {**source_rows[filename], 'storage': 'read_only_frozen_dependency',
                           'fingerprint_provenance': 'frozen LAYOUT_PROTO manifest; no repeated unchanged-file hash'}
                rows.append(row)
            pin = pins[n]
            if short:
                numbers = {'shared_bundle_id': ni['shared_bundle_id']}
                digests = {}
                if n in changed:
                    numbers['file_bytes'] = len(changed[n])
                    digests.update(file_sha256=newsum[n], merkle_root=newroot[n])
                if n == 'shared':
                    numbers['resource_format'] = 4
                else:
                    digests['required_shared_sha256'] = newsum['shared']
                if n.endswith('_base'):
                    digests['required_scheme_sha256'] = newsum[n[:-5]]
                pin = g.patch_pin(pin, numbers, digests)
            armpins[n] = pin
        aliases = [armpins[n].replace('block_' + n[:-5], n) for n in ('xiaohe_base', 'full_pinyin_base')]
        pinfiles = {
            'release_pin.hpp': g.header('hanime_release', [armpins[n] for n in ('shared', 'xiaohe', 'full_pinyin')] + aliases),
            'block_release_pin.hpp': g.header('draftor_block_release', [armpins[n] for n in ('xiaohe_base', 'full_pinyin_base')]),
            'wubi_release_v1.hpp': g.header('wubi_release_v1', [armpins[n] for n in ('shared', 'wubi86', 'wubi86_base')])}
        pinrows = []
        for name, value in pinfiles.items():
            g.write_exact(dst / 'pins' / name, value)
            pinrows.append({'name': name, 'bytes': len(value), 'sha256': g.sha(value).hex()})
        arms[arm] = {'arm': arm, 'trust': 'Compiled pins, never this JSON', 'files': rows,
                     'all_files_bytes': sum(row['bytes'] for row in rows),
                     'logical_shared_bytes': len(new if short else old),
                     'shared_bundle_id': ni['shared_bundle_id'] if short else oi['shared_bundle_id'],
                     'pins': pinrows}
        g.json_exact(dst / 'RESOURCE_MANIFEST.json', arms[arm])
    g.json_exact(args.output/'GENERATION_CHECK.json',{'all_identity_default_general_records_equal':True,'shared_bundle_id':ni['shared_bundle_id'],'full_semantics':semantics,'physical':equality,'schemes':schemes})
    print('Complete format4 resources and pins written:',args.output)
if __name__=='__main__':main()
