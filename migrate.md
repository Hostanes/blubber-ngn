# Migration Plan

## Architecture Goals
- Engine core: stable binary, never changes for game work
- Game systems: hot-reloadable shared library (.so / .dll)
- All persistent state lives in ECS components, never inside systems
- Components and archetype compositions defined in external schema files (custom format)

---

## Scene & System Model

### Scene tree
- World has a root entity; all other entities are children in a tree
- Tree is purely data — a `Parent` component (entity ID) and a `Children` component (list of entity IDs)
- Tree handles ownership and lifecycle only — destroying a parent destroys its children
- Actual component data still lives in flat archetype arrays; the tree does not fragment memory layout

### Two-tier system dispatch

**Global systems — run at root, flat, every frame**
Process all entities of a type in one tight loop. Performance-critical. Never tree-traversed.
Examples from current game: `MovementSystem`, `PhysicsSystem`, `RenderSystem`, `BulletSystem`, `CollisionSystem`, `WaveSystem`

**Event/lifecycle handlers — attached per subtree node, fire on demand**
Run in response to something happening, not every frame. Wired up via schema, not hardcoded.
Examples from current game: `OnDeath` (drop loot, update wave count), `OnCollision` (apply damage, spawn particle), info box trigger, wave spawn

### Practical win over current code
Right now `level_creater_helper.c` spawn functions manually assemble every component, set every field, and assign function pointers. In the new model, spawning an entity means: instantiate archetype from schema, attach to parent node. Components, handlers, and systems are all declared in the schema — the spawn call becomes 2-3 lines.

---

## Todo

### 1. C → C++ Migration
- [ ] Rename engine files to `.cpp`, switch CMake to C++17
- [ ] Replace `malloc`/`free` in ECS pools with typed allocators
- [ ] Replace fixed `char[256]` path fields and `strncpy` with `std::string`
- [ ] Replace fixed-size arrays (`edModelPaths[48][256]` etc.) with `std::vector`
- [ ] Replace `OnDeath.fn` function pointer pattern with `std::function` or virtual interface
- [ ] Replace `ECS_GET` macro with a typed template function
- [ ] Wrap engine code in a namespace

### 2. Custom Schema System
- [ ] Design schema file format for component definitions (field names + types)
- [ ] Design schema file format for archetype definitions (which components they carry)
- [ ] Write schema parser
- [ ] Feed parsed schemas into `ComponentRegistry` at startup instead of hardcoded calls
- [ ] Move archetype assembly out of `level_creater_helper.c` and into schema files

### 3. System Registry
- [ ] Define a `SystemRegistry` that maps string names to system functions
- [ ] Define system priority / execution order (can be declared in schema or registry)
- [ ] Replace the hardcoded system calls in `game_loop.c` with registry dispatch
- [ ] Separate global systems (flat, root-level, every frame) from event/lifecycle handlers (per-node, on-demand)
- [ ] Add `Parent` and `Children` components to ECS for scene tree ownership
- [ ] Implement tree lifecycle propagation — destroying a parent despawns its children

### 4. Hot-Reload Layer
- [ ] Split game systems into a separate shared library target in CMake
- [ ] Define a stable `SystemTable` ABI struct (function pointers the library fills, engine reads)
- [ ] Implement platform file watcher to detect library rebuild (Linux `inotify`, Windows `ReadDirectoryChangesW`)
- [ ] Implement `dlopen`/`dlclose` reload cycle on change
- [ ] Abstract platform difference (`dlopen` vs `LoadLibrary`) behind a thin platform layer

### 5. Built-in vs Game Systems
- [ ] Identify which current systems become engine built-ins (movement, collision, rendering, physics)
- [ ] Identify which become game-layer hot-reloadable systems (AI, player control, combat, game rules)
- [ ] Ensure built-ins run before game systems via priority ordering
- [ ] Verify game systems can override built-in behaviour by registering higher-priority handlers

### 6. State Audit
- [ ] Audit `GameWorld` fields — move per-entity state into ECS components
- [ ] Ensure no persistent state lives inside systems (no static locals that need to survive reload)
