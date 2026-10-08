#define main retained_daily_main
#include "whole_cost.cpp"
#undef main

namespace {
#include "reference_cases.inc"
}

int main(int argc, char** argv) {
  try {
    require(argc == 5, "logical transport scheme locate_slots");
    const std::string scheme = argv[3];
    const auto rules = scheme == "xiaohe" ? SchemeRules::R1Xiaohe : SchemeRules::FullPinyin;
    for (const auto& sample : cases) {
      if (scheme != sample.scheme) continue;
      R runtime;
      runtime.scenario = sample.id;
      runtime.mount(argv[1], argv[2], scheme, std::stoul(argv[4]));
      auto config = lc1::input_config(rules);
      // Existing same-domain reference policy: structure and ranking are fixed.
      // These larger computation budgets apply ONLY to this reference process.
      config.planning.core_work = 50000000;
      config.planning.local_work = 2000000;
      config.planning.read_bytes = 128 * 1024 * 1024;
      runtime.core_limit = config.planning.core_work;
      runtime.local_limit = config.planning.local_work;
      Composition input(runtime.scheme, runtime.shared, rules, config, runtime.storage.view());
      require(input.edit_raw(0, 0, sample.suffix) == Status::Ready, "reference suffix input");
      const auto status = runtime.query(input, "sufficient_suffix");
      require(status == Status::PageReady || status == Status::Unresolved ||
                  status == Status::NoPrefixMatch, "reference terminal status");
      const auto plan = input.planning_report();
      require(plan.valid && plan.core_cost.work_items < config.planning.core_work &&
                  plan.local_work < config.planning.local_work &&
                  plan.core_cost.read_bytes < config.planning.read_bytes,
              "reference must finish within computation budgets");
    }
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "REFERENCE_FAIL " << error.what() << '\n';
    return 1;
  }
}
