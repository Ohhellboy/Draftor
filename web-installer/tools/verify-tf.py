#!/usr/bin/env python3
"""Verify only public downloaded archives; never opens disks, serial or TF cards."""
from pathlib import Path
import hashlib
import json
import zipfile

root = Path(__file__).resolve().parents[1]
manifest = json.loads((root / 'public/firmware/TF_RESOURCES.json').read_text())
inputs = root / 'release-inputs'
archives = [
    ('Draftor_v0.1.0-alpha.2.1_Resources.zip', 60546074,
     '460096846036bd01c941bc07ab98e6db4e130a247563e44fa3cb01d12a9c6636', manifest['files'][:-2]),
    ('Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_LearningIndex.zip', 1831828,
     '05da4128d132c0673ae28bf9a1f35d3997a017cd1995d51d38051dc15f55c3eb', manifest['files'][-2:]),
]
verified = []
for name, size, digest, files in archives:
    archive = inputs / name
    assert archive.is_file() and not archive.is_symlink(), 'Use an ordinary public archive: ' + name
    data = archive.read_bytes()
    assert len(data) == size and hashlib.sha256(data).hexdigest() == digest, name
    with zipfile.ZipFile(str(archive)) as bundle:
        for item in files:
            content = bundle.read('Resources/' + item['tf_path'])
            assert len(content) == item['bytes'], item['tf_path']
            assert hashlib.sha256(content).hexdigest() == item['sha256'], item['tf_path']
            verified.append(dict(item, verified=True))
receipt = dict(result='PASS', files=verified, actualTFCardAccess=False,
               resourceSource='Exact public alpha.2.1 Resources + alpha.3 LearningIndex',
               shared_bundle_id=str(manifest['shared_bundle_id']))
(root / 'evidence/tf-resources.json').write_text(json.dumps(receipt, indent=2) + '\n')
print('PASS: 18 exact public TF resource files; no physical TF card accessed.')
