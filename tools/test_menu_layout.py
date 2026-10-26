#!/usr/bin/env python3
"""Test bounded product helper and compare unchanged reviewed Closeout names."""
from pathlib import Path
import argparse,json,subprocess
R=Path(__file__).resolve().parents[1]
ap=argparse.ArgumentParser();ap.add_argument('--closeout',type=Path,required=True);a=ap.parse_args()
out=R/'TestBuilds/menu-font';ev=R/'evidence/r2';ev.mkdir(exist_ok=True,parents=True)
command=['/usr/bin/clang++','-std=c++17','-Werror','-Wall','-Wextra','-g','-fsanitize=address,undefined','-I'+str(R/'integration/product/platform'),'-I'+str(R/'integration/product/dependencies/common'),'-I'+str(out),str(R/'integration/product/platform/menu_font.cpp'),str(R/'integration/product/tests/menu_name_layout.cpp'),str(R/'integration/product/dependencies/common/utf8.cpp'),'-o',str(out/'menu_name_test')]
subprocess.run(command,check=True);run=subprocess.run([str(out/'menu_name_test')],capture_output=True,text=True,check=True);assert not run.stderr
rows=[json.loads(line) for line in (a.closeout/'evidence/wrapping.jsonl').read_text().splitlines()];accepted=[r for r in rows if r.get('result')=='PASS'];assert accepted
compare=subprocess.run([str(out/'menu_name_test'),'--compare'],input=''.join(r['name']+'\n' for r in accepted),capture_output=True,text=True,check=True);assert not compare.stderr
for record,line in zip(accepted,compare.stdout.splitlines()):
    nums=list(map(int,line.split(',')));count=nums[0];offsets=nums[1:];raw=record['name'].encode();lines=[raw[offsets[i]:offsets[i+1]].decode() for i in range(count)]
    assert lines==record['new_lines'],(record['name'],lines,record['new_lines'])
assert len(compare.stdout.splitlines())==len(accepted)
report={'status':'PASS','bounded_product_cases':json.loads(run.stdout),'closeout_exact_linebreak_cases':len(accepted),'closeout_source':str(a.closeout),'purpose':'Retain approved layouts and byte identity; extend extreme punctuation/missingglyph behavior only as newly authorized','sanitizers':'ASan+UBSan','build_command':command,'device_tested':False}
(ev/'MENU_NAME_LAYOUT_TEST.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n');print(json.dumps(report,ensure_ascii=False,indent=2))
preview_command=command.copy();preview_command[preview_command.index(str(R/'integration/product/tests/menu_name_layout.cpp'))]=str(R/'integration/product/tests/menu_name_preview.cpp');preview_command[-1]=str(out/'menu_preview_test')
subprocess.run(preview_command,check=True);preview=subprocess.run([str(out/'menu_preview_test')],capture_output=True,text=True,check=True);assert not preview.stderr
(ev/'MENU_NAME_PREVIEW_TEST.json').write_text(json.dumps({'status':'PASS','sanitizers':'ASan+UBSan','build_command':preview_command,'stdout':preview.stdout,'device_tested':False,'scope':'Reviewed recovery suffix and real distinguishing Han scalar, bounded96byte/100byteUTF8output. No universal uniqueness guarantee for arbitrary many colliding names; full selected name remains authoritative.'},ensure_ascii=False,indent=2)+'\n')
print(preview.stdout)
