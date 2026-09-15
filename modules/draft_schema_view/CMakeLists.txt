cmake_minimum_required(VERSION 3.14...3.28) 

set(DRAFT_SCHEMA_VIEW_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(DRAFT_SCHEMA_VIEW_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
set(DRAFT_SCHEMA_VIEW_PRIVATE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/private_include")

add_library(draft_schema_view STATIC
    "${DRAFT_SCHEMA_VIEW_SOURCE_DIR}/draft_schema_view.cpp"

    "${DRAFT_SCHEMA_VIEW_SOURCE_DIR}/base64.cpp"
    "${DRAFT_SCHEMA_VIEW_SOURCE_DIR}/draft_schema_valid.cpp"
    "${DRAFT_SCHEMA_VIEW_SOURCE_DIR}/dsv_common.cpp"
    "${DRAFT_SCHEMA_VIEW_SOURCE_DIR}/dsv_registry.cpp"
    "${DRAFT_SCHEMA_VIEW_SOURCE_DIR}/dsv_sql_helpers.cpp"
    "${DRAFT_SCHEMA_VIEW_SOURCE_DIR}/dsv_string_helpers.cpp"
    "${DRAFT_SCHEMA_VIEW_SOURCE_DIR}/dsv_vtable_triggers.cpp"
)

target_include_directories(draft_schema_view PUBLIC
    "${DRAFT_SCHEMA_VIEW_INCLUDE_DIR}"
)

target_include_directories(draft_schema_view PRIVATE
    "${DRAFT_SCHEMA_VIEW_PRIVATE_INCLUDE_DIR}"
)

# This automatically handles the include paths and underlying math engines for you.
target_link_libraries(draft_schema_view
    PUBLIC 
        sqlite3
        nlohmann_json_schema_validator
        nlohmann_json_schema_validator::validator
)

#target_compile_definitions(draft_schema_view PUBLIC SQLITE_ENABLE_PREUPDATE_HOOK SQLITE_ENABLE_SESSION SQLITE_ENABLE_COLUMN_METADATA)

if (BUILD_TESTING)
    add_subdirectory(tests)
endif()