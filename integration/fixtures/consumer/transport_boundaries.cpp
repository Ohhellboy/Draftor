#define main retained_daily_main
#include "whole_cost.cpp"
#undef main

int main(int argc, char** argv) {
  try {
    require(argc == 5, "logical transport scheme locate_slots");
    const std::string id = argv[3];
    const auto rules = id == "xiaohe" ? SchemeRules::R1Xiaohe : SchemeRules::FullPinyin;
    const std::string raw = id == "xiaohe" ? "wojbtm" : "wojintian";
    R runtime;
    runtime.scenario = "TRANSPORT_BOUNDARIES";
    runtime.mount(argv[1], argv[2], id, std::stoul(argv[4]));
    Composition input(runtime.scheme, runtime.shared, rules, lc1::input_config(rules), runtime.storage.view());
    require(input.edit_raw(0, 0, raw) == Status::Ready, "bounded seed raw");
    require(runtime.query(input, "initial") == Status::PageReady, "initial complete query");
    const auto old_selection = input.selection(0);
    const auto old_operation = input.operation();
    require(runtime.query(input, "manual", 2) == Status::PageReady, "explicit range");
    require(input.select(old_selection) == Status::Stale && input.cancel(old_operation) == Status::Stale,
            "old Selection/owner must not touch the new query");
    require(runtime.query(input, "changed_context", 0, 7) == Status::PageReady &&
                input.planning_report().cache_hits == 0, "changed context retires facts");
    std::cout << "BOUNDARY PASS context and stale tickets\n";

    require(input.drop_planning_cache() == Status::Ready, "retire completed facts");
    require(input.query_next({}, 7) == Status::Working, "start query for cancellation");
    const auto cancelled = input.operation();
    for (std::size_t ticks = 0; input.planning_report().queries == 0; ++ticks) {
      require(ticks < 100000, "bounded wait for first actual range");
      require(input.tick(cancelled, {1, 1024, 1}).status == Status::Working, "pending before completion");
    }
    require(input.cancel(cancelled) == Status::Cancelled, "cancel owned pending range");
    require(runtime.query(input, "retry_cancelled", 0, 7) == Status::PageReady &&
                input.planning_report().queries > 0 && input.planning_report().cache_hits == 0,
            "incomplete range remains Unknown and must be queried again");
    require(input.cancel(cancelled) == Status::Stale, "cancelled ticket must not cancel retry");
    std::cout << "BOUNDARY PASS incomplete is not NoMatch\n";

    // Remount the same authenticated bytes under a new shared activation token (same numeric mount epoch).
    // No resource/pin is modified. Existing Selection and facts must retire.
    const auto retired_selection = input.selection(0);
    // BaseOrder construction reads its header from authenticated cache only.
    // Reuse of an already-evicted header is not a legal remount fixture: create
    // fresh Readers and run the existing bootstrap/admission path instead.
    runtime.mount(argv[1], argv[2], id, std::stoul(argv[4]));
    require(runtime.admit() == TextAccessStatus::Ready, "new activation output admission");
    require(input.select(retired_selection) == Status::Stale, "old activation Selection stale");
    require(input.snapshot().raw == raw && input.snapshot().member_count == 0, "remount preserves raw");
    require(input.rebind() == Status::Ready, "explicit qualified rebind");
    require(runtime.query(input, "after_rebind", 0, 7) == Status::PageReady &&
                input.planning_report().cache_hits == 0, "retired facts not resurrected");
    std::cout << "BOUNDARY PASS activation and scheme epoch\n";

    runtime.order_reader->deactivate();
    require(!runtime.order_reader->healthy(), "order retirement latched");
    const auto failure = runtime.query(input, "retired_order", 0, 7);
    require(failure == Status::ResourceError || failure == Status::Stale,
            "retired order must fail explicitly, never NoMatch or a usable page");
    require(input.snapshot().raw == raw && input.snapshot().member_count == 0,
            "order failure preserves editable input");
    std::cout << "BOUNDARY PASS order retirement preserves raw\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << "BOUNDARY_FAIL " << error.what() << '\n';
    return 1;
  }
}
