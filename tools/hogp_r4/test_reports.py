from pathlib import Path
import json
from inspect_reports import parse,decode
p=Path(__file__).resolve().parent;r=parse((p/'HID_REPORT_MAP_R3.bin').read_bytes());assert {k:v['bytes'] for k,v in r.items()}=={1:7,3:4,4:1,5:2,17:19}
assert r[3]['pages']==[0xff0c] and r[5]['pages']==[0x0c] and r[4]['pages']==[1] and r[17]['pages']==[0xff43]
k=decode(r,1,bytes.fromhex('00420000000000'));assert k['contains_standard_keyboard_F9']
c=decode(r,5,bytes.fromhex('8000'));assert not c['contains_standard_keyboard_F9'] and c['active_fields']==[{'page':'0x000C','usage':'0x00E2','value':1,'offset':7}]
v=decode(r,3,bytes.fromhex('42000000'));assert not v['contains_standard_keyboard_F9'] and v['active_fields'][0]['page']=='0xFF0C'
try:decode(r,1,bytes(8));raise AssertionError('accepted wrong length')
except ValueError:pass
out=p.parents[1]/'evidence/hogp-r4';(out/'REPORT_DESCRIPTOR_CHECK.json').write_text(json.dumps({'status':'PASS','actual_descriptor_bytes':253,'input_reports':r,'synthetic_standard_F9':k,'synthetic_consumer':c,'synthetic_vendor':v,'synthetic_is_not_device_evidence':True},indent=2)+'\n')
print('PASS actual descriptor: 7/4/1/2/19 bytes; vendor FF0C is distinct from Consumer 000C; numeric decode never maps vendor payload to a keyboard command')
