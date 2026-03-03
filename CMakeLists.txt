cmake_minimum_required(VERSION 3.16)
project(raids_of_umm CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Dependency paths (locally extracted packages) ────────────────────────────
set(LOCAL_DEPS "${CMAKE_SOURCE_DIR}/local_deps/usr")

# SDL2 — use system shared library (libSDL2-2.0.so.0 is already installed)
#         but dev headers come from local_deps
set(SDL2_LIBRARY "/usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0")
find_path(SDL2_INCLUDE_DIR NAMES SDL2/SDL.h HINTS "${LOCAL_DEPS}/include" REQUIRED)

# GLM (header-only)
find_path(GLM_INCLUDE_DIR NAMES glm/glm.hpp HINTS "${LOCAL_DEPS}/include" REQUIRED)

# nlohmann/json (header-only)
find_path(NLOHMANN_INCLUDE_DIR NAMES nlohmann/json.hpp HINTS "${LOCAL_DEPS}/include" REQUIRED)

# OpenGL — point to system libs (GLX lives in system /usr/lib, not local_deps)
set(GL_LIB  "/usr/lib/x86_64-linux-gnu/libGL.so.1")
set(GLX_LIB "/usr/lib/x86_64-linux-gnu/libGLX.so.0")

# ── GLAD (bundled) ────────────────────────────────────────────────────────────
add_library(glad STATIC
    third_party/glad/src/glad.c
)
target_include_directories(glad PUBLIC third_party/glad/include)

# ── Source files ──────────────────────────────────────────────────────────────
set(SOURCES
    src/main.cpp
    src/core/Application.cpp
    src/core/StateMachine.cpp
    src/hex/HexGrid.cpp
    src/render/Shader.cpp
    src/render/HexRenderer.cpp
    src/render/Texture.cpp
    src/render/SpriteRenderer.cpp
    src/render/Camera2D.cpp
    src/world/WorldMap.cpp
    src/adventure/AdventureState.cpp
    src/castle/CastleState.cpp
    src/combat/CombatState.cpp
    src/render/HUDRenderer.cpp
    src/worldbuilder/WorldBuilderState.cpp
)

add_executable(raids_of_umm ${SOURCES})

target_include_directories(raids_of_umm PRIVATE
    src/
    "${SDL2_INCLUDE_DIR}"
    "${LOCAL_DEPS}/include/x86_64-linux-gnu"   # SDL2 arch-specific _real_SDL_config.h
    "${GLM_INCLUDE_DIR}"
    "${NLOHMANN_INCLUDE_DIR}"
    third_party/glad/include
)

target_link_libraries(raids_of_umm PRIVATE
    glad
    "${GL_LIB}"
    "${SDL2_LIBRARY}"
    z
    dl
)

# Use SDL2main if present (required on some platforms)
find_library(SDL2MAIN_LIBRARY NAMES SDL2main HINTS "${LOCAL_DEPS}/lib/x86_64-linux-gnu")
if(SDL2MAIN_LIBRARY)
    target_link_libraries(raids_of_umm PRIVATE "${SDL2MAIN_LIBRARY}")
endif()

# ── Compiler warnings ─────────────────────────────────────────────────────────
target_compile_options(raids_of_umm PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O2 -DNDEBUG>
)

# ── Unit tests (no SDL / no OpenGL — pure logic only) ────────────────────────
# Run with: ./build/raids_tests
#
# Stub test files (test_worldmap.cpp, test_camera.cpp) compile but contain no
# active suites until their modules exist and the WORLDMAP_IMPL / CAMERA2D_IMPL
# defines are added here.
add_executable(raids_tests
    tests/main.cpp
    tests/test_hex.cpp
    tests/test_resources.cpp
    tests/test_worldmap.cpp
    tests/test_camera.cpp
    src/hex/HexGrid.cpp
    src/world/WorldMap.cpp
    src/render/Camera2D.cpp
)

target_include_directories(raids_tests PRIVATE
    src/
    tests/
    "${GLM_INCLUDE_DIR}"
    "${NLOHMANN_INCLUDE_DIR}"
)

target_compile_definitions(raids_tests PRIVATE
    WORLDMAP_IMPL               # WorldMap is built — activate its test suites
    CAMERA2D_IMPL               # Camera2D is built — activate its test suites
)

target_compile_options(raids_tests PRIVATE
    -Wall -Wextra -Wpedantic
    $<$<CONFIG:Debug>:-g -O0>
    $<$<CONFIG:Release>:-O2 -DNDEBUG>
)

# ── Copy assets next to executable ───────────────────────────────────────────
add_custom_command(TARGET raids_of_umm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:raids_of_umm>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/data" "$<TARGET_FILE_DIR:raids_of_umm>/data"
    COMMENT "Copying assets and data to build directory"
)
