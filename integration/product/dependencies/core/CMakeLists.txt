cmake_minimum_required(VERSION 3.20)

project(HanIMEIndependentCore VERSION 0.13.1 LANGUAGES CXX)
# Distribution candidate 0.13.1-rc.1; runtime public API remains 0.13.0.
set(HANIME_RELEASE_ID "HanIME-Core-0.13.1-rc.1")
set(HANIME_PUBLIC_API_VERSION "0.13.0")
set(CMAKE_CXX_EXTENSIONS OFF)
set(HANIME_BASE_ORDER_LOCATE_ENTRIES "8" CACHE STRING "Default positive directory facts per BaseOrderIndex (0, 4, 8)")
if(NOT HANIME_BASE_ORDER_LOCATE_ENTRIES MATCHES "^(0|4|8)$")
  message(FATAL_ERROR "HANIME_BASE_ORDER_LOCATE_ENTRIES must be 0, 4 or 8")
endif()

option(HANIME_ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)
# Standalone development retains the original full build. Embedded consumers
# opt into Host adapters/tools/fixtures/tests instead of inheriting them.
set(hanime_development_default OFF)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(hanime_development_default ON)
endif()
option(HANIME_BUILD_HOST_ADAPTERS "Build POSIX Host file adapters" ${hanime_development_default})
option(HANIME_BUILD_FIXTURES "Build the small identity fixture library" ${hanime_development_default})
option(HANIME_BUILD_TOOLS "Build Host resource construction tools" ${hanime_development_default})
option(HANIME_BUILD_TESTS "Build and register the original full Host regression suite" ${hanime_development_default})
if(HANIME_BUILD_TESTS AND (NOT HANIME_BUILD_HOST_ADAPTERS OR NOT HANIME_BUILD_FIXTURES OR NOT HANIME_BUILD_TOOLS))
  message(FATAL_ERROR "HANIME_BUILD_TESTS requires HOST_ADAPTERS, FIXTURES and TOOLS; disable tests for runtime-only consumers")
endif()

add_library(hanime_core STATIC
  src/core.cpp
  src/stable_key.cpp
  src/paged_scheme_index.cpp
  src/paged_shared_resource.cpp
  src/exact_ranker.cpp
  src/base_order.cpp
  src/member_eligibility.cpp
  src/bounded_output.cpp
  src/candidate_session.cpp
  src/composition_commit.cpp
  src/state_v3.cpp
  src/bounded_user_state.cpp
  src/verified_resource.cpp
  src/packed_auth.cpp
)
target_include_directories(hanime_core PUBLIC include)
target_compile_features(hanime_core PUBLIC cxx_std_17)
target_compile_options(hanime_core PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_library(HanIME::Core ALIAS hanime_core)
set_target_properties(hanime_core PROPERTIES VERSION ${PROJECT_VERSION})
target_compile_definitions(hanime_core PRIVATE HANIME_BASE_ORDER_LOCATE_ENTRIES=${HANIME_BASE_ORDER_LOCATE_ENTRIES})

if(HANIME_BUILD_FIXTURES)
add_library(hanime_small_fixture STATIC
  src/small_fixture.cpp
)
target_link_libraries(hanime_small_fixture PUBLIC hanime_core)
target_compile_features(hanime_small_fixture PUBLIC cxx_std_17)
target_compile_options(hanime_small_fixture PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

if(HANIME_BUILD_HOST_ADAPTERS)
add_library(hanime_host_adapters STATIC
  src/file_user_state_persistence.cpp
  src/host_state_files.cpp
  src/host_resource_file.cpp
  src/resource_publisher.cpp
  src/packed_auth_build.cpp
  src/base_order_build.cpp
  src/base_order_paged_build.cpp
)
target_link_libraries(hanime_host_adapters PUBLIC hanime_core)
target_compile_features(hanime_host_adapters PUBLIC cxx_std_17)
target_compile_options(hanime_host_adapters PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_library(HanIME::Host ALIAS hanime_host_adapters)
endif()

if(HANIME_ENABLE_SANITIZERS)
  foreach(target hanime_core hanime_small_fixture hanime_host_adapters)
    if(TARGET ${target})
    target_compile_options(${target} PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(${target} PUBLIC -fsanitize=address,undefined)
    endif()
  endforeach()
endif()

if(HANIME_BUILD_TOOLS)
if(HANIME_BUILD_FIXTURES)
add_executable(hanime_build_fixture tools/build_fixture.cpp)
target_link_libraries(hanime_build_fixture PRIVATE hanime_small_fixture)
target_compile_options(hanime_build_fixture PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(hanime_build_trial tools/build_trial_resources.cpp)
target_link_libraries(hanime_build_trial PRIVATE hanime_core)
target_compile_options(hanime_build_trial PRIVATE -Wall -Wextra -Wpedantic -Werror)
if(HANIME_BUILD_HOST_ADAPTERS)
add_executable(hanime_publish_verified tools/publish_verified.cpp)
target_link_libraries(hanime_publish_verified PRIVATE hanime_host_adapters)
target_compile_options(hanime_publish_verified PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
endif()

if(HANIME_BUILD_TESTS)
add_executable(hanime_paged_publication_tests tests/test_paged_publication.cpp)
target_link_libraries(hanime_paged_publication_tests PRIVATE hanime_host_adapters hanime_small_fixture)
target_compile_options(hanime_paged_publication_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_test(NAME hanime_paged_publication COMMAND hanime_paged_publication_tests "${CMAKE_CURRENT_BINARY_DIR}/paged-publication-tests")
if(HANIME_ENABLE_SANITIZERS)
  set_tests_properties(hanime_paged_publication PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;UBSAN_OPTIONS=halt_on_error=1")
endif()
add_executable(hanime_output_reuse_tests tests/test_output_reuse.cpp)
target_link_libraries(hanime_output_reuse_tests PRIVATE hanime_host_adapters hanime_small_fixture)
target_compile_options(hanime_output_reuse_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -fstack-usage)
set_property(SOURCE src/bounded_output.cpp APPEND PROPERTY COMPILE_OPTIONS -fstack-usage)
add_test(NAME hanime_output_reuse COMMAND hanime_output_reuse_tests "${CMAKE_CURRENT_BINARY_DIR}/output-reuse-tests")
if(HANIME_ENABLE_SANITIZERS)
  set_tests_properties(hanime_output_reuse PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;UBSAN_OPTIONS=halt_on_error=1")
endif()
add_executable(hanime_packed_auth_tests tests/test_packed_auth.cpp)
target_link_libraries(hanime_packed_auth_tests PRIVATE hanime_host_adapters hanime_small_fixture)
target_compile_options(hanime_packed_auth_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -fstack-usage)
set_property(SOURCE src/verified_resource.cpp src/packed_auth.cpp APPEND PROPERTY COMPILE_OPTIONS -fstack-usage)
add_test(NAME hanime_packed_auth COMMAND hanime_packed_auth_tests "${CMAKE_CURRENT_BINARY_DIR}/packed-auth-tests")
if(HANIME_ENABLE_SANITIZERS)
  set_tests_properties(hanime_packed_auth PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;UBSAN_OPTIONS=halt_on_error=1")
endif()
add_executable(hanime_base_order_tests tests/test_base_order.cpp)
target_link_libraries(hanime_base_order_tests PRIVATE hanime_host_adapters hanime_small_fixture)
target_compile_options(hanime_base_order_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -fstack-usage)
set_property(SOURCE src/base_order.cpp src/member_eligibility.cpp APPEND PROPERTY COMPILE_OPTIONS -fstack-usage)
add_test(NAME hanime_base_order_membership COMMAND hanime_base_order_tests "${CMAKE_CURRENT_BINARY_DIR}/base-order-tests")
if(HANIME_ENABLE_SANITIZERS)
  set_tests_properties(hanime_base_order_membership PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;UBSAN_OPTIONS=halt_on_error=1")
endif()
add_executable(hanime_verified_tests tests/test_verified_resource.cpp)
add_executable(hanime_visible_text_tests tests/test_visible_text_io.cpp)
target_link_libraries(hanime_visible_text_tests PRIVATE hanime_host_adapters hanime_small_fixture)
target_compile_options(hanime_visible_text_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -fstack-usage)
set_property(SOURCE src/paged_shared_resource.cpp APPEND PROPERTY COMPILE_OPTIONS -fstack-usage)
add_test(NAME hanime_visible_text_io COMMAND hanime_visible_text_tests "${CMAKE_CURRENT_BINARY_DIR}/visible-text-tests")
if(HANIME_ENABLE_SANITIZERS)
  set_tests_properties(hanime_visible_text_io PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;UBSAN_OPTIONS=halt_on_error=1")
endif()
target_link_libraries(hanime_verified_tests PRIVATE hanime_core)
target_compile_options(hanime_verified_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -fstack-usage)
add_executable(hanime_publisher_tests tests/test_resource_publisher.cpp)
target_link_libraries(hanime_publisher_tests PRIVATE hanime_host_adapters hanime_small_fixture)
target_compile_options(hanime_publisher_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -fstack-usage)
add_test(NAME hanime_verified_blocks COMMAND hanime_verified_tests)
add_test(NAME hanime_verified_publication COMMAND hanime_publisher_tests "${CMAKE_CURRENT_BINARY_DIR}/publication-tests")
set_property(SOURCE src/verified_resource.cpp APPEND PROPERTY COMPILE_OPTIONS -fstack-usage)
if(HANIME_ENABLE_SANITIZERS)
  set_tests_properties(hanime_verified_blocks hanime_verified_publication PROPERTIES
    ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;UBSAN_OPTIONS=halt_on_error=1")
endif()
add_executable(hanime_tests tests/test_main.cpp)
target_link_libraries(hanime_tests PRIVATE hanime_small_fixture)
target_compile_options(hanime_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(hanime_step2a_tests tests/test_paged_query.cpp)
target_link_libraries(hanime_step2a_tests PRIVATE hanime_core)
target_compile_options(hanime_step2a_tests
  PRIVATE -Wall -Wextra -Wpedantic -Werror
)

add_executable(hanime_file_persistence_probe tests/file_persistence_probe.cpp)
target_link_libraries(hanime_file_persistence_probe
  PRIVATE hanime_small_fixture hanime_host_adapters
)
target_compile_options(hanime_file_persistence_probe
  PRIVATE -Wall -Wextra -Wpedantic -Werror
)

add_executable(hanime_step2b_tests tests/test_shared_resource.cpp)
target_link_libraries(hanime_step2b_tests PRIVATE hanime_small_fixture hanime_host_adapters)
target_compile_options(hanime_step2b_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)

enable_testing()
add_executable(hanime_composition_tests tests/test_composition_commit.cpp tests/rank_allocation_probe.cpp)
target_link_libraries(hanime_composition_tests PRIVATE hanime_core)
target_compile_options(hanime_composition_tests PRIVATE -Wall -Wextra -Wpedantic -Werror -fstack-usage)
set_property(SOURCE src/composition_commit.cpp APPEND PROPERTY COMPILE_OPTIONS -fstack-usage)
if(HANIME_ENABLE_SANITIZERS)
  target_compile_definitions(hanime_composition_tests PRIVATE HANIME_PROBE_DISABLED=1)
endif()
add_test(NAME hanime_composition_commit COMMAND hanime_composition_tests)
if(HANIME_ENABLE_SANITIZERS)
  set_tests_properties(hanime_composition_commit PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;UBSAN_OPTIONS=halt_on_error=1")
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_executable(hanime_real_host tests/real_resource_host.cpp tests/rank_allocation_probe.cpp)
add_executable(hanime_production_host tests/production_resource_host.cpp tests/rank_allocation_probe.cpp)
target_link_libraries(hanime_production_host PRIVATE hanime_host_adapters)
target_compile_options(hanime_production_host PRIVATE -Wall -Wextra -Wpedantic -Werror -fstack-usage)
if(HANIME_ENABLE_SANITIZERS)
  target_compile_definitions(hanime_production_host PRIVATE HANIME_PROBE_DISABLED=1)
endif()
target_link_libraries(hanime_real_host PRIVATE hanime_host_adapters)
target_compile_options(hanime_real_host PRIVATE -Wall -Wextra -Wpedantic -Werror -fstack-usage)
if(HANIME_ENABLE_SANITIZERS)
  target_compile_definitions(hanime_real_host PRIVATE HANIME_PROBE_DISABLED=1)
endif()
add_test(NAME hanime_step2f_conversion COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_step2f_conversion.py" $<TARGET_FILE:hanime_build_trial>)
foreach(order a b)
  set(reverse_arg)
  if(order STREQUAL "b")
    set(reverse_arg --reverse)
  endif()
  add_test(NAME hanime_step2f_build_${order} COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/convert_step2f.py"
    --source "${CMAKE_CURRENT_SOURCE_DIR}/resources/step2f/source_v1_1.zip"
    --snapshot "${CMAKE_CURRENT_SOURCE_DIR}/resources/step2f/candidate_adoption.json"
    --full-pinyin "${CMAKE_CURRENT_SOURCE_DIR}/resources/step2f/full_pinyin.json"
    --builder $<TARGET_FILE:hanime_build_trial> --out "${CMAKE_CURRENT_BINARY_DIR}/step2f/order_${order}" ${reverse_arg})
  set_tests_properties(hanime_step2f_build_${order} PROPERTIES FIXTURES_SETUP trial_${order})
endforeach()
add_test(NAME hanime_step2f_file_chain COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/run_real_host.py"
  $<TARGET_FILE:hanime_real_host> "${CMAKE_CURRENT_BINARY_DIR}/step2f/order_a" "${CMAKE_CURRENT_BINARY_DIR}/step2f/order_b" "${CMAKE_CURRENT_BINARY_DIR}/step2f/states")
set_tests_properties(hanime_step2f_file_chain PROPERTIES FIXTURES_REQUIRED "trial_a;trial_b")
add_executable(hanime_state_tests tests/test_bounded_state.cpp tests/rank_allocation_probe.cpp)
target_link_libraries(hanime_state_tests PRIVATE hanime_host_adapters)
target_compile_options(hanime_state_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_executable(hanime_state_v3_restart tests/state_v3_restart.cpp tests/rank_allocation_probe.cpp)
target_link_libraries(hanime_state_v3_restart PRIVATE hanime_host_adapters)
target_compile_options(hanime_state_v3_restart PRIVATE -Wall -Wextra -Wpedantic -Werror)
if(HANIME_ENABLE_SANITIZERS)
  target_compile_definitions(hanime_state_tests PRIVATE HANIME_PROBE_DISABLED=1)
  target_compile_definitions(hanime_state_v3_restart PRIVATE HANIME_PROBE_DISABLED=1)
endif()
add_test(NAME hanime_step2e_bounded_state COMMAND hanime_state_tests "${CMAKE_CURRENT_BINARY_DIR}/step2e-state-tests")
add_test(NAME hanime_state_v3_write COMMAND hanime_state_v3_restart write "${CMAKE_CURRENT_BINARY_DIR}/step2e-restart")
add_test(NAME hanime_state_v3_restart_read COMMAND hanime_state_v3_restart read "${CMAKE_CURRENT_BINARY_DIR}/step2e-restart")
set_tests_properties(hanime_state_v3_write PROPERTIES FIXTURES_SETUP hanime_v3_fixture)
set_tests_properties(hanime_state_v3_restart_read PROPERTIES FIXTURES_REQUIRED hanime_v3_fixture DEPENDS hanime_state_v3_write)
add_test(NAME hanime_state_v3_uncertain_write COMMAND hanime_state_v3_restart uncertain-write "${CMAKE_CURRENT_BINARY_DIR}/step2e-uncertain")
add_test(NAME hanime_state_v3_uncertain_restart COMMAND hanime_state_v3_restart uncertain-read "${CMAKE_CURRENT_BINARY_DIR}/step2e-uncertain")
set_tests_properties(hanime_state_v3_uncertain_write PROPERTIES FIXTURES_SETUP hanime_v3_uncertain_fixture)
set_tests_properties(hanime_state_v3_uncertain_restart PROPERTIES FIXTURES_REQUIRED hanime_v3_uncertain_fixture DEPENDS hanime_state_v3_uncertain_write)
add_executable(hanime_ranking_tests tests/test_exact_ranker.cpp tests/rank_allocation_probe.cpp)
target_link_libraries(hanime_ranking_tests PRIVATE hanime_core)
target_compile_options(hanime_ranking_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
if(HANIME_ENABLE_SANITIZERS)
  target_compile_definitions(hanime_ranking_tests PRIVATE HANIME_PROBE_DISABLED=1)
endif()
add_test(NAME hanime_complete_code_ranking COMMAND hanime_ranking_tests)
add_executable(hanime_session_tests tests/test_candidate_session.cpp tests/rank_allocation_probe.cpp)
target_link_libraries(hanime_session_tests PRIVATE hanime_core)
target_compile_options(hanime_session_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
if(HANIME_ENABLE_SANITIZERS)
  target_compile_definitions(hanime_session_tests PRIVATE HANIME_PROBE_DISABLED=1)
endif()
add_test(NAME hanime_output_commit_session COMMAND hanime_session_tests)
add_test(NAME hanime_a1_a13_and_review_regressions COMMAND hanime_tests)
add_test(NAME hanime_step2a_paged_query COMMAND hanime_step2a_tests)
add_test(NAME hanime_step2b_shared_records COMMAND hanime_step2b_tests "${CMAKE_CURRENT_BINARY_DIR}/step2b-test-state")
set(HANIME_FILE_STATE_PROBE_PATH
  "${CMAKE_CURRENT_BINARY_DIR}/hanime_file_state_restart.txt"
)
add_test(
  NAME hanime_file_state_write
  COMMAND hanime_file_persistence_probe write "${HANIME_FILE_STATE_PROBE_PATH}"
)
add_test(
  NAME hanime_file_state_restart_read
  COMMAND hanime_file_persistence_probe read "${HANIME_FILE_STATE_PROBE_PATH}"
)
set_tests_properties(hanime_file_state_restart_read PROPERTIES
  DEPENDS hanime_file_state_write
  FIXTURES_REQUIRED hanime_file_state_fixture
)
set_tests_properties(hanime_file_state_write PROPERTIES
  FIXTURES_SETUP hanime_file_state_fixture
)

if(HANIME_ENABLE_SANITIZERS)
  set_tests_properties(
    hanime_step2f_conversion
    hanime_step2f_build_a
    hanime_step2f_build_b
    hanime_step2f_file_chain
    hanime_a1_a13_and_review_regressions
    hanime_step2e_bounded_state
    hanime_state_v3_write
    hanime_state_v3_restart_read
    hanime_state_v3_uncertain_write
    hanime_state_v3_uncertain_restart
    hanime_step2a_paged_query
    hanime_step2b_shared_records
    hanime_complete_code_ranking
    hanime_output_commit_session
    hanime_file_state_write
    hanime_file_state_restart_read
    PROPERTIES ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0;UBSAN_OPTIONS=halt_on_error=1"
  )
endif()
endif() # HANIME_BUILD_TESTS
