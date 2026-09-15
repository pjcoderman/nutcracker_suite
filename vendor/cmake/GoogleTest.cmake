# vendor/cmake/GoogleTest.cmake
option(BUILD_TESTING "Build the test suite" ON)

if(BUILD_TESTING)
  enable_testing()

  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
  )

  # Prevent GTest from installing files globally
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

  # Modern CMake: FetchContent_MakeAvailable automatically handles the setup.
  # The "SYSTEM" suppression is handled at the subdirectory level in the root CMakeLists.txt
  FetchContent_MakeAvailable(googletest)

  
  foreach(_tgt gtest gtest_main gmock gmock_main)
    if(TARGET ${_tgt})
      # Tell clang-cl / clang to ignore the warning strictly when compiling Google's code
      target_compile_options(${_tgt} PRIVATE 
        $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wno-character-conversion>
        $<$<CXX_COMPILER_ID:MSVC>:/wd4244>
      )
    endif()
  endforeach()

  include(GoogleTest) # Provides gtest_discover_tests()
endif()
