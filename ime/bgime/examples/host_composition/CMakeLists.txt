cmake_minimum_required(VERSION 3.20)
project(bgime_host_composition VERSION 0.1 LANGUAGES CXX)

if(NOT UNIX)
  message(FATAL_ERROR "This example uses the existing POSIX Host file adapter.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(HANIME_BUILD_HOST_ADAPTERS ON)
set(HANIME_BUILD_FIXTURES ON)
set(HANIME_BUILD_TOOLS ON)
set(HANIME_BUILD_TESTS OFF)
get_filename_component(bgime_source_root "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(bgime_dependencies "${bgime_source_root}/integration/product/dependencies")
add_subdirectory("${bgime_dependencies}/core" "${CMAKE_CURRENT_BINARY_DIR}/core" EXCLUDE_FROM_ALL)
add_subdirectory("${bgime_dependencies}/extension" "${CMAKE_CURRENT_BINARY_DIR}/extension" EXCLUDE_FROM_ALL)

# Reuse the existing ResourceBuilder tool; unrelated tools and Navigation are
# excluded from the default build. No product CMake entry point is involved.
set(fixture_dir "${CMAKE_CURRENT_BINARY_DIR}/fixture")
set(fixture_files "${fixture_dir}/shared.hns" "${fixture_dir}/full_pinyin.hnm"
                  "${fixture_dir}/xiaohe.hnm" "${fixture_dir}/manifest.json")
add_custom_command(
  OUTPUT ${fixture_files}
  COMMAND $<TARGET_FILE:hanime_build_fixture> "${fixture_dir}"
  DEPENDS hanime_build_fixture
  COMMENT "Building the isolated Core small fixture"
  VERBATIM)
add_custom_target(bgime_host_composition_fixture DEPENDS ${fixture_files})

add_executable(bgime_host_composition main.cpp)
target_link_libraries(bgime_host_composition PRIVATE HanIME::Extension HanIME::Host HanIME::Core)
target_compile_options(bgime_host_composition PRIVATE -Wall -Wextra -Wpedantic -Werror)
add_dependencies(bgime_host_composition bgime_host_composition_fixture)

enable_testing()
add_test(NAME bgime_host_composition COMMAND bgime_host_composition "${fixture_dir}")
set_tests_properties(bgime_host_composition PROPERTIES TIMEOUT 60)
