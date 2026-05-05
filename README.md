
```
  A mech fps shooter written in C and using raylib for rendering.
  Built to showcase data oriented design in a standard gaming context
```

# Blubber ngn

A work in progress game engine and demonstration FPS game. Built to test the effectiveness of data oriented style programming in a gaming context.
The focus of this work is the Entity Component System architecture. Which prioritizes contiguous memory arrays and predictable iterations.

## Installation

Prerequisites:

- GCC
- CMake 3.16+
- Raylib 4.0+ installed on your system


```Bash
git clone https://github.com/Hostanes/blubber-ngn.git
cd blubber-ngn

mkdir build
cd build
```

configure using Cmake

```Bash
cmake ..
```

if raylib isnt found automatically you can specify its path manually:

```Bash
cmake -DRAYLIB_INCLUDE_DIR=/path/to/raylib/include \
      -DRAYLIB_LIBRARY=/path/to/libraylib.a ..
```

```Bash
cmake --build .
```

the executable will be in the bin/ directory
