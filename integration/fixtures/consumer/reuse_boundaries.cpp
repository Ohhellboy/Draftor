#define main retained_daily_main
#include "whole_cost.cpp"
#undef main

int main(int argc, char** argv) {
  try {
    require(argc == 5, "logical transport scheme locate_slots");
    const std::string id = argv[3];
    const auto rules = id == "xiaohe" ? SchemeRules::R1Xiaohe : SchemeRules::FullPinyin;
    const std::string today = id == "xiaohe" ? "wojbtm" : "wojintian";
    const auto raw = today + today + today;
    R runtime;
    runtime.scenario = "SAME_CODE_BOUNDARIES";
    runtime.mount(argv[1], argv[2], id, std::stoul(argv[4]));
    Composition input(runtime.scheme, runtime.shared, rules, lc1::input_config(rules), runtime.storage.view());
    require(input.edit_raw(0, 0, raw) == Status::Ready, "boundary raw");
    require(runtime.query(input, "seed") == Status::PageReady, "seed completed graph");
    const auto old_selection = input.selection(0);
    require(runtime.query(input, "new_context", 0, 7) == Status::PageReady, "new context succeeds");
    require(observe::counters.retained == 0 && observe::counters.retired > 0,
            "context retires old facts before same-round reuse");
    bool positive = false, negative = false;
    for (std::size_t i = 0; i < observe::counters.count; ++i) {
      const auto& event = observe::counters.ranges[i];
      require(event.terminal != 3, "no retained positional fact after context change");
      if (event.terminal == 5) {
        positive |= event.matched;
        negative |= !event.matched;
      }
    }
    require(positive && negative, "both completed Match and authenticated NoMatch reused");
    require(input.select(old_selection) == Status::Stale, "fact reuse never revives Selection");
    require(runtime.query(input, "manual", 2) == Status::PageReady, "fresh manual page");
    runtime.select(input, "manual_select", "我");
    require(input.select(old_selection) == Status::Stale, "old span Selection cannot select the new span");
    for (unsigned part = 0; input.snapshot().state != CompositionState::AllSelected; ++part) {
      require(part < 16, "bounded boundary completion");
      require(runtime.query(input, "suffix_" + std::to_string(part)) == Status::PageReady,
              "independent suffix path");
      runtime.select(input, "suffix_select");
    }
    commit(runtime, input, "我今天我今天我今天", true);
    std::cout << "REUSE_BOUNDARY PASS context / Match / NoMatch / Selection / commit\n";

    for (bool retire_resource : {false, true}) {
      require(input.edit_raw(0, 0, raw) == Status::Ready, "new owner input raw");
      observe::counters = {};
      require(input.query_next({}, 7) == Status::Working, "start incremental scan");
      const auto operation = input.operation();
      for (std::size_t ticks = 0;; ++ticks) {
        require(ticks < 1000000, "reach bounded active-code scan");
        require(input.tick(operation, {1, 1024, 1}).status == Status::Working, "single charged unit");
        if (observe::counters.phases[12] > 8 && input.planning_report().queries >= 2) break;
      }
      require(input.planning_report().cache_hits == 0, "interrupt before first code conclusion");
      if (retire_resource) {
        runtime.order_reader->deactivate();
        auto status = Status::Working;
        for (unsigned ticks = 0; status == Status::Working; ++ticks) {
          require(ticks < 10000, "retired order must terminate");
          status = input.tick(operation, {1, 1024, 1}).status;
        }
        require(status == Status::ResourceError || status == Status::Stale,
                "unhealthy order is not a reusable completed miss");
        require(input.snapshot().raw == raw, "resource failure retains editable raw");
      } else {
        require(input.cancel(operation) == Status::Cancelled, "cancel incremental scan");
        require(input.snapshot().raw == raw, "cancel preserves raw");
        require(runtime.query(input, "retry_scan", 0, 7) == Status::PageReady, "explicit retry scan");
        require(input.cancel(operation) == Status::Stale, "old owner does not cancel successor");
        require(input.cancel_input(input.snapshot().input_id) == Status::Cancelled, "explicitly close this synthetic input");
      }
    }
    std::cout << "REUSE_BOUNDARY PASS incremental cancellation / order health\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "REUSE_BOUNDARY FAIL " << error.what() << '\n';
    return 1;
  }
}
