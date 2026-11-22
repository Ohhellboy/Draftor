"""Validate the corresponding source and exclude private/workspace inputs."""
from pathlib import PurePosixPath
import json,zipfile,sys,hashlib
with zipfile.ZipFile(sys.argv[1]) as z:
    names=z.namelist(); assert len(names)==len(set(names))
    identity=json.loads(z.read('SOURCE_IDENTITY.json'));assert identity['sourceCommit']==sys.argv[2]
    assert set(names)==set(identity['files'])|{'SOURCE_IDENTITY.json'}
    for n,pin in identity['files'].items():
        assert not n.startswith('/') and '..' not in PurePosixPath(n).parts
        assert not n.startswith(('delivery/','evidence/','.draftor_private/','.git/','.test-cache/'))
        b=z.read(n);assert hashlib.sha256(b).hexdigest()==pin
        if n.endswith(('.js','.mjs','.py','.html','.json','.md')):
            for private in [b'/Volumes/'+b'OnlyRos/',b'/Users/'+b'wangruotao/',b'Flash_ReadOnly_'+b'Backup_',b'R9_'+b'CURRENT']:
                assert private not in b,(n,private)
    for n in ['src/public-install-session.js','src/public-ui.js','src/r10-main.js','src/r10-recovery.js','src/r10-online-recovery.js','tools/build-r10.mjs','tools/package-r10-source.py','tools/validate-r10-pages.mjs','package-lock.json','docs/R10_BUILD.md']:
        assert n in names,n
print('SOURCE_HASHES_AND_PRIVACY_PASS')
