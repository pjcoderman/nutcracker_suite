cmake_minimum_required(VERSION 3.14...3.28) 

set(SEMANTIC_DATABASE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(SEMANTIC_DATABASE_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(semantic_database STATIC
    "${SEMANTIC_DATABASE_SOURCE_DIR}/semantic_database.cpp"
)

target_include_directories(semantic_database PUBLIC
    "${SEMANTIC_DATABASE_INCLUDE_DIR}"
)

# Enforce C++20 as a minimum for this target
#target_compile_features(semantic_database PRIVATE cxx_std_20)

# Add this line right below it to force the Microsoft STL headers to wake up
#target_compile_definitions(semantic_database PRIVATE _HAS_CXX20=1)

# This automatically handles the include paths and underlying math engines for you.
target_link_libraries(semantic_database
    PUBLIC 
        SQLiteCpp
        sqlite3
        sqlite-vec
        nlohmann_json_schema_validator
        nlohmann_json_schema_validator::validator
        draft_schema_view
        llama
        llama-common
)

target_compile_definitions(semantic_database PUBLIC SQLITE_ENABLE_PREUPDATE_HOOK SQLITE_ENABLE_SESSION SQLITE_ENABLE_COLUMN_METADATA)
