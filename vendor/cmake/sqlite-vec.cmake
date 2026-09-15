include(FetchContent)

# 1. Fetch official pre-generated amalgamation
FetchContent_Declare(
    sqlite_vec_amalgamation
    URL "https://github.com/asg017/sqlite-vec/releases/download/v0.1.8/sqlite-vec-0.1.8-amalgamation.tar.gz"
)

FetchContent_MakeAvailable(sqlite_vec_amalgamation)

# 2. Build the static extension
add_library(sqlite-vec STATIC
    "${sqlite_vec_amalgamation_SOURCE_DIR}/sqlite-vec.c"
)

target_compile_definitions(sqlite-vec
    PUBLIC
        SQLITE_CORE
        SQLITE_VEC_STATIC
)

target_include_directories(sqlite-vec
    PUBLIC
        "${sqlite_vec_amalgamation_SOURCE_DIR}"
)

# 3. Platform & link dependencies
include(CheckLibraryExists)
check_library_exists(m sqrt "" HAVE_LIB_M)
if(HAVE_LIB_M)
    target_link_libraries(sqlite-vec PRIVATE m)
endif()

target_link_libraries(sqlite-vec PUBLIC sqlite3)