# blubber-ngn

A game ECS engine written in C using raylib. The project is a work in progress.

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build && ./bin/Game
```

CMake uses `GLOB_RECURSE` on `src/engine/*.c` and `src/game/*.c` — new `.c` files are picked up automatically, but adding new files requires re-running `cmake -B build` to re-scan.

## Project layout

```
src/
  engine/
    ecs/          # Core ECS: world, archetype, entity, component pool, registry
    math/         # Heightmap, collision shapes (AABB, capsule, sphere)
    memory/       # Allocator, arena, pool headers
    util/         # Bitset, json_reader, debug, id
  game/
    components/   # All component struct definitions + components.h with COMP_* enum
    systems/      # Per-system .c files (render, movement, physics, AI, bullet, timer...)
    nav_grid/     # Nav grid + A* pathing
    game.h                      # Engine and GameWorld structs, shared game-wide includes
    engine_init.c               # EngineInit / EngineShutdown
    main.c                      # Entry point — calls SetupComponentRegistry here (see below)
    world_spawn.c               # GameWorldCreate, level loading, bullet pool pre-spawn
    level_creater_helper.c      # Spawn functions only (SpawnEnemy*/SpawnPlayer/etc.)
    component_registry_setup.c  # Registers every component into the ComponentRegistry
    archetype_loader.c          # Loads archetype definitions from JSON files
assets/
  entities/   # JSON archetype definitions (grunt.json, player.json, etc.)
```

## ECS overview

### Entity
`entity_t` is a generational handle: `{id, generation, type}`. `EntityIsAlive` checks that `generations[id] == entity.generation`. Destroyed entities have their generation bumped, invalidating old handles.

### Component pool (`component.c`)
A sparse set over `uint32_t` handles. `sparse[handle]` → dense index → `denseData[index]`. Supports `Create`, `Get`, `Remove` (swap-and-pop), `Has`.

### Archetype (`archetype.c`)
Groups entities that share the exact same component bitmask. Stores data in Structure-of-Arrays columns. Two column storage types:

- **Inline** (`ArchetypeStorageInline`): component struct lives directly in the column buffer. Used for hot, small, frequently iterated components (Position, Velocity, Health, etc.).
- **Handle** (`ArchetypeStorageHandle`): column stores `uint32_t` handles; actual data lives in a shared `componentPool_t`. Used for large or pooled components (ModelCollection_t, Timer). Allows multiple archetypes to share one pool.

Entity removal is swap-and-pop in both the archetype row array and (for handle columns) the component pool.

### World (`world.c`)
Owns the entity manager, archetype array, and `entityLocations[]` table (indexed by `entity.id`, stores `{archetype_index, row_index}`). `WorldGetComponent` resolves: entity → location → archetype column → pointer (direct for inline, pool lookup for handle).

### Component registry (`component_registry.c`)
A flat array acting as a phone book: name string → `{id, size, storage, pool}`.

The registry is the bridge between a string in a JSON file and the C type information the ECS needs. When the archetype loader reads `"Health"` from a file, it looks up `"Health"` in the registry and gets back `{id=COMP_HEALTH, size=sizeof(Health), storage=inline}` — enough to set the right bit in the bitmask and call `ArchetypeAddInline` with the correct size. Without the registry, the JSON loader would need hardcoded C knowledge of every type.

Key functions:
- `ComponentRegistry_Find(reg, "Health")` → `ComponentDef*`
- `ComponentRegistry_AddToArchetype(reg, arch, "Health")` → calls `ArchetypeAddInline` or `ArchetypeAddHandle` as appropriate; skips tag components (size=0)

Populated once at startup by `SetupComponentRegistry` in `component_registry_setup.c`.

### CRITICAL: registry pointer lifetime
The registry stores raw pointers to the pool fields inside the `Engine` struct (`&engine->timerPool`, `&engine->modelPool`). These pointers are only valid as long as `Engine` doesn't move in memory.

**`SetupComponentRegistry` must be called in `main` after `EngineInit` returns**, not inside `EngineInit`. Calling it inside `EngineInit` would capture `&engine.timerPool` where `engine` is a local — `return engine` then copies the struct to a new address, leaving all pool pointers dangling. This caused a segfault in `realloc` the first time around.

```c
// main.c — correct order:
Engine engine = EngineInit();
SetupComponentRegistry(&engine.componentRegistry, &engine); // engine is now stable
GameWorld game = GameWorldCreate(&engine, engine.world);
```

### Tag components
`COMP_GRAVITY`, `COMP_TYPE_PLAYER`, `COMP_TYPE_GRUNT`, `COMP_TYPE_RANGER` are mask-only tags — they appear in the archetype bitmask for system filtering but have no column data (`size=0`, no `ArchetypeAddInline` call).

### Archetype loader (`archetype_loader.c`)
`ArchetypeLoader_FromFile(world, reg, "assets/entities/grunt.json")`:
1. Reads the `"components"` string array from the JSON file
2. Looks up each name in the registry to get the component ID
3. Builds the bitmask with those IDs
4. Calls `WorldCreateArchetype`
5. Calls `ComponentRegistry_AddToArchetype` for each component to add columns

`world_spawn.c` now calls this for every archetype instead of hardcoded `Register*` functions.

### JSON reader (`src/engine/util/json_reader.c`)
Minimal parser for flat JSON objects (no nested objects, no numbers). Scans for `"key":` by treating every quoted string as a potential key and checking whether `:` immediately follows — value strings don't, so they're skipped naturally. Two public functions: `JsonReadString`, `JsonReadStringArray`.

## Component IDs (`components.h`)

All IDs are in the `enum` in `src/game/components/components.h`. The string names in `component_registry_setup.c` are the canonical keys. When adding a new component: add it to the enum, define its struct in `components/`, register it in `component_registry_setup.c`, and add it to the relevant JSON file(s) in `assets/entities/`.

## Shared pools

`Engine` holds two component pools used as handle targets:
- `timerPool` — all Timer components (fire timers, move timers, dash timers, etc.)
- `modelPool` — all ModelCollection_t components

Both are initialized inside `EngineInit` before the registry is populated.

## Terrain snapping

`HeightMap_GetHeightCatmullRom(&game->terrainHeightMap, x, z)` returns the terrain Y at a world XZ position. Ground enemy spawn functions (`SpawnEnemyGrunt`, `SpawnEnemyRanger`, `SpawnEnemyMissile`) snap `position.y` to the terrain immediately after `WorldCreateEntity`, before setting any component values.

## Roadmap / in-progress

- **Level layout from JSON**: entity placements (`{template, position, yaw}`) loaded from `assets/levels/level1.json` instead of hardcoded spawn calls in `world_spawn.c`.
- **Nav grid from image**: load nav cell types from a greyscale PNG (`NavGrid_FromPNG`) so the grid can be painted externally and eventually exported from a 3D editor.

## Coding conventions

- No comments unless the WHY is non-obvious.
- No trailing summaries after edits.
- Prefer editing existing files over creating new ones.
- All new `.c` files under `src/` are auto-included by CMake, but `cmake -B build` must be re-run after adding files.
