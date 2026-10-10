cmake_minimum_required(VERSION 3.20)
project(bgime_host_wubi86 VERSION 0.1 LANGUAGES CXX)

if(NOT UNIX)
  message(FATAL_ERROR "This example uses the existing POSIX Host file adapter.")
endif()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(HANIME_BUILD_HOST_ADAPTERS ON)
set(HANIME_BUILD_FIXTURES OFF)
set(HANIME_BUILD_TOOLS OFF)
set(HANIME_BUILD_TESTS OFF)
get_filename_component(bgime_source_root "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(bgime_dependencies "${bgime_source_root}/integration/product/dependencies")
add_subdirectory("${bgime_dependencies}/core" "${CMAKE_CURRENT_BINARY_DIR}/core" EXCLUDE_FROM_ALL)

# Example-local target over the original source; no product CMake or Extension.
add_library(bgime_demo_wubi86 STATIC "${bgime_dependencies}/wubi/src/wubi.cpp")
target_include_directories(bgime_demo_wubi86 PUBLIC "${bgime_dependencies}/wubi/include")
target_link_libraries(bgime_demo_wubi86 PUBLIC HanIME::Core)
target_compile_options(bgime_demo_wubi86 PRIVATE -Wall -Wextra -Wpedantic -Werror)

add_executable(bgime_host_wubi86 main.cpp fixture.cpp)
target_link_libraries(bgime_host_wubi86 PRIVATE bgime_demo_wubi86 HanIME::Host HanIME::Core)
target_compile_options(bgime_host_wubi86 PRIVATE -Wall -Wextra -Wpedantic -Werror)

# The same executable has a build-only resource-writing mode. Normal runs open
# these files read-only. Building the default target generates them automatically.
set(fixture_dir "${CMAKE_CURRENT_BINARY_DIR}/fixture")
set(fixture_files "${fixture_dir}/shared.hns" "${fixture_dir}/wubi86.hnm"
                  "${fixture_dir}/manifest.json")
add_custom_command(
  OUTPUT ${fixture_files}
  COMMAND $<TARGET_FILE:bgime_host_wubi86> --build-fixture "${fixture_dir}"
  DEPENDS bgime_host_wubi86
  COMMENT "Building the isolated artificial Wubi86 fixture"
  VERBATIM)
add_custom_target(bgime_host_wubi86_fixture ALL DEPENDS ${fixture_files})

enable_testing()
add_test(NAME bgime_host_wubi86 COMMAND bgime_host_wubi86 "${fixture_dir}")
set_tests_properties(bgime_host_wubi86 PROPERTIES TIMEOUT 60)
