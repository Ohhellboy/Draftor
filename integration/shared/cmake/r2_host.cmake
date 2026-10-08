# This target references the accepted R2 source without executing its project,
# dependency verifier, device configuration, resource build or tests.
file(REAL_PATH "${DRAFTOR_R2_SOURCE}" R2)
if(NOT EXISTS "${R2}/src/candidate_build.hpp")
  message(FATAL_ERROR "HB requires the accepted Phase2C_R2/source tree")
endif()
file(READ "${R2}/src/candidate_build.hpp" R2_ID)
if(NOT R2_ID MATCHES "DRAFTOR_S3_P23_PHASE2C_R2_20260924_V1_0_TF10M")
  message(FATAL_ERROR "Unexpected candidate identity; do not silently substitute R3")
endif()
set(R2_UNITS
  dependencies/common/document.cpp
  dependencies/common/body_editor.cpp
  dependencies/common/utf8.cpp
  dependencies/common/layout.cpp
  dependencies/common/visual_cursor_navigator.cpp
  dependencies/common/hanime_document_editor_port.cpp
  dependencies/common/src/hanime_document/types.cpp
  dependencies/common/src/hanime_document/record_codec.cpp
  dependencies/common/src/hanime_document/record_store.cpp
  dependencies/common/src/hanime_document/cooperative_record_writer.cpp
  dependencies/common/src/hanime_document/autosave_scheduler.cpp
  dependencies/common/src/hanime_document/document_catalog.cpp
  dependencies/common/src/hanime_document/persistence_service.cpp
  dependencies/common/src/hanime_document/document_session.cpp
  dependencies/common/src/hanime_document/entropy_id_source.cpp
  platform/core_memory.cpp
  platform/operation_profile.cpp
  src/vertical_presentation.cpp
  src/vertical_layout.cpp
  src/vertical_viewport.cpp
  src/vertical_raster.cpp
  src/vertical_punctuation.cpp
  src/vertical_ab.cpp
  src/ime_composition_font.cpp)
set(R2_ABSOLUTE_UNITS)
foreach(unit IN LISTS R2_UNITS)
  list(APPEND R2_ABSOLUTE_UNITS "${R2}/${unit}")
endforeach()
add_library(r2_host_reference STATIC ${R2_ABSOLUTE_UNITS})
target_include_directories(r2_host_reference PUBLIC
  "${R2}/platform" "${R2}/src" "${R2}/dependencies/common"
  "${R2}/dependencies/core/include" "${R2}/dependencies/extension/include")
target_compile_definitions(r2_host_reference PUBLIC
  HANIME_SAVE_SUBSTAGE_DIAGNOSTIC=0 HANIME_METADATA_CACHE_ENTRIES=128)
target_compile_options(r2_host_reference PRIVATE -Wall -Wextra -Wpedantic)
add_executable(hb_characterization tests/hb_characterization.cpp)
target_link_libraries(hb_characterization PRIVATE r2_host_reference)
add_executable(hb_export tests/hb_export.cpp)
target_link_libraries(hb_export PRIVATE r2_host_reference draftor_exchange)
target_compile_options(hb_characterization PRIVATE -Wall -Wextra -Wpedantic)
target_compile_options(hb_export PRIVATE -Wall -Wextra -Wpedantic)
foreach(case document saving partial storage create)
  add_test(NAME HB_characterization_${case} COMMAND hb_characterization ${case})
  set_tests_properties(HB_characterization_${case} PROPERTIES LABELS "HB;baseline" TIMEOUT 30)
endforeach()
foreach(case horizontal vertical old_save partial failures storage_controls bounds)
  add_test(NAME HB_export_${case} COMMAND hb_export ${case})
  set_tests_properties(HB_export_${case} PROPERTIES LABELS "HB;integration" TIMEOUT 30)
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/r2_units.txt" "")
foreach(unit IN LISTS R2_UNITS)
  file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/r2_units.txt" "${unit}\n")
endforeach()
