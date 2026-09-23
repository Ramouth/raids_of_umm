cmake_minimum_required(VERSION 3.22)
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
project(umm_godot_combat LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(FetchContent)

# Pin bindings to Godot 4.5's stable API, also supported by the 4.7 runtime.
set(GODOTCPP_BUILD_PROFILE "${CMAKE_CURRENT_SOURCE_DIR}/build_profile.json" CACHE FILEPATH "" FORCE)
set(GODOTCPP_TARGET template_debug CACHE STRING "" FORCE)
set(GODOTCPP_DISABLE_EXCEPTIONS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(godot_cpp
    URL https://codeload.github.com/godotengine/godot-cpp/tar.gz/e83fd0904c13356ed1d4c3d09f8bb9132bdc6b77
    URL_HASH SHA256=d4c03daf46c8bef4544614182b0762c3e32789151c7f8f44e8eb2053d5cddeba)
FetchContent_Declare(json
    URL https://codeload.github.com/nlohmann/json/tar.gz/refs/tags/v3.11.3
    URL_HASH SHA256=0d8ef5af7f9794e3263480193c491549b2ba6cc74bb018906202ada498a79406)
FetchContent_MakeAvailable(godot_cpp json)

get_filename_component(UMM_ROOT "../.." ABSOLUTE)
add_library(umm_combat_core STATIC
    "${UMM_ROOT}/src/combat/CombatEngine.cpp"
    "${UMM_ROOT}/src/combat/CombatAI.cpp"
    "${UMM_ROOT}/src/core/ResourceManager.cpp"
    "${UMM_ROOT}/src/core/TurnManager.cpp"
    "${UMM_ROOT}/src/hero/Hero.cpp"
    "${UMM_ROOT}/src/hex/HexGrid.cpp"
    "${UMM_ROOT}/src/world/WorldMap.cpp"
    "${UMM_ROOT}/src/adventure/AdventureSession.cpp"
    "${UMM_ROOT}/src/adventure/Scenario.cpp"
    "${UMM_ROOT}/src/adventure/Rivals.cpp"
    "${UMM_ROOT}/src/adventure/Specials.cpp"
    combat_session.cpp
    adventure_bridge.cpp)
target_include_directories(umm_combat_core PUBLIC "${UMM_ROOT}/src" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(umm_combat_core PUBLIC nlohmann_json::nlohmann_json)
target_compile_options(umm_combat_core PRIVATE -Wall -Wextra -Wpedantic)

add_library(umm_combat SHARED register_types.cpp)
target_link_libraries(umm_combat PRIVATE umm_combat_core godot::cpp)
set_target_properties(umm_combat PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../bin")

enable_testing()
add_executable(combat_bridge_tests tests.cpp)
target_link_libraries(combat_bridge_tests PRIVATE umm_combat_core)
add_test(NAME combat_bridge COMMAND combat_bridge_tests "${UMM_ROOT}/data")
add_executable(combat_regression_tests
    "${UMM_ROOT}/tests/main.cpp" "${UMM_ROOT}/tests/test_combat.cpp")
target_include_directories(combat_regression_tests PRIVATE "${UMM_ROOT}/tests")
target_compile_definitions(combat_regression_tests PRIVATE COMBAT_ENGINE_IMPL)
target_link_libraries(combat_regression_tests PRIVATE umm_combat_core)
add_test(NAME combat_regression COMMAND combat_regression_tests)

add_executable(battle_sim battle_sim.cpp)
target_link_libraries(battle_sim PRIVATE umm_combat_core)
