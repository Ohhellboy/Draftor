#define main retained_daily_main
#include "whole_cost.cpp"
#undef main

int main(int argc, char** argv) {
  try {
    require(argc == 5, "logical transport scheme locate_slots");
    const std::string id = argv[3];
    const bool xiaohe = id == "xiaohe";
    const auto rules = xiaohe ? SchemeRules::R1Xiaohe : SchemeRules::FullPinyin;
    const std::string first = xiaohe ? "wojbtm" : "wojintian";
    const std::string second = xiaohe ? "wojbyt" : "wojinyuan";
    require(first.size() == second.size() && first != second, "generated equal-length distinct codes");
    R runtime;
    runtime.scenario = "SYN_SAME_PREFIX_DIFFERENT_TAIL";
    runtime.mount(argv[1], argv[2], id, std::stoul(argv[4]));
    auto config = lc1::input_config(rules);
    config.planning.core_work = runtime.core_limit = 50000000;
    config.planning.local_work = runtime.local_limit = 2000000;
    config.planning.read_bytes = 128 * 1024 * 1024;
    Composition input(runtime.scheme, runtime.shared, rules, config, runtime.storage.view());
    require(input.edit_raw(0, 0, first + second) == Status::Ready, "explicit synthetic input");
    require(runtime.query(input, "different_tail") == Status::PageReady, "synthetic candidate");
    bool first_queried = false, second_queried = false;
    for (std::size_t i = 0; i < observe::counters.count; ++i) {
      const auto& event = observe::counters.ranges[i];
      if ((event.begin == 0 && event.end == first.size()) ||
          (event.begin == first.size() && event.end == first.size() + second.size())) {
        require(!event.hit && (event.terminal == 0 || event.terminal == 1),
                "equal prefix is insufficient: each full code must query and finish independently");
        if (event.begin == 0) first_queried = true;
        else second_queried = true;
      }
    }
    require(first_queried && second_queried, "both equal-length different tails were reached");
    std::cout << "DIFFERENT_TAIL PASS\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "DIFFERENT_TAIL FAIL " << error.what() << '\n';
    return 1;
  }
}
