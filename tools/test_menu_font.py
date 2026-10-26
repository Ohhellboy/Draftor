#!/usr/bin/env python3
"""ASan/UBSan full atlas roundtrip vs independent FreeType raster and R1.6 hashes."""
from pathlib import Path
import argparse,hashlib,json,math,os,subprocess,sys
from generate_menu_font import FaceInfo,SOURCE_SHA
R=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--font',required=True,type=Path);a=ap.parse_args()
assert hashlib.sha256(a.font.read_bytes()).hexdigest()==SOURCE_SHA
out=R/'TestBuilds/menu-font';ev=R/'evidence/r2';ev.mkdir(exist_ok=True,parents=True)
command=['/usr/bin/clang++','-std=c++17','-Werror','-Wall','-Wextra','-g','-fsanitize=address,undefined','-I'+str(R/'integration/product/platform'),'-I'+str(out),str(R/'integration/product/platform/menu_font.cpp'),str(R/'integration/product/tests/menu_font.cpp'),'-o',str(out/'menu_font_test')]
subprocess.run(command,check=True);result=subprocess.run([str(out/'menu_font_test')],capture_output=True,text=True,check=True);assert not result.stderr
(out/'lookup.csv').write_text(result.stdout)
face=FaceInfo(a.font);fonts={px:face.load(a.font,px) for px in [14,16]};counts={14:0,16:0}
for line in result.stdout.splitlines():
    px,cp,gw,pitch,stride,advance,bearing,actual=map(int,line.split(','));font=fonts[px];baseline=18 if px==14 else 20
    mask,offset=font.getmask2(chr(cp),mode='L',anchor='ls');w,h=mask.size;left,top=offset
    ink=[(i%w,i//w) for i,v in enumerate(bytes(mask)) if v>=128] if w else []
    exp_advance=max(1,math.floor(font.getlength(chr(cp))+0.5));assert advance==exp_advance,(px,cp,'advance')
    expected=bytearray(stride*pitch)
    if ink:
        x0=min(x for x,y in ink);x1=max(x for x,y in ink)+1
        assert bearing==left+x0 and gw==x1-x0,(px,cp,'metrics')
        for x,y in ink:expected[(baseline+top+y)*stride+(x-x0)//8]|=128>>((x-x0)%8)
    else:assert gw==0 and bearing==0
    digest=2166136261
    for v in expected:digest=((digest^v)*16777619)&0xffffffff
    assert actual==digest,(px,cp,'pixels',actual,digest)
    counts[px]+=1
report={'status':'PASS','test':'Every staged glyph through actual FontPort to independently rasterized source; metadata and full linebox FNV pixel hashes; ASan/UBSan plus missing/invalid scalars','counts':counts,'font_sha256':SOURCE_SHA,'build_command':command,'stderr':result.stderr,'device_tested':False}
(ev/'MENU_FONT_TEST.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
