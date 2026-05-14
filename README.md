
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

---

# Benchmark C: Entity Churn

Tests how many entity create+destroy pairs fit inside a single 16.67 ms frame budget (60 fps).

## Test Design

A pool of **100,000 live entities** (Position + Velocity, both inline) is maintained throughout the run.
Each iteration destroys K entities and immediately creates K new ones — net-zero change to the pool,
so archetype capacity stays stable and there is no realloc pressure.
This mirrors real game workloads like bullets and particles that spawn and die every frame.

**Per K value:** 200 warmup iterations, then 1000 measured iterations.

## System

```
CPU:   AMD Ryzen 7 PRO 5850U
       3.4 GHz (capped)
       8 cores used

Cache: L1d  256 KiB (8 instances)
       L1i  256 KiB (8 instances)
       L2     4 MiB (8 instances)
       L3    16 MiB (1 instance)
```

## Results

| K (churn/frame) | Mean ms | StdDev | Min ms | Max ms | Fits 60 fps? |
|----------------:|--------:|-------:|-------:|-------:|:------------:|
|             100 |   0.011 |  0.011 |  0.008 |  0.111 | YES          |
|             500 |   0.040 |  0.017 |  0.034 |  0.158 | YES          |
|           1,000 |   0.075 |  0.025 |  0.066 |  0.194 | YES          |
|           5,000 |   0.366 |  0.096 |  0.323 |  1.053 | YES          |
|          10,000 |   0.737 |  0.188 |  0.647 |  1.920 | YES          |
|          50,000 |   3.451 |  0.732 |  3.043 |  7.371 | YES          |

All tested churn rates fit comfortably within the 16.67 ms budget.

Scaling linearly from the 50k result, the estimated break-even point is around **~240,000 entities
churned per frame** before the budget is exceeded — with a 2-component archetype.

## Notes

- Churn cost scales roughly linearly with K (0.075 ms at 1k → 3.45 ms at 50k, ~46x for 50x entities).
- The slight super-linear growth at higher K is likely swap-remove pressure: deleting entities
  requires updating `entityLocations` for the entity swapped in from the tail, and at large K
  that table starts thrashing cache.
- These numbers reflect a **minimal 2-component archetype**. Archetypes with more inline columns
  will be slower per delete because each swap-remove touches more component arrays.
