cmake_minimum_required(VERSION 3.14...3.28) 

set(SCENE_NARRATOR_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(SCENE_NARRATOR_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")

add_library(scene_narrator STATIC
    "${SCENE_NARRATOR_SOURCE_DIR}/scene_narrator.cpp"
    "${SCENE_NARRATOR_SOURCE_DIR}/scene_narration_buffer.cpp"
    "${SCENE_NARRATOR_SOURCE_DIR}/scene_narration_stream.cpp"
)

target_include_directories(scene_narrator PUBLIC
    "${SCENE_NARRATOR_INCLUDE_DIR}"
)

# Enforce C++20 as a minimum for this target
#target_compile_features(scene_narrator PRIVATE cxx_std_20)

# Add this line right below it to force the Microsoft STL headers to wake up
#target_compile_definitions(scene_narrator PRIVATE _HAS_CXX20=1)


# This automatically handles the include paths and underlying math engines for you.
target_link_libraries(scene_narrator
    PRIVATE 
        llama
        llama-common
)