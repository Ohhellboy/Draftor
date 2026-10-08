#pragma once
#include "hanime/resource_publisher.hpp"
#include "hanime/paged_shared_resource.hpp"
namespace hanime::detail {
// Host-only full reference; never linked into Core. Sources already underwent
// public complete validation in the publisher. The checker reads final bytes.
std::vector<std::uint8_t> build_base_order(const ResourceReader&,PagedSharedResource&);
bool check_base_order(const ResourceReader& derived,const ResourceReader& scheme,
                      PagedSharedResource&,std::string* error);
}
