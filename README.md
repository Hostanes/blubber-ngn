
```
  A mech fps shooter written in C and using raylib for rendering.
  Built to showcase data oriented design in a standard gaming context
```

# TODO list
- [x] Implement custom 3d models
- [x] Movement
	- [x] Independent leg and torso
	- [x] head bobbing 
	- [ ] Dashing
	- [x] Sound effects 
	- [ ] Animations
- [x] Ground heightmap and collisions
- [x] Primitive shape collisions
- [x] Weapon shooting and shootable targets
- [x] Enemies
	- [x] Turrets
	- [ ] Tanks
	- [ ] Raptors
	- [ ] Bosses
- [x] Textures
- [ ] Outlines using inverse hull method
- [x] Custom UI
- [ ] Music


# Blubber ngn

Currently the engine and demo game are intertwined during development, refactoring is on the todo list

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
