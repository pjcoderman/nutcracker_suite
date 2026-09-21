cmake_minimum_required(VERSION 3.14...3.28) 

set(DRAFT_SCHEMA_VIEW_TESTS_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(DRAFT_SCHEMA_VIEW_TESTS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_executable(draft_schema_view_tests 
    "${DRAFT_SCHEMA_VIEW_TESTS_SOURCE_DIR}/draft_schema_view_test_base.cpp"
    "${DRAFT_SCHEMA_VIEW_TESTS_SOURCE_DIR}/draft_schema_view_base64_tests.cpp"
    "${DRAFT_SCHEMA_VIEW_TESTS_SOURCE_DIR}/draft_schema_view_module_init_tests.cpp"
    "${DRAFT_SCHEMA_VIEW_TESTS_SOURCE_DIR}/draft_schema_view_simple_schema_tests.cpp"
    "${DRAFT_SCHEMA_VIEW_TESTS_SOURCE_DIR}/draft_schema_view_complex_schema_tests.cpp"
)

# Link your library + gtest_main (handles the main() runner for you)
target_link_libraries(draft_schema_view_tests PRIVATE 
    draft_schema_view 
    GTest::gtest_main 
    GTest::gmock
)

# Grant test access to internal headers without exposing them to external consumers:
target_include_directories(draft_schema_view_tests PRIVATE
    "${DRAFT_SCHEMA_VIEW_TESTS_INCLUDE_DIR}" 
    "${CMAKE_CURRENT_SOURCE_DIR}/../private_include"
)

target_compile_options(draft_schema_view_tests PRIVATE /clang:-fprofile-instr-generate /clang:-fcoverage-mapping)
#target_link_options(draft_schema_view_tests PRIVATE /clang:-fprofile-instr-generate)

# Automatically registers tests with CTest
gtest_discover_tests(draft_schema_view_tests)
