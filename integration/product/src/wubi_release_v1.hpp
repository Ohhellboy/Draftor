#pragma once
#include "hanime/verified_resource.hpp"
// TEST_ONLY. Generated only after full-record equivalence and original transport checks.
// These compiled pins are trusted; on-resource JSON is never a trust source.
namespace wubi_release_v1 {
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
inline const hanime::ResourceReleaseSpec wubi86 = [] {
  hanime::ResourceReleaseSpec s;
  s.rule_version=1; s.resource_format=2; s.key_format=1;
  s.role=hanime::ResourceReleaseRole::Scheme;
  s.registry_namespace=5206528720952950785ULL; s.shared_bundle_id=17304919664082782091ULL;
  s.file_bytes=1482792ULL; s.file_sha256={{15,164,179,28,158,129,118,38,244,52,245,227,128,44,138,215,181,62,247,165,131,212,153,254,187,135,190,186,84,200,253,132}};
  s.merkle_root={{233,2,131,151,244,4,19,230,8,162,134,142,63,42,28,36,33,12,155,249,27,132,34,164,22,149,201,83,29,177,52,111}};
  s.required_shared_sha256={{246,162,175,236,71,89,255,135,158,96,82,246,197,130,96,111,51,231,130,112,165,61,130,209,227,222,184,83,81,161,115,57}};
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
  s.registry_namespace=5206528720952950785ULL; s.shared_bundle_id=17304919664082782091ULL;
  s.file_bytes=7369432ULL; s.file_sha256={{202,225,71,11,133,55,230,150,163,248,31,156,107,150,50,146,145,142,250,254,236,117,172,73,243,155,234,187,250,58,92,94}};
  s.merkle_root={{91,108,72,166,215,236,187,167,161,201,222,115,10,12,62,7,196,157,141,197,84,235,149,224,129,130,187,35,199,189,167,182}};
  s.required_shared_sha256={{246,162,175,236,71,89,255,135,158,96,82,246,197,130,96,111,51,231,130,112,165,61,130,209,227,222,184,83,81,161,115,57}};
  s.runtime_ref_count=242979; s.text_capacity=64; s.name_capacity=128;
  s.scheme_id[0]=119;
  s.scheme_id[1]=117;
  s.scheme_id[2]=98;
  s.scheme_id[3]=105;
  s.scheme_id[4]=56;
  s.scheme_id[5]=54;
  s.required_scheme_sha256={{15,164,179,28,158,129,118,38,244,52,245,227,128,44,138,215,181,62,247,165,131,212,153,254,187,135,190,186,84,200,253,132}}; s.ranking_rule_version=1;
  s.bootstrap_root_offset=0ULL; s.bootstrap_root_bytes=0;
  return s;
}();
}
