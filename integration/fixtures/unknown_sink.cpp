#define main original_adapter_tests_main
#include "adapter_tests.cpp"
#undef main
int main(){try{FixtureRun run;const auto root=fixture();bridge(root);run.success=true;return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
