#include "base_order_build.hpp"
#include "base_order_wire.hpp"
#include "paged_scheme_index_internal.hpp"
#include <algorithm>
#include <map>
#include <stdexcept>

namespace hanime::detail {
namespace {
void require(bool ok,const char* why) {if (!ok) throw std::runtime_error(why);}
using Reference=std::map<std::string,std::vector<RankItem>>;
class HostChunkReader final:public ResourceReader {
 public:
  explicit HostChunkReader(const ResourceReader& r):source(r) {}
  std::size_t size() const override {return source.size();}
  bool read(std::size_t at,void* out,std::size_t n) const override {
    auto* p=static_cast<std::uint8_t*>(out);
    while (n) {const auto amount=std::min<std::size_t>(n,65536);if(!source.read(at,p,amount))return false;at+=amount;p+=amount;n-=amount;}
    return true;
  }
 private: const ResourceReader& source;
};
Reference reference(const ResourceReader& file,PagedSharedResource& shared) {
  DecodedPagedScheme source; std::string error;
  HostChunkReader chunks(file);
  if (!decode_paged_scheme_bundle(chunks,&source,&error)) throw std::runtime_error("base-order source decode failed: "+error);
  require(shared.ready() && source.registry_namespace==shared.info().registry_namespace &&
          source.required_shared_bundle_id==shared.info().shared_bundle_id,"base-order source dependency mismatch");
  std::map<std::string,std::map<RuntimeLexemeRef,RankItem>> grouped;
  for (const auto& m:source.mappings) {
    require(m.code.size()<=kBaseMaxCode,"base-order code exceeds 128-byte format capability");
    auto& x=grouped[m.code][m.runtime_ref];
    if (!x.path_count || m.scheme_weight>x.scheme_weight) {x.scheme_weight=m.scheme_weight;x.representative_ordinal=m.mapping_ordinal;}
    else if (m.scheme_weight==x.scheme_weight) x.representative_ordinal=std::min(x.representative_ordinal,m.mapping_ordinal);
    ++x.path_count; x.ref=m.runtime_ref;
  }
  // Host-only metadata memo avoids repeating reads for aliases; no runtime map.
  std::map<RuntimeLexemeRef,RuntimeLexemeInfo> metadata;
  Reference result;
  for (const auto& code:grouped) {
    auto& out=result[code.first];
    for (const auto& entry:code.second) {
      auto item=entry.second;
      auto found=metadata.find(item.ref);
      if (found==metadata.end()) {
        SharedReadCursor c;
        require(shared.begin_read(SharedRecordKind::Lexeme,item.ref,&c)==SharedReadStatus::InProgress,"base-order bad reference");
        for (;;) {
          auto step=shared.advance_read(&c,{64,16384,4096});
          if (step.status==SharedReadStatus::Ready) {found=metadata.emplace(item.ref,step.lexeme.identity).first;break;}
          require(step.status==SharedReadStatus::InProgress,"base-order shared read failed");
        }
      }
      item.key=found->second.stable_key;
      item.score=std::int64_t(found->second.base_weight)+item.scheme_weight;
      out.push_back(item);
    }
    std::sort(out.begin(),out.end(),[](const RankItem& a,const RankItem& b){return a.score!=b.score ? a.score>b.score : a.key<b.key;});
  }
  return result;
}
bool same(const RankItem& a,const RankItem& b) {
  return a.key==b.key && a.score==b.score && a.ref==b.ref && a.scheme_weight==b.scheme_weight &&
      a.path_count==b.path_count && a.representative_ordinal==b.representative_ordinal && !a.effective_count;
}
}
std::vector<std::uint8_t> build_base_order(const ResourceReader& source,PagedSharedResource& shared) {
  const auto ref=reference(source,shared);
  std::uint32_t max_code=1,groups=0;
  for (const auto& c:ref) {max_code=std::max(max_code,static_cast<std::uint32_t>(c.first.size()));groups+=static_cast<std::uint32_t>(c.second.size());}
  const auto width=max_code+16;
  const std::uint64_t rank_offset=kBaseHeader+std::uint64_t(width)*ref.size();
  const auto member_offset=rank_offset+std::uint64_t(kBaseItem)*groups;
  std::vector<std::uint8_t> bytes(member_offset+std::uint64_t(kBaseMember)*groups,0);
  auto* p=bytes.data(); put32(p,kBaseMagic);put32(p+4,1);put32(p+8,width);put32(p+12,static_cast<std::uint32_t>(ref.size()));
  put32(p+16,groups);put32(p+20,max_code);put32(p+24,kBaseItem);put64(p+32,kBaseHeader);
  put64(p+40,rank_offset);put64(p+48,member_offset);put64(p+56,bytes.size());
  std::uint32_t code=0,start=0;
  for (const auto& group:ref) {
    auto* d=p+kBaseHeader+std::size_t(code++)*width;put32(d,static_cast<std::uint32_t>(group.first.size()));
    put32(d+4,start);put32(d+8,static_cast<std::uint32_t>(group.second.size()));
    std::copy(group.first.begin(),group.first.end(),d+16);
    std::uint32_t paths=0;
    std::map<RuntimeLexemeRef,std::uint32_t> members;
    for (std::size_t i=0;i<group.second.size();++i) {
      const auto& x=group.second[i]; paths+=x.path_count;
      encode_item(p+rank_offset+std::size_t(start+i)*kBaseItem,x);
      members.emplace(x.ref,static_cast<std::uint32_t>(i));
    }
    put32(d+12,paths);std::size_t i=0;
    for (const auto& m:members) {auto* q=p+member_offset+(start+i++)*kBaseMember;put32(q,m.first);put32(q+4,m.second);}
    start+=static_cast<std::uint32_t>(group.second.size());
  }
  return bytes;
}
bool check_base_order(const ResourceReader& file,const ResourceReader& source,PagedSharedResource& shared,std::string* error) {
  try {
    std::array<std::uint8_t,144> b{};BaseHeader h;
    require(file.read(0,b.data(),kBaseHeader)&&base_header(b.data(),file.size(),h),"base-order header/layout failed");
    const auto ref=reference(source,shared); // Rebuilt from ORIGINAL mappings, not manifest.
    require(ref.size()==h.codes,"base-order code coverage mismatch");
    std::uint32_t code=0,start=0;
    for (const auto& group:ref) {
      require(group.first.size()<=h.max_code,"base-order code width mismatch");
      require(file.read(kBaseHeader+std::size_t(code++)*h.width,b.data(),h.width),"base-order directory short read");
      require(bo32(b.data())==group.first.size() && bo32(b.data()+4)==start && bo32(b.data()+8)==group.second.size(),"base-order directory coverage failed");
      require(std::string_view(reinterpret_cast<char*>(b.data()+16),group.first.size())==group.first,"base-order code order failed");
      require(std::all_of(b.begin()+16+group.first.size(),b.begin()+h.width,[](auto x){return x==0;}),"base-order padding failed");
      const auto paths=bo32(b.data()+12);std::uint32_t expected_paths=0;
      std::map<RuntimeLexemeRef,std::uint32_t> members;
      for (std::size_t i=0;i<group.second.size();++i) {
        require(file.read(h.rank_offset+(start+i)*kBaseItem,b.data(),kBaseItem),"base-order item short read");
        RankItem x;
        require(base_item(b.data(),shared.info().sections[0].count,x)&&same(x,group.second[i]),"base-order identity/group/score/order mismatch");
        expected_paths+=x.path_count;members.emplace(x.ref,static_cast<std::uint32_t>(i));
      }
      require(paths==expected_paths,"base-order raw path coverage mismatch");std::size_t i=0;
      for (const auto& m:members) {
        require(file.read(h.member_offset+(start+i++)*kBaseMember,b.data(),kBaseMember),"base-order member short read");
        require(bo32(b.data())==m.first && bo32(b.data()+4)==m.second,"base-order member permutation/coverage failed");
      }
      start+=static_cast<std::uint32_t>(group.second.size());
    }
    require(start==h.groups,"base-order uncovered trailing groups");
    if (error) error->clear();
    return true;
  } catch (const std::exception& e) {if(error)*error=e.what();return false;}
}
} // namespace hanime::detail
