// Host adapter to the EXISTING frozen ResourceBuilder scheme encoder. It never
// invokes the shared builder or allocates/reorders RuntimeLexemeRefs.
#include "paged_scheme_index_internal.hpp"
#include "hanime/host_resource_file.hpp"
#include "hanime/resource_publisher.hpp"
#include "release_pin.hpp"
#include "wubi_auto_safe.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <set>
namespace fs = std::filesystem;
using namespace hanime;
void require(bool ok, const std::string& message) { if (!ok) throw std::runtime_error(message); }
int main(int argc, char** argv) {
    try {
        require(argc==5, "usage: build_wubi_resource facts.tsv shared.hns full_pinyin.hnm NEW_OUTPUT");
        const fs::path out(argv[4]);
        require(!fs::exists(out), "output already exists");
        std::ifstream input(argv[1]);
        std::string line;
        require(bool(std::getline(input,line)) && line ==
            "#wubi86\t152a0d3f3efe40cae216d1e3b338242446848d07\twubi_binding_policy_v1\twubi_weight_policy_v1_zero\t58d8f794577ebedacb4b7ce71a8b885aca75a8eb",
            "missing/wrong source or policy identity");
        HostResourceMetrics metrics;
        auto shared = std::make_shared<HostResourceFile>(argv[2],128u*1024u*1024u,metrics);
        auto pinyin = std::make_shared<HostResourceFile>(argv[3],128u*1024u*1024u,metrics);
        MountedResourceSet mounted;
        std::string error;
        require(mounted.mount(shared,{pinyin},&error), "accepted resource mount: "+error);
        require(mounted.registry_namespace()==hanime_release::shared.registry_namespace &&
                mounted.shared_bundle_id()==hanime_release::shared.shared_bundle_id, "accepted identity mismatch");
        std::vector<detail::PagedSchemeBuildMapping> mappings;
        std::set<std::pair<std::string,StableLexemeKey>> seen;
        while (std::getline(input,line)) {
            std::istringstream row(line); std::string code,key_text,ref_text,weight,kind,extra;
            require(bool(std::getline(row,code,'\t')) && bool(std::getline(row,key_text,'\t')) &&
                    bool(std::getline(row,ref_text,'\t')) && bool(std::getline(row,weight,'\t')) && bool(std::getline(row,kind,'\t')) &&
                    !std::getline(row,extra,'\t'), "bad fact transport");
            require(!code.empty() && code.size()<=4 && code.find_first_not_of("abcdefghijklmnopqrstuvwxy")==std::string::npos && weight=="0", "invalid code/weight");
            StableLexemeKey key;
            require(parse_slk1(key_text,&key) && !key.empty(), "invalid existing key");
            const auto wide_ref = std::stoull(ref_text);
            require(wide_ref > 0 && wide_ref <= hanime_release::shared.runtime_ref_count, "invalid ref");
            const auto ref = RuntimeLexemeRef(wide_ref);
            RuntimeLexemeInfo identity;
            require((kind=="word" || kind=="character") && mounted.lexeme_info(ref,&identity) && identity.stable_key==key &&
                    identity.kind==(kind=="word" ? LexemeKind::Word : LexemeKind::Character),
                    "key/ref/canonical kind does not match accepted shared");
            require(seen.insert({code,key}).second, "duplicate mapping");
            mappings.push_back({code,ref,0,uint32_t(mappings.size())});
        }
        require(!mappings.empty(), "no admitted facts");
        std::vector<uint8_t> bytes; detail::PagedSchemeBuildStats stats;
        require(detail::build_paged_scheme_bundle("wubi86",mounted.registry_namespace(),mounted.shared_bundle_id(),
                    mappings,4096,&bytes,&stats,&error), "frozen scheme build: "+error);
        fs::create_directories(out);
        { std::ofstream file(out/"wubi86.hnm",std::ios::binary);
          file.write(reinterpret_cast<const char*>(bytes.data()),std::streamsize(bytes.size())); require(file.good(),"write scheme"); }
        PublishVerifiedOptions options;
        options.shared_path=argv[2]; options.schemes={{"wubi86",(out/"wubi86.hnm").string()}};
        options.output_directory=(out/"published").string();
        options.registry_namespace=hanime_release::shared.registry_namespace;
        options.expected_shared_bundle_id=hanime_release::shared.shared_bundle_id;
        options.base_order=true; options.base_order_format=2; options.packed_auth=true;
        const auto published=publish_verified_resources(options);
        require(published.ok,"frozen verified publication: "+published.error);
        require(published.resources.size()==3, "unexpected publication count");
        require(ime_device::sameRelease(published.resources.front().spec,hanime_release::shared),
                "publisher changed accepted shared pin");
        std::cout << "PASS source/policy identity; accepted shared key/ref validation mappings=" << mappings.size()
                  << " scheme_bytes=" << bytes.size() << " page_bytes=" << stats.page_bytes
                  << " pages=" << stats.page_count << " tree_depth=" << stats.tree_depth
                  << "; shared pin ALL fields unchanged; HBO2+verified publication\n";
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
