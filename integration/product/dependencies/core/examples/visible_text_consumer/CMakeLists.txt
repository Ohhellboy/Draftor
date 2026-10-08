cmake_minimum_required(VERSION 3.20)
project(HanIMEVisibleTextConsumer LANGUAGES CXX)
set(CMAKE_CXX_EXTENSIONS OFF)
set(HANIME_CORE_PATH "" CACHE PATH "Explicit core source tree")
set(BASIC_HOST_PATH "" CACHE PATH "Frozen Basic Host 0.6 source tree")
set(HANIME_RELEASE_PIN_DIR "" CACHE PATH "Trusted published release_pin.hpp")
option(VISIBLE_SANITIZERS "Enable address/undefined without reducing warnings" OFF)
foreach(required HANIME_CORE_PATH BASIC_HOST_PATH HANIME_RELEASE_PIN_DIR)
  if(NOT IS_DIRECTORY "${${required}}")
    message(FATAL_ERROR "Set ${required} to an explicit existing directory")
  endif()
endforeach()

# Fail closed on accidental consumption of a parallel extension working tree.
function(require_basic_file relative expected)
  file(SHA256 "${BASIC_HOST_PATH}/${relative}" actual)
  if(NOT actual STREQUAL expected)
    message(FATAL_ERROR "Basic Host 0.6 frozen source mismatch: ${relative}")
  endif()
endfunction()
require_basic_file(src/composition.cpp 23acb58ae68396b8cc491cb0c1b3f2d92c80ed3c951b3aa2b674769125537d62)
require_basic_file(src/submission.cpp ff11354c2c5f068bf9c816fe5d30bc4131d6cd696b4c70913360bef70c7ac939)
require_basic_file(src/navigation.cpp 3b95305185a1dcd2f75a0f2a18ff35097b37332fe9450f22c249d87dbae0ef7e)
require_basic_file(src/navigation_syllables.inc 6aeeffc0040ba95233b6b3502ac06a7ade1a601da87f6e674fa5c43b6645ad6b)
require_basic_file(include/hanime_extension/extension.hpp a02ee825cd8d66b842a9e5224ffab3f1512a80a0ac90d9b1cd45b8e57b8f1b7f)
require_basic_file(include/hanime_extension/submission.hpp a969e174e7e34a7bc32840401ac642e79cbfedf50dd0578c48ed71cf86dba855)
require_basic_file(include/hanime_extension/navigation.hpp d30492714408494bee2be11c82ba3bd2b6f704f9b1e8c40064369834a155a38a)

set(HANIME_BUILD_HOST_ADAPTERS OFF CACHE BOOL "" FORCE)
set(HANIME_BUILD_FIXTURES OFF CACHE BOOL "" FORCE)
set(HANIME_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(HANIME_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(HANIME_ENABLE_SANITIZERS ${VISIBLE_SANITIZERS} CACHE BOOL "" FORCE)
add_subdirectory("${HANIME_CORE_PATH}" core)
target_compile_options(hanime_core PRIVATE -fstack-usage)

# The original extension top-level CMake pins its earlier core dependency.
# Build its exact three consumer translation units against one explicit core.
add_library(visible_basic_host STATIC
  "${BASIC_HOST_PATH}/src/composition.cpp"
  "${BASIC_HOST_PATH}/src/submission.cpp"
  "${BASIC_HOST_PATH}/src/navigation.cpp")
target_include_directories(visible_basic_host PUBLIC "${BASIC_HOST_PATH}/include")
target_link_libraries(visible_basic_host PUBLIC HanIME::Core)
target_compile_options(visible_basic_host PRIVATE -Wall -Wextra -Wpedantic -Werror -fstack-usage)
add_executable(visible_text_consumer main.cpp)
target_include_directories(visible_text_consumer PRIVATE "${HANIME_RELEASE_PIN_DIR}")
target_link_libraries(visible_text_consumer PRIVATE visible_basic_host)
target_compile_options(visible_text_consumer PRIVATE -Wall -Wextra -Wpedantic -Werror -fstack-usage)
