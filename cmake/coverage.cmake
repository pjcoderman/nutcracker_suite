find_program(LLVM_PROFDATA_BIN llvm-profdata REQUIRED)
find_program(LLVM_COV_BIN llvm-cov REQUIRED)

file(TO_NATIVE_PATH "${CMAKE_BINARY_DIR}/coverage_html" NATIVE_HTML_DIR)

add_custom_target(coverage
  COMMAND ${CMAKE_COMMAND} -E env "LLVM_PROFILE_FILE=${CMAKE_BINARY_DIR}/coverage-%p.profraw" $<TARGET_FILE:draft_schema_view_tests>
  COMMAND "${LLVM_PROFDATA_BIN}" merge -sparse "${CMAKE_BINARY_DIR}/coverage-*.profraw" -o "${CMAKE_BINARY_DIR}/coverage.profdata"
  COMMAND "${LLVM_COV_BIN}" show $<TARGET_FILE:draft_schema_view_tests>
          -instr-profile="${CMAKE_BINARY_DIR}/coverage.profdata"
          -format=html
          -output-dir="${NATIVE_HTML_DIR}"
          -ignore-filename-regex=".*tests.*|.*googletest.*|.*vendor.*|.*third_party.*|.*vcpkg.*"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  DEPENDS draft_schema_view_tests
  USES_TERMINAL
  COMMENT "Generating LLVM source-based coverage report..."
)
