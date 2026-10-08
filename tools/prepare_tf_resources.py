# SPDX-License-Identifier: Apache-2.0
# Copyright 2026 Ruotao Wang
"""Prepare a new, explicitly selected offline B2 directory; no device access.

Packaging adaptation of the accepted whole-directory staging procedure.
Original payload digest verification and runtime authentication are preserved.
"""
from pathlib import Path
import argparse
import hashlib
import json
import shutil

RELEASE = 'LEXICON_R3_B2_20261006'
FIRMWARE = 'DRAFTOR_S3_ALPHA_T06_TF20_R1_20261007'

def expected(folder):
    manifest = json.loads((folder/'release_manifest.json').read_text())
    if manifest['status'] != 'HOST_FINAL_BYTES_VALIDATED':
        raise ValueError('Unexpected manifest status')
    files = {}
    for row in manifest['files']:
        for name_key,size_key,hash_key in [('source_file','file_bytes','file_sha256'),('index_file','index_bytes','index_sha256')]:
            name = row[name_key]
            if Path(name).name != name or name in files:
                raise ValueError('Unsafe or duplicate resource member')
            files[name] = (row[size_key],row[hash_key])
    if len(files) != 14 or sum(v[0] for v in files.values()) != 126940251:
        raise ValueError('Unexpected B2 payload set')
    return files

def validate(folder, files):
    for name,(size,digest) in files.items():
        p = folder/name
        if p.is_symlink() or not p.is_file() or p.stat().st_size != size:
            raise ValueError('Missing/size-mismatched resource: '+name)
        h = hashlib.sha256()
        with p.open('rb') as f:
            for chunk in iter(lambda:f.read(1024*1024),b''):
                h.update(chunk)
        if h.hexdigest() != digest:
            raise ValueError('Resource digest mismatch: '+name)

def ready(folder):
    value = dict(release=RELEASE,firmware=FIRMWARE,files=14,
        validation='all final staged bytes checked against trusted software manifest; runtime pins remain authoritative')
    (folder/'CANDIDATE_READY.json').write_text(json.dumps(value,indent=2)+'\n')

def stage(resources,target,write=False):
    origin = resources/'HANIME/IME'/RELEASE
    if any(p.is_symlink() for p in [resources,resources/'HANIME',resources/'HANIME/IME',origin]):
        raise ValueError('Resource path must not be a symlink')
    files = expected(origin)
    if target.name != RELEASE or target.parent.name != 'IME' or target.parent.parent.name != 'HANIME':
        raise ValueError('Target must be a new HANIME/IME/'+RELEASE)
    if target.exists() or target.is_symlink():
        raise ValueError('Never replace an existing release directory')
    temp = target.with_name('.'+RELEASE+'.staging')
    if temp.exists() or temp.is_symlink():
        raise ValueError('Interrupted stage retained; investigate it first')
    plan = dict(status='DRY_RUN',target=str(target),files=14,payload_bytes=126940251)
    if not write:
        validate(origin,files)
        return plan
    if not target.parent.is_dir() or any(p.is_symlink() for p in target.parents):
        raise ValueError('Target parent must exist without symlink ancestors')
    temp.mkdir()
    (temp/'STAGING_INCOMPLETE').write_text('Not an active resource root.\n')
    for name in [*files,'release_manifest.json','release_pin.hpp']:
        src=origin/name
        if src.is_symlink():
            raise ValueError('Source member must not be a symlink')
        shutil.copyfile(src,temp/name)
    validate(temp,files)
    ready(temp)
    (temp/'STAGING_INCOMPLETE').unlink()
    temp.rename(target)
    plan['status']='STAGED_COMPLETE_NO_DEVICE_OPERATION'
    return plan

if __name__ == '__main__':
    p=argparse.ArgumentParser()
    p.add_argument('--resources',type=Path,required=True)
    p.add_argument('--target',type=Path,required=True)
    p.add_argument('--write',action='store_true')
    a=p.parse_args()
    print(json.dumps(stage(a.resources.absolute(),a.target.absolute(),a.write),indent=2))
