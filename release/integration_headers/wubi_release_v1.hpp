#pragma once
#include "hanime/verified_resource.hpp"
// Generated AFTER complete semantic validation of the final bytes.
// Trust these values only through the explicit firmware release process.
// Replacing a resource needs a new pin/firmware; TF JSON is not trusted.
namespace wubi_release_v1 {
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
inline const hanime::ResourceReleaseSpec wubi86 = [] {
  hanime::ResourceReleaseSpec s;
  s.rule_version=1; s.resource_format=2; s.key_format=1;
  s.role=hanime::ResourceReleaseRole::Scheme;
  s.registry_namespace=5206528720952950785ULL; s.shared_bundle_id=14778185187454200648ULL;
  s.file_bytes=1482792ULL; s.file_sha256={{62,245,1,28,84,42,201,71,192,130,164,231,82,85,214,113,164,3,116,218,125,67,48,89,54,243,12,171,13,203,43,123}};
  s.merkle_root={{137,253,221,48,45,10,99,141,28,139,30,75,210,229,8,86,2,92,30,204,246,242,184,250,201,96,81,125,49,38,234,39}};
  s.required_shared_sha256={{111,101,137,47,72,252,212,150,47,138,248,9,143,199,11,222,19,175,114,131,39,58,56,210,24,95,147,86,219,138,64,188}};
  s.runtime_ref_count=242979; s.text_capacity=64; s.name_capacity=128;
  s.scheme_id[0]=119;
  s.scheme_id[1]=117;
  s.scheme_id[2]=98;
  s.scheme_id[3]=105;
  s.scheme_id[4]=56;
  s.scheme_id[5]=54;
  s.bootstrap_root_offset=1478696ULL; s.bootstrap_root_bytes=4096;
  return s;
}();
inline const hanime::ResourceReleaseSpec wubi86_base = [] {
  hanime::ResourceReleaseSpec s;
  s.rule_version=1; s.resource_format=2; s.key_format=1;
  s.role=hanime::ResourceReleaseRole::BaseOrder;
  s.registry_namespace=5206528720952950785ULL; s.shared_bundle_id=14778185187454200648ULL;
  s.file_bytes=7369432ULL; s.file_sha256={{202,225,71,11,133,55,230,150,163,248,31,156,107,150,50,146,145,142,250,254,236,117,172,73,243,155,234,187,250,58,92,94}};
  s.merkle_root={{91,108,72,166,215,236,187,167,161,201,222,115,10,12,62,7,196,157,141,197,84,235,149,224,129,130,187,35,199,189,167,182}};
  s.required_shared_sha256={{111,101,137,47,72,252,212,150,47,138,248,9,143,199,11,222,19,175,114,131,39,58,56,210,24,95,147,86,219,138,64,188}};
  s.runtime_ref_count=242979; s.text_capacity=64; s.name_capacity=128;
  s.scheme_id[0]=119;
  s.scheme_id[1]=117;
  s.scheme_id[2]=98;
  s.scheme_id[3]=105;
  s.scheme_id[4]=56;
  s.scheme_id[5]=54;
  s.required_scheme_sha256={{62,245,1,28,84,42,201,71,192,130,164,231,82,85,214,113,164,3,116,218,125,67,48,89,54,243,12,171,13,203,43,123}}; s.ranking_rule_version=1;
  s.bootstrap_root_offset=0ULL; s.bootstrap_root_bytes=0;
  return s;
}();
} // namespace wubi_release_v1
