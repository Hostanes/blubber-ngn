cmake_minimum_required(VERSION 3.16)
project(Game C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)


# ------------------- OpenMP-------------------
find_package(OpenMP)

# ------------------- Raylib -------------------
find_package(raylib 4.0 REQUIRED)

# ------------------- Source Files -------------------
file(GLOB_RECURSE SOURCES
    src/engine/*.c
    src/game/*.c
)

# ------------------- Executable -------------------
add_executable(Game ${SOURCES})

# ------------------- Include Directories -------------------
target_include_directories(Game PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

target_compile_options(Game PRIVATE
    -O3
    -march=native
    -ffast-math
)

# ------------------- Link Libraries -------------------
target_link_libraries(Game PRIVATE raylib)

if (OpenMP_C_FOUND)
    target_link_libraries(Game PRIVATE OpenMP::OpenMP_C)
endif()


# ------------------- Platform-specific options -------------------
if (UNIX)
    target_link_libraries(Game PRIVATE m pthread dl)
endif()

# ------------------- Output -------------------
set_target_properties(Game PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)
