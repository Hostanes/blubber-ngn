
```
  A mech fps shooter written in C and using raylib for rendering.
  Built to showcase data oriented design in a standard gaming context
```


# Blubber ngn

Currently the engine and demo game are intertwined during development, refactoring is on the todo list

## Installation

### Itch io download

the full compiled demo can be downloaded from this itchio page for windows

https://srcr-hostanes.itch.io/mechwarriorarena-demo

### Build from source

NOTE: you will need the assets folder from the itch.io page download

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
