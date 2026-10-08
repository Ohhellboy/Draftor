#include "hanime/core.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
std::string decode(const std::string& hex) {
  if (hex == "-") return {};
  if (hex.size() % 2) throw std::runtime_error("odd UTF-8 hex token");
  std::string out;
  auto nibble=[](char c)->unsigned {
    if(c>='0' && c<='9') return unsigned(c-'0');
    if(c>='a' && c<='f') return unsigned(c-'a'+10);
    throw std::runtime_error("invalid UTF-8 hex token");
  };
  for(std::size_t i=0;i<hex.size();i+=2) out.push_back(static_cast<char>((nibble(hex[i])<<4)|nibble(hex[i+1])));
  return out;
}
void write(const std::filesystem::path& p,const void* bytes,std::size_t size) {
  std::ofstream f(p,std::ios::binary|std::ios::trunc);
  if(!f.write(static_cast<const char*>(bytes),static_cast<std::streamsize>(size)))
    throw std::runtime_error("output write failed: "+p.string());
}
}
int main(int argc,char** argv) {
  try {
    if(argc!=3) throw std::runtime_error("usage: hanime_build_trial NORMALIZED_INPUT OUTPUT_DIR");
    std::ifstream in(argv[1]);
    hanime::ResourceBuildInput input;
    std::string line,magic; unsigned long long ns=0;
    if(!std::getline(in,line)) throw std::runtime_error("missing header");
    {std::istringstream h(line); std::string extra;
      if(!(h>>magic>>ns) || h>>extra ||
         !((magic=="HANIME_STEP2F_TRIAL_V1" && ns==0x54465249414c0001ULL) ||
           (magic=="HANIME_REGISTERED_V1" && ns==0x48414e494d450001ULL)))
        throw std::runtime_error("unknown build contract or namespace");}
    const bool registered=magic=="HANIME_REGISTERED_V1";
    input.registry_namespace=ns;
    input.source_name="v1.1 source evidence + explicit Step2F trial snapshot";
    input.source_version="Step2F.v0.1";
    input.relation_basis="Candidate adoption, NOT production approval; full trace in manifest.json";
    if (registered) {
      input.source_name="Production Resource Handoff v1.0 / central registry R1";
      input.source_version="Closeout.v1.0";
      input.relation_basis="Read-only registry; original default; explicit generic forms; unresolved conditions retained";
    }
    std::map<hanime::StableLexemeKey,std::size_t> positions;
    std::size_t number=1;
    while(std::getline(in,line)) {
      ++number; std::istringstream r(line); std::string tag,key_text,a,b,c,d,e,extra;
      hanime::StableLexemeKey key;
      if(!(r>>tag>>key_text) || !hanime::parse_slk1(key_text,&key)) throw std::runtime_error("line "+std::to_string(number)+": invalid key");
      if(tag=="L") {
        unsigned kind=0;
        if(!(r>>kind>>a>>b>>c) || (kind!=1 && kind!=2) || positions.count(key)) throw std::runtime_error("invalid/duplicate trial lexeme");
        hanime::LexemeSourceRecord l;
        l.stable_key=key; l.kind=static_cast<hanime::LexemeKind>(kind);
        l.status=hanime::RegistryStatus::Approved; // Admission checked by the corresponding build-side converter.
        if (registered && !(r>>l.base_weight)) throw std::runtime_error("missing base weight");
        l.default_output_utf8=decode(a); l.identity_definition=decode(b); l.provenance=decode(c);
        positions.emplace(key,input.lexemes.size()); input.lexemes.push_back(std::move(l));
      } else if(tag=="O") {
        unsigned availability=0,blocked=0;
        if(!(r>>a>>b>>c>>availability>>blocked>>d) || !positions.count(key) || availability<1 || availability>7 || blocked>1)
          throw std::runtime_error("invalid output record");
        hanime::OutputRelationInput o;
        o.form_id=decode(a); o.text_utf8=decode(b); o.condition=decode(c); o.selection_basis=decode(d);
        o.condition_kind=o.condition.empty()?hanime::ConditionKind::None:hanime::ConditionKind::WordOrSense;
        o.availability=static_cast<std::uint8_t>(availability);
        o.admission=blocked?hanime::OutputAdmission::Blocked:hanime::OutputAdmission::Ready;
        input.lexemes[positions.at(key)].output_relations.push_back(std::move(o));
      } else if(tag=="C") {
        if(!(r>>a>>b) || !positions.count(key) || (a!="full_pinyin" && a!="xiaohe")) throw std::runtime_error("invalid mapping");
        std::int32_t weight=0;
        if (registered && !(r>>weight)) throw std::runtime_error("missing scheme weight");
        input.mappings.push_back({a,b,key,weight});
      } else throw std::runtime_error("unknown record tag");
      if(r>>extra) throw std::runtime_error("line "+std::to_string(number)+": trailing fields");
    }
    if(!in.eof()) throw std::runtime_error("input read failed");
    auto built=hanime::ResourceBuilder().build(input,registered ? hanime::ResourceBuildOptions{4096,4096} : hanime::ResourceBuildOptions{96,96});
    if(!built.ok) throw std::runtime_error(built.error);
    const std::filesystem::path out(argv[2]); std::filesystem::create_directories(out);
    write(out/"shared.hns",built.resources.shared_bundle.data(),built.resources.shared_bundle.size());
    for(const auto& scheme:built.resources.scheme_bundles)
      write(out/(scheme.first+".hnm"),scheme.second.data(),scheme.second.size());
    const auto manifest=built.resources.manifest.to_json();
    write(out/"builder_manifest.json",manifest.data(),manifest.size());
    std::cout<<(registered ? "BUILD REGISTERED PASS keys=" : "BUILD TRIAL PASS keys=")<<input.lexemes.size()<<" shared_bytes="<<built.resources.shared_bundle.size()<<'\n';
  } catch(const std::exception& e) {std::cerr<<"REJECT "<<e.what()<<'\n';return 1;}
}
