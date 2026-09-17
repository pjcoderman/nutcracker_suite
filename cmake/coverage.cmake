# coverage.cmake
find_program(OPENCPPCOVERAGE_BIN OpenCppCoverage)

if(NOT OPENCPPCOVERAGE_BIN)
  message(FATAL_ERROR "OpenCppCoverage was not found on PATH. Restart VS Code if recently installed.")
endif()

file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}" NATIVE_SRC_DIR)
file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/coverage_html" NATIVE_HTML_DIR)

add_custom_target(coverage
  COMMAND "${OPENCPPCOVERAGE_BIN}"
          "--sources=${NATIVE_SRC_DIR}"
          "--excluded_sources=tests"
          "--excluded_sources=Test"
          "--excluded_sources=googletest"
          "--excluded_sources=vendor"
          "--excluded_sources=third_party"
          "--excluded_sources=vcpkg"
          "--excluded_sources=SYSTEM"
          "--excluded_sources=sqlite"
          "--export_type=html:${NATIVE_HTML_DIR}"
          "--"
          "$<TARGET_FILE:draft_schema_view_tests>"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  DEPENDS draft_schema_view_tests
  USES_TERMINAL
  COMMENT "Generating OpenCppCoverage report..."
)