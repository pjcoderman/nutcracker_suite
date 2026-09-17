cmake_minimum_required(VERSION 3.14...3.28) 

if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
  set(CMAKE_CUDA_ARCHITECTURES 86) # Replace with your target compute capability
endif()

project("nutcracker_suite" C CXX ASM CUDA)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Turn off llama.cpp tests/examples so they don't slow down build times
set(GGML_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(GGML_BUILD_TESTS    OFF CACHE INTERNAL "")
set(LLAMA_BUILD_COMMON  ON  CACHE INTERNAL "")

# Enable CUDA GPU backend
set(GGML_CUDA ON CACHE INTERNAL "")

set(SQLITE_ENABLE_PREUPDATE_HOOK ON CACHE INTERNAL "")
set(SQLITE_ENABLE_SESSION ON CACHE INTERNAL "")
set(SQLITE_ENABLE_COLUMN_METADATA ON CACHE INTERNAL "")

if(WIN32 AND NOT DEFINED OPENSSL_ROOT_DIR)
    set(OPENSSL_ROOT_DIR "C:/Program Files/OpenSSL-Win64" CACHE PATH "Default OpenSSL location for Windows" FORCE)
endif()

set(NLOHMANN_JSON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/json" CACHE PATH "" FORCE)

# Put all binaries in build/bin so executables and shared DLLs live together
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# Mark the entire vendor directory as SYSTEM to suppress all 3rd-party warnings
add_subdirectory("vendor" SYSTEM)
add_subdirectory("modules")
add_subdirectory("game")
add_subdirectory("models")

include("${CMAKE_CURRENT_LIST_DIR}/cmake/coverage.cmake")
