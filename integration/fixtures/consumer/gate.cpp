#define main retained_daily_main
#include "whole_cost.cpp"
#undef main

int main(int argc, char** argv) {
  try {
    require(argc == 6, "logical transport scheme locate_slots budget");
    const std::string id = argv[3];
    const bool xiaohe = id == "xiaohe";
    const auto rules = xiaohe ? SchemeRules::R1Xiaohe : SchemeRules::FullPinyin;
    const std::string today = xiaohe ? "wojbtm" : "wojintian";
    const bool sufficient = std::string(argv[5]) == "reference";
    for (bool repeated : {true, false}) {
      R runtime;
      runtime.scenario = repeated ? "CONT3_GATE" : "L2_GATE";
      auto config = lc1::input_config(rules);
      if (sufficient) {
        config.planning.core_work = runtime.core_limit = 50000000;
        config.planning.local_work = runtime.local_limit = 2000000;
        config.planning.read_bytes = 128 * 1024 * 1024;
      }
      runtime.mount(argv[1], argv[2], id, std::stoul(argv[4]));
      Composition input(runtime.scheme, runtime.shared, rules, config, runtime.storage.view());
      const std::string word = xiaohe ? "vshxrfmbgshego" : "zhonghuarenmingongheguo";
      const std::string three = xiaohe ? "wjdjle" : "wandanle";
      const auto raw = repeated ? word + three : today;
      require(input.edit_raw(0, 0, raw) == Status::Ready, "gate raw");
      for (unsigned segment = 0; input.snapshot().state != CompositionState::AllSelected; ++segment) {
        require(segment < 16, "bounded gate path");
        require(runtime.query(input, "continuous_" + std::to_string(segment)) == Status::PageReady,
                "gate candidate");
        runtime.select(input, "continuous_select");
      }
      const std::string body(input.snapshot().selected_text);
      commit(runtime, input, body, true);
      runtime.memory(input);
    }
    std::cout << "GATE PASS\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "GATE FAIL " << error.what() << '\n';
    return 1;
  }
}
