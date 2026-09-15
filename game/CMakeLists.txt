cmake_minimum_required(VERSION 3.14...3.28) 

set(GAME_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")

add_executable(unnamed_rpg
    "${GAME_SRC_DIR}/main.cpp"
)

# Enforce C++20 as a minimum for this target
#target_compile_features(unnamed_rpg PRIVATE cxx_std_20)

# This automatically handles the include paths and underlying math engines for you.
target_link_libraries(unnamed_rpg 
    PRIVATE 
        llama
        llama-common
        scene_narrator
        semantic_database
        SQLiteCpp
        nlohmann_json_schema_validator::validator
)


target_compile_definitions(unnamed_rpg PUBLIC SQLITE_ENABLE_PREUPDATE_HOOK SQLITE_ENABLE_SESSION)