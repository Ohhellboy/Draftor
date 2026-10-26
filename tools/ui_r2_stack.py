#!/usr/bin/env python3
from pathlib import Path
import re,json
r=Path(__file__).resolve().parents[1];b=r/'TestBuilds/s3/DISPLAY_TX_PROTO/build/sketch';rows=[]
for f in b.glob('*.su'):
 for line in f.read_text().splitlines():
  p=line.split('\t')
  if len(p)==3:rows.append(dict(symbol=p[0],bytes=int(p[1]),kind=p[2],file=f.name))
patterns={'loop':r':void loop\(\)$','deviceLoop':r':void deviceLoop\(\)$','draw':r':void \{anonymous\}::draw\(\)$','presentContextWrapper':r':void \{anonymous\}::presentContext\(\)$','presentContextPart':r':void \{anonymous\}::_ZN12_GLOBAL__N_114presentContextEv\$part\$0\(\)$','drawDocumentMenu':r':bool \{anonymous\}::drawDocumentMenu\(','documentMenu':r':bool s3app::display::documentMenu\(.*Fill = void','wrapMenuName':r':s3app::display::MenuNameLines s3app::display::wrapMenuName\(','menuNamePreview':r':s3app::display::MenuNamePreview s3app::display::menuNamePreview\(','previewBase':r':s3app::display::MenuNamePreview s3app::display::menu_preview_detail::base\(','previewMiddle':r':s3app::display::MenuNamePreview s3app::display::menu_preview_detail::middle\(','previewAppend':r':bool s3app::display::MenuNamePreview::append\(','previewDistinguish':r':s3app::display::MenuNamePreview s3app::display::menu_preview_detail::distinguish\(','glyph':r':virtual s3host::GlyphView s3app::MenuFont::glyph\(','fontEntry':r':const uint8_t\* s3app::\{anonymous\}::entry\(','nameLines':r':void s3app::display::nameLines\(','menuText':r':void \{anonymous\}::menuText\(','menuContent':r':void \{anonymous\}::menuContent\(','pixel':r':void ST7306OsptekLpm8::pixel\('}
frames={}
for key,pattern in patterns.items():
 found=[x for x in rows if re.search(pattern,x['symbol'])];assert found,key;assert all(x['kind']=='static' for x in found),key;frames[key]=max(found,key=lambda x:x['bytes'])
s=lambda *keys:sum(frames[x]['bytes'] for x in keys)
prefix=64+s('loop','deviceLoop','presentContextWrapper','presentContextPart','draw','drawDocumentMenu','documentMenu')
# 64 B is the inherited linked SDK/Arduino wrapper reserve. 288 B deliberately
# covers the mutually exclusive UTF8/font/bit lookup/copy leaf branches; adds
# the largest emitted helper chain, even when the compiler can tail-call it.
leaf=288
paths={'preview_with_context':prefix+s('menuNamePreview','previewBase','previewMiddle','previewAppend')+leaf,'wrap_with_context':prefix+s('wrapMenuName')+leaf,'raster_with_context':prefix+s('nameLines','menuContent','menuText')+leaf}
result=dict(status='PASS_BOUNDED_STATIC_MENU_PATH' if max(paths.values())<=5632 else 'EXCEEDS_EXISTING_REFERENCE',frames=frames,prefix_bytes=prefix,leaf_reserve_bytes=leaf,paths=paths,reference_bytes=5632,notes='Conservative reviewed menu call paths using actual Xtensa .su; shared 5632 B critical-path reference unchanged. Not whole-program worst-case proof or device stack high-water. Font and pixel branches are sequential, not recursive. Both presentContext wrapper and split part included.')
(r/'evidence/ui_r2/MENU_STACK.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(dict(status=result['status'],paths=paths),indent=2));assert result['status']=='PASS_BOUNDED_STATIC_MENU_PATH'
