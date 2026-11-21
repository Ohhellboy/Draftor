#!/usr/bin/env python3
"""R6 source + default locked site. Never archives the experimental build."""
import hashlib,json,os,shutil,zipfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'delivery/R6_20261121'
EXCLUDED={'node_modules','dist','dist-device-test','dist-r6-locked','dist-first-install-test','.test-cache','test-results','playwright-report','delivery','__pycache__','.git'}
SOURCE_NAME='Draftor_Web_R6_source.zip'
SITE_NAME='Draftor_Web_R6_locked_site.zip'
ALIAS='Draftor_Web_Installer_R1_source.zip'
def sha(b):return hashlib.sha256(b).hexdigest()
def files(root):
    for base,dirs,names in os.walk(root):
        dirs[:]=sorted(d for d in dirs if d not in EXCLUDED and not (Path(base)==ROOT/'release-inputs' and d=='factory'))
        for n in sorted(names):
            p=Path(base)/n
            if n=='.DS_Store' or p.is_symlink():continue
            yield p
def add(z,name,data):
    assert not name.startswith('/') and '..' not in Path(name).parts
    info=zipfile.ZipInfo(name,date_time=(2026,11,21,0,0,0));info.compress_type=zipfile.ZIP_DEFLATED;info.external_attr=0o100644<<16;z.writestr(info,data)
def main():
    OUT.mkdir(exist_ok=True);source=OUT/SOURCE_NAME;site=OUT/SITE_NAME
    assert not source.exists() and not site.exists(),'Keep earlier deliverables; choose a new version instead of overwriting.'
    inventory={};private_mac=None
    if os.environ.get('DRAFTOR_R6_PRIVATE_CONTEXT'):
        private_mac=json.loads(Path(os.environ['DRAFTOR_R6_PRIVATE_CONTEXT']).read_text())['expected_mac'].lower()
    with zipfile.ZipFile(source,'x',zipfile.ZIP_DEFLATED)as z:
        for p in files(ROOT):
            rel=p.relative_to(ROOT).as_posix()
            assert '.private' not in rel and p.name not in {'full_flash.bin','active_app_slot.bin','nvs.bin'},'Private artifact must not enter source'
            if rel=='public/sources/'+ALIAS or rel in ['release-inputs/Draftor_v0.1.0-alpha.2.1_Resources.zip','release-inputs/Draftor_v0.1.0-alpha.3_WAVESHARE_RLCD42_Source.zip']:continue
            b=p.read_bytes()
            if private_mac and p.suffix.lower() in {'.md','.json','.js','.mjs','.py','.html','.txt','.csv','.yml'}:
                text=b.decode('utf8',errors='replace').lower();assert private_mac not in text and private_mac.replace(':','') not in text,'Private identity in source'
            add(z,rel,b);inventory[rel]={'bytes':len(b),'sha256':sha(b)}
        add(z,'R6_SOURCE_IDENTITY.json',(json.dumps(inventory,indent=2)+'\n').encode())
    with zipfile.ZipFile(source)as z:assert z.testzip() is None
    # Preserve R4's public/source files and both frozen R4 dist directories.
    for out in ['dist-r6-locked','dist-first-install-test']:
        folder=ROOT/out;shutil.copyfile(source,folder/'sources'/ALIAS)
        paths=sorted(p for p in folder.rglob('*') if p.is_file() and p.name!='SITE_SHA256SUMS')
        (folder/'SITE_SHA256SUMS').write_text(''.join(sha(p.read_bytes())+'  '+p.relative_to(folder).as_posix()+'\n' for p in paths))
    locked=ROOT/'dist-r6-locked';config=json.loads((locked/'BUILD.json').read_text())
    assert config['hardwareUpdateWrites'] is False and config['hardwareFreshWrites'] is False and config['experimental'] is False
    with zipfile.ZipFile(site,'x',zipfile.ZIP_DEFLATED)as z:
        for p in sorted(locked.rglob('*')):
            if p.is_file():assert not p.is_symlink();add(z,p.relative_to(locked).as_posix(),p.read_bytes())
    with zipfile.ZipFile(site)as z:
        assert z.testzip() is None
        manifest=z.read('SITE_SHA256SUMS').decode().splitlines()
        for line in manifest:
            expected,name=line.split('  ',1);assert sha(z.read(name))==expected
        assert sha(z.read('sources/'+ALIAS))==sha(source.read_bytes())
        assert set(z.namelist())=={line.split('  ',1)[1] for line in manifest}|{'SITE_SHA256SUMS'}
    result={'status':'PASS','locked_site':{'file':SITE_NAME,'bytes':site.stat().st_size,'sha256':sha(site.read_bytes())},'source':{'file':SOURCE_NAME,'bytes':source.stat().st_size,'sha256':sha(source.read_bytes())},'default_writes_locked':True,'experimental_build_not_in_site_archive':True,'factory_binary_not_redistributed':True,'private_device_data_included':False,'private_identity_text_scan':bool(private_mac),'corresponding_source_self_hosted':True}
    (OUT/'PACKAGE_VERIFICATION.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
if __name__=='__main__':main()
