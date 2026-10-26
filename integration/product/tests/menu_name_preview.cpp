#include "menu_name_preview.hpp"
#include "menu_font.hpp"
#include <cassert>
#include <cstdio>
#include <string>
#include <vector>
struct Item {std::string name;};
int main(){
 s3app::MenuFont16 font;
 std::vector<Item> items={{"城市公共空间与居民日常生活研究甲组访谈记录修订版"},{"城市公共空间与居民日常生活研究乙组访谈记录修订版"},{"文章（恢复）（1）"},{"关于城市公共空间与日常写作的田野调查记录（恢复）（1）"},{std::string(96,'W')}};
 auto a=s3app::display::menuNamePreview(items,0,font),b=s3app::display::menuNamePreview(items,1,font);
 assert(a.disambiguated&&b.disambiguated&&a.text()!=b.text());assert(a.text().find("甲")!=a.text().npos&&b.text().find("乙")!=b.text().npos);
 assert(s3app::display::menuNamePreview(items,2,font).text()==items[2].name);
 assert(s3app::display::menuNamePreview(items,3,font).text().find("（恢复）（1）")!=std::string_view::npos);
 for(size_t i=0;i<items.size();++i){auto out=s3app::display::menuNamePreview(items,i,font);assert(out.size&&out.size<100);assert(editor::isValidUtf8(out.text()));assert(s3app::display::measure(out.text(),font)<=264);std::printf("%zu %s\n",i,out.bytes);}
 std::puts("PASS: real distinguishing characters, true recovery suffix, bounded UTF-8 previews");
}
