
# force static instead of shared libs
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

add_subdirectory("ggml")
add_subdirectory("json")
add_subdirectory("json-schema-validator")
add_subdirectory("llama.cpp")
add_subdirectory("SQLiteCpp")

# Include the isolated sqlite-vec builder
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/sqlite-vec.cmake")
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/GoogleTest.cmake")
