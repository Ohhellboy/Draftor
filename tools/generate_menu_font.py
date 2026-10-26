#!/usr/bin/env python3
"""Generate local TEST_ONLY menu data, inheriting R1.6 SFNT/FreeType pipeline.

No resource is installed or published. Outputs stay in ignored TestBuilds;
MiSans bitmap distribution permission remains a separate release decision.
"""
from pathlib import Path
from PIL import ImageFont, features
import argparse,hashlib,json,math,re,struct
ROOT=Path(__file__).resolve().parents[1]
SOURCE_SHA='0ddef90648998900175cfdca9a6f087a2544c182f130b0ad4f7e94a03a115e79'
class FaceInfo:
    def __init__(self, path):
        self.raw = path.read_bytes()
        r = self.raw
        assert r[:4] in (b'\x00\x01\x00\x00', b'OTTO'), 'Use one standalone TTF/OTF face'
        self.tables = {}
        for i in range(struct.unpack_from('>H',r,4)[0]):
            tag, checksum, at, size = struct.unpack_from('>4sIII', r, 12+i*16)
            assert at+size <= len(r)
            self.tables[tag.decode()] = r[at:at+size]
        names = self.tables['name']; self.names = {}
        _, count, base = struct.unpack_from('>HHH',names)
        for i in range(count):
            platform, enc, lang, key, size, at = struct.unpack_from('>6H',names,6+i*12)
            if platform not in (0,1,3): continue
            value = names[base+at:base+at+size].decode('utf-16-be' if platform in (0,3) else 'mac_roman',errors='replace')
            self.names.setdefault(str(key),[])
            if value not in self.names[str(key)]: self.names[str(key)].append(value)
        self.weight = struct.unpack_from('>H',self.tables['OS/2'],4)[0]
        self.upem = struct.unpack_from('>H',self.tables['head'],18)[0]
        self.hhea = struct.unpack_from('>hhh',self.tables['hhea'],4)
        self.axes = []
        if 'fvar' in self.tables:
            v=self.tables['fvar']; offset,_,count,size=struct.unpack_from('>HHHH',v,4)
            for i in range(count):
                tag, lo, default, hi = struct.unpack_from('>4siii',v,offset+i*size)
                self.axes.append({'tag':tag.decode(),'min':lo/65536,'default':default/65536,'max':hi/65536})
        cmap=self.tables['cmap']; maps={}
        for i in range(struct.unpack_from('>H',cmap,2)[0]):
            platform, enc, offset=struct.unpack_from('>HHI',cmap,4+i*8)
            if not (platform==0 or platform==3 and enc in (1,10)): continue
            fmt=struct.unpack_from('>H',cmap,offset)[0]; sub=cmap[offset:]
            if fmt==12:
                for j in range(struct.unpack_from('>I',sub,12)[0]):
                    first,last,gid=struct.unpack_from('>III',sub,16+12*j)
                    for cp in range(first,last+1):
                        if gid+cp-first: maps[cp]=gid+cp-first
            elif fmt==4:
                n=struct.unpack_from('>H',sub,6)[0]//2
                end=14; start=end+2*n+2; delta=start+2*n; ro=delta+2*n
                for j in range(n):
                    a=struct.unpack_from('>H',sub,start+j*2)[0]; b=struct.unpack_from('>H',sub,end+j*2)[0]
                    d=struct.unpack_from('>h',sub,delta+j*2)[0]; off=struct.unpack_from('>H',sub,ro+j*2)[0]
                    for cp in range(a,b+1):
                        if cp==0xffff: continue
                        g=struct.unpack_from('>H',sub,ro+j*2+off+2*(cp-a))[0] if off else cp
                        if g: g=(g+d)&65535
                        if g: maps.setdefault(cp,g)
        self.cmap=maps
        assert any('MiSans' in s for s in self.names.get('1',[])+self.names.get('16',[])), 'Not a MiSans family'
        if self.axes:
            axis=next((a for a in self.axes if a['tag']=='wght'),None)
            assert axis and axis['min']<=500<=axis['max'], 'No real weight-500 instance'
        else:
            assert self.weight==500, 'Static face must be actual OS/2 weight 500'
            assert any('Medium' in s for s in self.names.get('2',[])+self.names.get('17',[])+self.names.get('4',[])), 'Static face must be Medium'
    def load(self,path,px):
        f=ImageFont.truetype(str(path),px,layout_engine=ImageFont.Layout.BASIC)
        if self.axes: f.set_variation_by_axes([500 if a['tag']=='wght' else a['default'] for a in self.axes])
        return f


def emit_bytes(name,data):
    return 'inline constexpr uint8_t '+name+'[]={\n'+''.join(','.join(str(v) for v in data[i:i+32])+',\n' for i in range(0,len(data),32))+'};\n'

def generate(path,corpus_paths):
    info=FaceInfo(path)
    assert hashlib.sha256(info.raw).hexdigest()==SOURCE_SHA,'Not the approved local MiSans VF 4.009 source'
    assert info.axes and next(a for a in info.axes if a['tag']=='wght')
    # Deliberately bounded label-only 14px. Dynamic user content uses full16.
    corpus=''.join(p.read_text() for p in corpus_paths)
    required=set(range(32,127))|{ord(c) for c in corpus if ord(c)>=128}
    missing14=sorted(required-set(info.cmap));assert not missing14,('Label source has absent MiSans scalars',missing14)
    out=ROOT/'TestBuilds/menu-font';out.mkdir(parents=True,exist_ok=True)
    evidence=ROOT/'evidence/r2';evidence.mkdir(parents=True,exist_ok=True)
    header='// LOCAL TEST_ONLY MiSans VF 4.009 wght=500; not an Apache-2.0 asset.\n#pragma once\n#include <cstdint>\n#include <cstddef>\nnamespace s3app::menu_data {\nstruct Page {uint16_t page,first;uint8_t mask[32];};\nstatic_assert(sizeof(Page)==36,"Packed page index");\n'
    report={'source_sha256':SOURCE_SHA,'font_names':info.names,'effective_weight':500,'freetype':features.version('freetype2'),'raster':'native hinted ppem, grayscale >=128; per-scalar BASIC metrics; exact R1.6 shared baselines','distribution':'LOCAL TEST_ONLY; generated assets excluded from source patch and release','corpus':[str(p.relative_to(ROOT)) if ROOT in p.parents else str(p) for p in corpus_paths],'sizes':[],'scratch_bytes_per_FontPort':112,'dynamic_allocations':0,'extra_framebuffers':0,'fallback':'Visible hollow square per unavailable scalar, missing=true; source name/ID/transactions untouched. Combining marks retain inherited per-scalar model, not grapheme shaping.'}
    for px,pitch,baseline in [(14,24,18),(16,28,20)]:
        cps=sorted(required if px==14 else (cp for cp in info.cmap if cp>=32))
        font=info.load(path,px);pages={};records=bytearray();bits=bytearray();maxw=maxh=0;glyph_info=[]
        for index,cp in enumerate(cps):
            mask,offset=font.getmask2(chr(cp),mode='L',anchor='ls');w,h=mask.size;left,top=offset
            ink=[(i%w,i//w) for i,v in enumerate(bytes(mask)) if v>=128] if w else []
            advance=max(1,math.floor(font.getlength(chr(cp))+0.5))
            gw=gh=gy=0;bearing=0;glyph=bytearray()
            if ink:
                x0=min(x for x,y in ink);y0=min(y for x,y in ink);gw=max(x for x,y in ink)-x0+1;gh=max(y for x,y in ink)-y0+1
                gy=baseline+top+y0;bearing=left+x0;glyph=bytearray((gw*gh+7)//8)
                for x,y in ink:
                    at=(y-y0)*gw+x-x0;glyph[at//8]|=128>>(at%8)
            assert 0<=gw<32 and 0<=gh<32 and 0<=gy<32 and gy+gh<=pitch,(px,hex(cp),gw,gh,gy)
            assert -32<=bearing<32 and 0<advance<32,(px,hex(cp),advance,bearing)
            assert len(bits)<1<<24
            packed=gw|(gh<<5)|(gy<<10)|(advance<<15)|((bearing+32)<<20)
            records+=len(bits).to_bytes(3,'little')+packed.to_bytes(4,'little');bits+=glyph
            page=cp>>8
            if page not in pages:pages[page]=[index,bytearray(32)]
            pages[page][1][(cp&255)//8]|=1<<(cp&7)
            maxw=max(maxw,gw);maxh=max(maxh,gh)
            glyph_info.append({'cp':cp,'advance':advance,'width':gw,'height':gh,'top':gy,'bearing':bearing})
        header+='inline constexpr Page pages'+str(px)+'[]={\n'
        header+=''.join('{'+str(page)+','+str(first)+',{'+','.join(map(str,mask))+'}},\n' for page,(first,mask) in pages.items())+'};\n'
        header+=f'inline constexpr size_t PageCount{px}={len(pages)},GlyphCount{px}={len(cps)};\n'
        header+=emit_bytes('records'+str(px),records)+emit_bytes('bits'+str(px),bits)
        info_row={'px':px,'pitch':pitch,'baseline':baseline,'glyph_count':len(cps),'pages':len(pages),'index_bytes':len(pages)*36,'record_bytes':len(records),'ink_bytes':len(bits),'flash_table_bytes':len(pages)*36+len(records)+len(bits),'max_width':maxw,'max_ink_height':maxh,'codepoints_sha256':hashlib.sha256(json.dumps(cps).encode()).hexdigest()}
        report['sizes'].append(info_row)
        (out/f'metrics-{px}.json').write_text(json.dumps(glyph_info,separators=(',',':'))+'\n')
        print(json.dumps(info_row),flush=True)
    header+='}\n';target=out/'menu_font_data.hpp';target.write_text(header)
    report['generated_header_sha256']=hashlib.sha256(target.read_bytes()).hexdigest()
    report['flash_table_bytes']=sum(r['flash_table_bytes'] for r in report['sizes'])
    report['source_cmap_count']=len(info.cmap)
    report['missing_probes']={f'U+{cp:04X}':cp not in info.cmap for cp in [0x28fcd,0x1f600,0x200d]}
    (evidence/'MENU_FONT.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n')
    print('Generated local TEST_ONLY tables:',target,flush=True)

if __name__=='__main__':
    ap=argparse.ArgumentParser();ap.add_argument('--font',required=True,type=Path);ap.add_argument('--corpus',action='append',type=Path,default=[]);args=ap.parse_args()
    paths=sorted(set(args.corpus+[p for p in (ROOT/'integration/product/platform').iterdir() if p.suffix in ('.hpp','.cpp')]))
    generate(args.font,paths)
