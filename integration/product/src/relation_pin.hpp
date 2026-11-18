#pragma once
#include "hanime/verified_resource.hpp"
namespace r1 {
inline const hanime::ResourceReleaseSpec relation_pin=[] { hanime::ResourceReleaseSpec p;
p.role=hanime::ResourceReleaseRole::RelationLocations; p.resource_format=301;
p.file_bytes=3587304ULL;
p.registry_namespace=5206528720952950785ULL;
p.shared_bundle_id=17304919664082782091ULL;
p.runtime_ref_count=242979ULL;
p.file_sha256={{6,74,35,4,159,138,170,5,142,88,30,174,49,143,160,77,25,207,101,162,143,52,74,206,15,74,0,175,82,159,233,25}};
p.merkle_root={{170,70,218,118,98,98,88,87,161,240,203,3,233,6,9,32,132,86,81,4,225,46,177,68,214,159,48,206,92,171,112,249}};
p.required_shared_sha256={{246,162,175,236,71,89,255,135,158,96,82,246,197,130,96,111,51,231,130,112,165,61,130,209,227,222,184,83,81,161,115,57}};
return p; }();
}
