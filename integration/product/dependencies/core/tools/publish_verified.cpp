#include "hanime/resource_publisher.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
std::uint64_t number(const std::string& value) {
  if (value.empty() || value.front()=='-') throw std::runtime_error("invalid unsigned number");
  std::size_t end=0; const auto parsed=std::stoull(value,&end,0);
  if (end!=value.size()) throw std::runtime_error("trailing numeric input");
  return parsed;
}
std::uint32_t capacity(const std::string& value) {
  const auto n=number(value);
  if (!n || n>std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("invalid capacity");
  return static_cast<std::uint32_t>(n);
}
}
int main(int argc,char** argv) {
  try {
    hanime::PublishVerifiedOptions options;
    auto next=[&](int& i)->std::string {
      if (++i>=argc) throw std::runtime_error("missing option value");
      return argv[i];
    };
    for (int i=1; i<argc; ++i) {
      const std::string option=argv[i];
      if (option=="--shared") options.shared_path=next(i);
      else if (option=="--namespace") options.registry_namespace=number(next(i));
      else if (option=="--bundle") options.expected_shared_bundle_id=number(next(i));
      else if (option=="--scheme") {
        const auto id=next(i); const auto path=next(i); options.schemes.push_back({id,path});
      } else if (option=="--out") options.output_directory=next(i);
      else if (option=="--text-capacity") options.text_capacity=capacity(next(i));
      else if (option=="--name-capacity") options.name_capacity=capacity(next(i));
      else if (option=="--base-order") options.base_order=true;
      else if (option=="--base-order-format") options.base_order_format=capacity(next(i));
      else if (option=="--packed-auth") options.packed_auth=true;
      else if (option=="--packed-auth-all") {options.packed_auth=true;options.packed_auth_all=true;}
      else throw std::runtime_error("unknown option "+option);
    }
    const auto result=hanime::publish_verified_resources(options);
    if (!result.ok) throw std::runtime_error(result.error);
    std::cout<<"PUBLISH PASS final_byte_validated_files="<<result.resources.size()
             <<" host_paged_live_vector_bytes="<<result.host_paged_live_vector_bytes
             <<" host_largest_index_vector="<<result.host_index_peak_bytes<<'\n';
    for (const auto& resource:result.resources) {
      std::cout<<resource.logical_name<<" resource_bytes="<<resource.spec.file_bytes
               <<" index_bytes="<<resource.index_bytes
               <<" semantic_steps="<<resource.semantic_validation.steps
               <<" semantic_read_bytes="<<resource.semantic_validation.cost.read_bytes
               <<" text_steps="<<resource.text_admission.steps
               <<" text_read_bytes="<<resource.text_admission.cost.read_bytes<<'\n';
    }
  } catch (const std::exception& error) {
    std::cerr<<"PUBLISH REJECT "<<error.what()<<'\n'
             <<"usage: hanime_publish_verified --shared FILE --namespace UINT --scheme ID FILE "
               "[--scheme ID FILE] --out NEW_DIR [--bundle UINT] [--text-capacity N] [--name-capacity N] [--base-order] [--base-order-format 1|2] [--packed-auth|--packed-auth-all]\n";
    return 1;
  }
}
