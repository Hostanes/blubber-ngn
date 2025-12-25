
# CMakeLists.txt
cmake_minimum_required(VERSION 3.16)
project(MechArenaDemo C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ------------------- Raylib -------------------
find_package(raylib 4.0 QUIET)

# you can also specify a path manually
# set(RAYLIB_INCLUDE_DIR "/path/to/raylib/include")
# set(RAYLIB_LIBRARY "/path/to/raylib/libraylib.a")

# ------------------- Source Files -------------------
set(SOURCES
    src/main.c
    src/game.c
    src/sound.c
    src/engine.c
    src/engine_components.c
    src/systems/systems.c
    src/systems/collision_system.c
    src/systems/projectile_system.c
    src/systems/render_system.c
    src/systems/game_updates.c
    src/systems/raycast_system.c
    src/systems/raycast_system.c
    src/systems/physics_system.c
    src/systems/menu_system.c
    src/systems/player_system.c
    src/systems/particle_system.c
    src/systems/death_system.c
    src/systems/ai_system.c
    src/systems/banner_system.c
)


# ------------------- Executable -------------------
add_executable(MechArenaDemo ${SOURCES})

# ------------------- Include Directories -------------------
target_include_directories(MechArenaDemo PRIVATE src)

# ------------------- Link Libraries -------------------
if(raylib_FOUND)
    target_link_libraries(MechArenaDemo PRIVATE raylib)
else()
    message(FATAL_ERROR "raylib not found! Please install it or set RAYLIB_INCLUDE_DIR / RAYLIB_LIBRARY.")
endif()

# ------------------- Platform-specific options -------------------
if (UNIX)
    target_link_libraries(MechArenaDemo PRIVATE m pthread dl)
endif()

# ------------------- Output -------------------
set_target_properties(MechArenaDemo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)
