#pragma once
#include "hanime/verified_resource.hpp"
// Generated AFTER complete semantic validation of the final bytes.
// Trust these values only through the explicit firmware release process.
// Replacing a resource needs a new pin/firmware; TF JSON is not trusted.
namespace hanime_release {
inline const hanime::ResourceReleaseSpec shared = [] {
  hanime::ResourceReleaseSpec s;
  s.rule_version=1; s.resource_format=2; s.key_format=1;
  s.role=hanime::ResourceReleaseRole::Shared;
  s.registry_namespace=5206528720952950785ULL; s.shared_bundle_id=14778185187454200648ULL;
  s.file_bytes=29349067ULL; s.file_sha256={{111,101,137,47,72,252,212,150,47,138,248,9,143,199,11,222,19,175,114,131,39,58,56,210,24,95,147,86,219,138,64,188}};
  s.merkle_root={{216,101,182,110,26,12,222,153,54,123,195,203,192,210,65,119,117,180,236,221,169,113,182,21,107,151,28,169,62,48,60,242}};
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
  s.registry_namespace=5206528720952950785ULL; s.shared_bundle_id=14778185187454200648ULL;
  s.file_bytes=2756648ULL; s.file_sha256={{232,250,127,103,179,42,91,108,174,199,206,15,81,183,163,214,42,22,197,225,225,52,202,141,113,99,20,198,29,160,184,50}};
  s.merkle_root={{102,70,86,233,49,197,189,166,242,245,236,134,117,204,160,220,208,26,6,127,121,72,4,204,5,26,118,219,219,84,60,44}};
  s.required_shared_sha256={{111,101,137,47,72,252,212,150,47,138,248,9,143,199,11,222,19,175,114,131,39,58,56,210,24,95,147,86,219,138,64,188}};
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
  s.registry_namespace=5206528720952950785ULL; s.shared_bundle_id=14778185187454200648ULL;
  s.file_bytes=2080808ULL; s.file_sha256={{163,164,219,112,247,73,155,199,171,107,156,106,84,43,195,204,117,136,131,165,32,232,236,160,174,10,203,91,98,132,3,75}};
  s.merkle_root={{12,195,29,137,25,143,219,129,245,153,190,184,21,166,77,204,44,91,45,248,126,129,130,226,46,217,21,79,0,56,253,38}};
  s.required_shared_sha256={{111,101,137,47,72,252,212,150,47,138,248,9,143,199,11,222,19,175,114,131,39,58,56,210,24,95,147,86,219,138,64,188}};
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
  s.rule_version=1; s.resource_format=2; s.key_format=1;
  s.role=hanime::ResourceReleaseRole::BaseOrder;
  s.registry_namespace=5206528720952950785ULL; s.shared_bundle_id=14778185187454200648ULL;
  s.file_bytes=13873768ULL; s.file_sha256={{63,58,33,34,51,193,125,237,174,181,178,162,60,158,95,250,47,130,121,55,150,225,32,157,3,209,47,88,92,115,255,12}};
  s.merkle_root={{128,135,52,225,156,29,68,98,221,172,23,245,211,138,199,197,62,41,147,73,77,168,148,164,232,203,237,68,82,179,158,40}};
  s.required_shared_sha256={{111,101,137,47,72,252,212,150,47,138,248,9,143,199,11,222,19,175,114,131,39,58,56,210,24,95,147,86,219,138,64,188}};
  s.runtime_ref_count=242979; s.text_capacity=64; s.name_capacity=128;
  s.scheme_id[0]=120;
  s.scheme_id[1]=105;
  s.scheme_id[2]=97;
  s.scheme_id[3]=111;
  s.scheme_id[4]=104;
  s.scheme_id[5]=101;
  s.required_scheme_sha256={{232,250,127,103,179,42,91,108,174,199,206,15,81,183,163,214,42,22,197,225,225,52,202,141,113,99,20,198,29,160,184,50}}; s.ranking_rule_version=1;
  s.bootstrap_root_offset=0ULL; s.bootstrap_root_bytes=0;
  return s;
}();
inline const hanime::ResourceReleaseSpec full_pinyin_base = [] {
  hanime::ResourceReleaseSpec s;
  s.rule_version=1; s.resource_format=2; s.key_format=1;
  s.role=hanime::ResourceReleaseRole::BaseOrder;
  s.registry_namespace=5206528720952950785ULL; s.shared_bundle_id=14778185187454200648ULL;
  s.file_bytes=10102232ULL; s.file_sha256={{239,53,250,87,10,220,245,108,119,49,167,42,2,113,246,82,143,38,159,112,153,201,72,221,240,217,24,173,98,154,116,29}};
  s.merkle_root={{44,235,222,199,242,105,236,244,207,38,41,46,156,9,140,95,138,0,120,129,67,29,236,20,137,195,211,79,119,191,240,10}};
  s.required_shared_sha256={{111,101,137,47,72,252,212,150,47,138,248,9,143,199,11,222,19,175,114,131,39,58,56,210,24,95,147,86,219,138,64,188}};
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
  s.required_scheme_sha256={{163,164,219,112,247,73,155,199,171,107,156,106,84,43,195,204,117,136,131,165,32,232,236,160,174,10,203,91,98,132,3,75}}; s.ranking_rule_version=1;
  s.bootstrap_root_offset=0ULL; s.bootstrap_root_bytes=0;
  return s;
}();
} // namespace hanime_release
