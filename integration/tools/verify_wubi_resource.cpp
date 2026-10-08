// Decode final bytes using the unchanged Core decoder, then compare every
// code/ref/weight with the already audited SchemeMappingFact transport.
#include "paged_scheme_index_internal.hpp"
#include "hanime/host_resource_file.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <set>
#include <tuple>
int main(int argc,char** argv) {
    if(argc!=3)return 2;
    hanime::HostResourceMetrics metrics;
    hanime::HostResourceFile file(argv[2],128u*1024u*1024u,metrics);
    hanime::detail::DecodedPagedScheme decoded;std::string error;
    if(!hanime::detail::decode_paged_scheme_bundle(file,&decoded,&error)){std::cerr<<error;return 1;}
    using Row=std::tuple<std::string,hanime::RuntimeLexemeRef,int32_t>;
    std::multiset<Row> expected,actual;
    std::ifstream input(argv[1]);std::string line;std::getline(input,line);
    while(std::getline(input,line)){
        std::istringstream row(line);std::string code,key,ref,weight;
        std::getline(row,code,'\t');std::getline(row,key,'\t');std::getline(row,ref,'\t');std::getline(row,weight,'\t');
        expected.emplace(code,hanime::RuntimeLexemeRef(std::stoul(ref)),std::stoi(weight));
    }
    for(const auto& row:decoded.mappings)actual.emplace(row.code,row.runtime_ref,row.scheme_weight);
    if(expected.size()!=81603 || actual!=expected || decoded.scheme_id!="wubi86" ||
       decoded.registry_namespace!=0x48414e494d450001ULL || decoded.required_shared_bundle_id!=11226326604333549466ULL)return 1;
    std::cout<<"PASS final frozen-decoder mapping equality: "<<actual.size()<<" code/ref/zero-weight facts; same accepted namespace and shared bundle\n";
    return 0;
}
