#pragma once
#include "hanime/verified_resource.hpp"
// TEST_ONLY. Generated only after full-record equivalence and original transport checks.
// These compiled pins are trusted; on-resource JSON is never a trust source.
namespace hanime_release {
inline const hanime::ResourceReleaseSpec shared = [] {
  hanime::ResourceReleaseSpec s;
  s.rule_version=1; s.resource_format=4; s.key_format=1;
  s.role=hanime::ResourceReleaseRole::Shared;
  s.registry_namespace=5206528720952950785ULL; s.shared_bundle_id=17304919664082782091ULL;
  s.file_bytes=32384515ULL; s.file_sha256={{246,162,175,236,71,89,255,135,158,96,82,246,197,130,96,111,51,231,130,112,165,61,130,209,227,222,184,83,81,161,115,57}};
  s.merkle_root={{175,243,101,217,209,171,253,83,157,239,30,48,196,52,67,23,160,226,119,209,92,223,86,251,180,245,191,113,180,251,69,15}};
  s.required_shared_sha256={{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}};
  s.runtime_ref_count=242979; s.text_capacity=64; s.name_capacity=128;
  s.verification_format=2;
  s.bootstrap_root_offset=0ULL; s.bootstrap_root_bytes=0;
  return s;
}();
inline const hanime::ResourceReleaseSpec xiaohe = [] {
  hanime::ResourceReleaseSpec s;
  s.rule_version=1; s.resource_format=2; s.key_format=1;
  s.role=hanime::ResourceReleaseRole::Scheme;
  s.registry_namespace=5206528720952950785ULL; s.shared_bundle_id=17304919664082782091ULL;
  s.file_bytes=2756648ULL; s.file_sha256={{154,5,133,241,102,46,99,55,160,147,108,106,84,121,177,163,255,158,118,132,9,39,222,240,90,206,20,7,13,165,99,40}};
  s.merkle_root={{249,58,211,78,117,236,44,40,198,183,176,89,98,241,182,98,185,105,206,176,69,236,139,64,186,60,236,89,73,95,37,105}};
  s.required_shared_sha256={{246,162,175,236,71,89,255,135,158,96,82,246,197,130,96,111,51,231,130,112,165,61,130,209,227,222,184,83,81,161,115,57}};
  s.runtime_ref_count=242979; s.text_capacity=64; s.name_capacity=128;
  s.scheme_id[0]=120;
  s.scheme_id[1]=105;
  s.scheme_id[2]=97;
  s.scheme_id[3]=111;
  s.scheme_id[4]=104;
  s.scheme_id[5]=101;
  s.bootstrap_root_offset=2752552ULL; s.bootstrap_root_bytes=4096;
  return s;
}();
inline const hanime::ResourceReleaseSpec full_pinyin = [] {
  hanime::ResourceReleaseSpec s;
  s.rule_version=1; s.resource_format=2; s.key_format=1;
  s.role=hanime::ResourceReleaseRole::Scheme;
  s.registry_namespace=5206528720952950785ULL; s.shared_bundle_id=17304919664082782091ULL;
  s.file_bytes=2080808ULL; s.file_sha256={{217,27,122,233,130,216,12,252,89,136,88,82,179,245,171,223,176,176,29,25,64,248,184,17,171,34,182,85,170,169,114,59}};
  s.merkle_root={{220,130,153,19,131,111,190,130,132,117,124,227,66,181,150,64,110,79,228,233,186,91,0,205,94,37,244,173,55,239,181,235}};
  s.required_shared_sha256={{246,162,175,236,71,89,255,135,158,96,82,246,197,130,96,111,51,231,130,112,165,61,130,209,227,222,184,83,81,161,115,57}};
  s.runtime_ref_count=242979; s.text_capacity=64; s.name_capacity=128;
  s.scheme_id[0]=102;
  s.scheme_id[1]=117;
  s.scheme_id[2]=108;
  s.scheme_id[3]=108;
  s.scheme_id[4]=95;
  s.scheme_id[5]=112;
  s.scheme_id[6]=105;
  s.scheme_id[7]=110;
  s.scheme_id[8]=121;
  s.scheme_id[9]=105;
  s.scheme_id[10]=110;
  s.bootstrap_root_offset=2076712ULL; s.bootstrap_root_bytes=4096;
  return s;
}();
inline const hanime::ResourceReleaseSpec xiaohe_base = [] {
  hanime::ResourceReleaseSpec s;
  s.rule_version=1; s.resource_format=105; s.key_format=1;
  s.role=hanime::ResourceReleaseRole::BaseOrder;
  s.registry_namespace=5206528720952950785ULL; s.shared_bundle_id=17304919664082782091ULL;
  s.file_bytes=11986536ULL; s.file_sha256={{249,16,157,215,184,156,85,115,224,157,116,161,76,47,41,69,101,233,189,100,154,127,52,243,134,219,184,114,52,208,133,48}};
  s.merkle_root={{199,115,169,170,47,251,60,53,47,46,185,118,235,17,175,178,190,42,108,232,200,56,84,145,157,51,151,18,103,194,46,169}};
  s.required_shared_sha256={{246,162,175,236,71,89,255,135,158,96,82,246,197,130,96,111,51,231,130,112,165,61,130,209,227,222,184,83,81,161,115,57}};
  s.runtime_ref_count=242979; s.text_capacity=64; s.name_capacity=128;
  s.scheme_id[0]=120;
  s.scheme_id[1]=105;
  s.scheme_id[2]=97;
  s.scheme_id[3]=111;
  s.scheme_id[4]=104;
  s.scheme_id[5]=101;
  s.required_scheme_sha256={{154,5,133,241,102,46,99,55,160,147,108,106,84,121,177,163,255,158,118,132,9,39,222,240,90,206,20,7,13,165,99,40}}; s.ranking_rule_version=1;
  s.bootstrap_root_offset=0ULL; s.bootstrap_root_bytes=0;
  return s;
}();
inline const hanime::ResourceReleaseSpec full_pinyin_base = [] {
  hanime::ResourceReleaseSpec s;
  s.rule_version=1; s.resource_format=105; s.key_format=1;
  s.role=hanime::ResourceReleaseRole::BaseOrder;
  s.registry_namespace=5206528720952950785ULL; s.shared_bundle_id=17304919664082782091ULL;
  s.file_bytes=8219096ULL; s.file_sha256={{189,35,185,114,145,226,196,134,220,236,46,45,57,48,212,171,13,113,139,218,197,186,89,169,71,152,107,128,192,243,147,101}};
  s.merkle_root={{30,215,188,175,100,235,229,119,254,17,113,21,106,60,187,52,242,244,145,20,215,216,78,12,156,28,137,42,145,105,58,227}};
  s.required_shared_sha256={{246,162,175,236,71,89,255,135,158,96,82,246,197,130,96,111,51,231,130,112,165,61,130,209,227,222,184,83,81,161,115,57}};
  s.runtime_ref_count=242979; s.text_capacity=64; s.name_capacity=128;
  s.scheme_id[0]=102;
  s.scheme_id[1]=117;
  s.scheme_id[2]=108;
  s.scheme_id[3]=108;
  s.scheme_id[4]=95;
  s.scheme_id[5]=112;
  s.scheme_id[6]=105;
  s.scheme_id[7]=110;
  s.scheme_id[8]=121;
  s.scheme_id[9]=105;
  s.scheme_id[10]=110;
  s.required_scheme_sha256={{217,27,122,233,130,216,12,252,89,136,88,82,179,245,171,223,176,176,29,25,64,248,184,17,171,34,182,85,170,169,114,59}}; s.ranking_rule_version=1;
  s.bootstrap_root_offset=0ULL; s.bootstrap_root_bytes=0;
  return s;
}();
}
