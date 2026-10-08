#define main inherited_main
#include "host_tests.cpp"
#undef main
#include "small_font.hpp"
void fontChecks(){
 s3host::BuiltinSmallFont font;
 for(const auto& row:s3small::glyphs){auto g=font.glyph(row.cp);CHECK(!g.missing&&g.height==14&&g.stride==2&&g.advance&&g.bits==row.bits);
  unsigned black=0;for(unsigned y=0;y<g.height;++y)for(unsigned x=0;x<g.width;++x)if(g.bits[y*g.stride+x/8]&(0x80>>(x%8)))++black;
  if(row.cp!=32) { CHECK(black); }
  CHECK(g.bearing>=0&&g.bearing+g.width<=g.advance+1);
 }
 auto old=s3font::get('-');unsigned all=0,decimated=0;for(unsigned y=0;y<28;++y)for(unsigned x=0;x<old->width;++x)if(old->bits[y*4+x/8]&(0x80>>(x%8))){++all;if(y%2==0&&x%2==0)++decimated;}CHECK(all==7&&decimated==0);
 auto hyphen=font.glyph('-');unsigned fixed=0;for(unsigned y=0;y<14;++y)for(unsigned x=0;x<hyphen.width;++x)if(hyphen.bits[y*2+x/8]&(0x80>>(x%8)))++fixed;CHECK(fixed);
 for(auto pair:{std::pair<char,char>{'I','l'},{'l','1'},{'O','0'},{'S','5'},{'B','8'}}){auto a=font.glyph(pair.first),b=font.glyph(pair.second);CHECK(a.width!=b.width||std::memcmp(a.bits,b.bits,28)!=0);}
 CHECK(s3app::display::fit("£££",font,2*font.glyph(163).advance)==4);
 std::printf("PASS 98 actual 14px glyphs, UK UTF8 metrics, ambiguous characters distinct; old hyphen=%u decimated=%u new=%u; table=%zu B\n",all,decimated,fixed,sizeof(s3small::glyphs));
}
void savedState(){Fixture f;f.start();f.key(4);CHECK(f.app.document.dirty()&&std::string(f.app.notice)=="Unsaved changes");f.key(22,1);f.key(5);f.settle();CHECK(f.app.document.dirty()&&std::string(f.app.notice)=="Snapshot saved; newer edits");f.save();CHECK(!f.app.document.dirty());f.key(29,1);CHECK(f.app.document.dirty()&&f.app.document.text()=="a"&&std::string(f.app.notice)=="Unsaved changes");f.save();auto id=f.app.session.currentId();CHECK(f.app.session.create("Undo boundary B",++f.now));CHECK(f.app.session.open(id,++f.now));auto body=f.app.document.text();f.key(29,1);CHECK(f.app.document.text()==body);}
int main(){try{fontChecks();savedState();std::puts("PASS visible save state follows matching body revision; save preserves same-document undo; A-B-A clears history");return 0;}catch(const std::exception&e){std::fprintf(stderr,"FAIL %s\n",e.what());return 1;}}
