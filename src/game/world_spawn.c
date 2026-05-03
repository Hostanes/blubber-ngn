#include "world_spawn.h"
#include "../engine/util/json_reader.h"
#include "archetype_loader.h"
#include "level_creater_helper.h"
#include <stdio.h>
#include <string.h>

#define MAX_BULLETS 2048

// --- ARCHETYPE REGISTRATION ---
// Call once after EngineInit. Archetypes persist through WorldClear.
void RegisterAllArchetypes(Engine *engine, GameWorld *gw, world_t *world) {
  const ComponentRegistry *reg = &engine->componentRegistry;

  gw->playerArchId      = ArchetypeLoader_FromFile(world, reg, "assets/entities/player.json");
  gw->enemyGruntArchId  = ArchetypeLoader_FromFile(world, reg, "assets/entities/grunt.json");
  gw->enemyRangerArchId = ArchetypeLoader_FromFile(world, reg, "assets/entities/ranger.json");
  gw->obstacleArchId    = ArchetypeLoader_FromFile(world, reg, "assets/entities/box.json");
  gw->levelModelArchId  = ArchetypeLoader_FromFile(world, reg, "assets/entities/level_model.json");
  gw->tutorialBoxArchId = ArchetypeLoader_FromFile(world, reg, "assets/entities/trigger.json");
  gw->missileArchId     = ArchetypeLoader_FromFile(world, reg, "assets/entities/homing_missile.json");

  // Wall segment archetype
  {
    uint32_t bits[] = {COMP_POSITION, COMP_COLLISION_INSTANCE,
                       COMP_WALL_SEGMENT_COLLIDER, COMP_ACTIVE};
    bitset_t mask = MakeMask(bits, 4);
    gw->wallSegArchId = WorldCreateArchetype(world, &mask);
    archetype_t *arch = WorldGetArchetype(world, gw->wallSegArchId);
    ArchetypeAddInline(arch, COMP_POSITION,             sizeof(Position));
    ArchetypeAddInline(arch, COMP_COLLISION_INSTANCE,   sizeof(CollisionInstance));
    ArchetypeAddInline(arch, COMP_WALL_SEGMENT_COLLIDER, sizeof(WallSegmentCollider));
    ArchetypeAddInline(arch, COMP_ACTIVE,               sizeof(Active));
  }

  // Spawner archetype (position + type + active; no model, no collision)
  {
    uint32_t bits[] = {COMP_POSITION, COMP_ENEMY_SPAWNER, COMP_ACTIVE};
    bitset_t mask = MakeMask(bits, 3);
    gw->spawnerArchId = WorldCreateArchetype(world, &mask);
    archetype_t *arch = WorldGetArchetype(world, gw->spawnerArchId);
    ArchetypeAddInline(arch, COMP_POSITION,      sizeof(Position));
    ArchetypeAddInline(arch, COMP_ENEMY_SPAWNER, sizeof(EnemySpawner));
    ArchetypeAddInline(arch, COMP_ACTIVE,        sizeof(Active));
  }

  // Bullet archetype
  {
    uint32_t bits[] = {
        COMP_POSITION, COMP_VELOCITY,    COMP_ORIENTATION,
        COMP_MODEL,    COMP_BULLETTYPE,  COMP_TIMER,
        COMP_ACTIVE,   COMP_SPHERE_COLLIDER, COMP_COLLISION_INSTANCE,
        COMP_BULLET_OWNER};
    bitset_t mask = MakeMask(bits, 10);
    gw->bulletArchId = WorldCreateArchetype(world, &mask);
    archetype_t *arch = WorldGetArchetype(world, gw->bulletArchId);
    ArchetypeAddInline(arch, COMP_POSITION,           sizeof(Position));
    ArchetypeAddInline(arch, COMP_VELOCITY,           sizeof(Velocity));
    ArchetypeAddInline(arch, COMP_ORIENTATION,        sizeof(Orientation));
    ArchetypeAddInline(arch, COMP_BULLETTYPE,         sizeof(BulletType));
    ArchetypeAddInline(arch, COMP_ACTIVE,             sizeof(Active));
    ArchetypeAddInline(arch, COMP_SPHERE_COLLIDER,    sizeof(SphereCollider));
    ArchetypeAddInline(arch, COMP_COLLISION_INSTANCE, sizeof(CollisionInstance));
    ArchetypeAddInline(arch, COMP_BULLET_OWNER,       sizeof(BulletOwner));
    ArchetypeAddHandle(arch, COMP_MODEL, &engine->modelPool);
    ArchetypeAddHandle(arch, COMP_TIMER, &engine->timerPool);
  }
}

// --- GLOBAL RESOURCE INIT ---
// Call once at startup. Loads shared models and registers all archetypes.
GameWorld GameWorldCreate(Engine *engine, world_t *world) {
  GameWorld gw = {0};
  gw.gameState  = GAMESTATE_MAINMENU;
  gw.arenaRadius = 175.0;

  gw.terrainModel  = LoadModel("assets/models/terrain-level1.glb");
  gw.ArenaModel175 = LoadModel("assets/models/175-arena-border.glb");
  gw.ruinsModel    = LoadModel("assets/models/ruins.glb");
  gw.obstacleModel = LoadModel("assets/models/obstacle.glb");
  gw.skyBox        = LoadModel("assets/models/skybox.glb");
  gw.gunModel      = LoadModel("assets/models/gun1.glb");
  gw.plasmaGunModel = LoadModel("assets/models/gun2-plasma.glb");
  gw.bulletModel   = LoadModel("assets/models/bullet.glb");
  gw.shadowModel   = LoadModel("assets/models/shadow.glb");
  gw.enemyModel    = LoadModel("assets/models/enemy-target.glb");
  gw.gruntGun      = LoadModel("assets/models/enemies/grunt/grunt-gun.glb");
  gw.gruntLegs     = LoadModel("assets/models/enemies/grunt/grunt-legs.glb");
  gw.gruntTorso    = LoadModel("assets/models/enemies/grunt/grunt-torso.glb");

  RegisterAllArchetypes(engine, &gw, world);

  return gw;
}

// --- BULLET POOL SPAWN ---
static void SpawnBulletPool(world_t *world, GameWorld *gw) {
  archetype_t *arch = WorldGetArchetype(world, gw->bulletArchId);
  bitset_t mask = arch->mask;

  for (int i = 0; i < MAX_BULLETS; i++) {
    entity_t b = WorldCreateEntity(world, &mask);

    ECS_GET(world, b, Active,     COMP_ACTIVE)->value    = false;
    ECS_GET(world, b, BulletType, COMP_BULLETTYPE)->type = 0;
    ECS_GET(world, b, Timer,      COMP_TIMER)->value     = 0.0f;

    ModelCollection_t *mc = ECS_GET(world, b, ModelCollection_t, COMP_MODEL);
    ModelCollectionInit(mc, 1);
    ModelCollectionAdd(mc, (ModelInstance_t){
        .model        = gw->bulletModel,
        .scale        = (Vector3){1, 1, 1},
        .rotationMode = MODEL_ROT_FULL,
        .parentIndex  = -1,
        .isActive     = true,
    });

    ECS_GET(world, b, SphereCollider, COMP_SPHERE_COLLIDER)->radius = 0.25f;

    CollisionInstance *ci = ECS_GET(world, b, CollisionInstance, COMP_COLLISION_INSTANCE);
    ci->type        = COLLIDER_SPHERE;
    ci->layerMask   = 1 << LAYER_BULLET;
    ci->collideMask = (1 << LAYER_ENEMY) | (1 << LAYER_PLAYER) | (1 << LAYER_WORLD);
  }
}

// --- JSON LEVEL LOADING ---

#define MAX_LEVEL_BOXES    1024
#define MAX_LEVEL_SPAWNERS 256

typedef struct { Vector3 pos; Vector3 scale; } LevelBox;
typedef struct { Vector3 pos; int enemyType; } LevelSpawner;

static int LoadBoxesFromJSON(const char *text, LevelBox *boxes, int maxCount) {
  const char *p = strstr(text, "\"boxes\"");
  if (!p) return 0;
  p = strchr(p, '[');
  if (!p) return 0;
  p++;

  int count = 0;
  while (*p && *p != ']' && count < maxCount) {
    while (*p && *p != '{' && *p != ']') p++;
    if (*p != '{') break;

    const char *obj = p;
    while (*p && *p != '}') p++;
    if (!*p) break;
    p++;

    int len = (int)(p - obj);
    if (len >= 512) { count++; continue; }

    char buf[512];
    memcpy(buf, obj, len);
    buf[len] = '\0';

    float x = 0, y = 0, z = 0, sx = 5, sy = 5, sz = 5;
    JsonReadFloat(buf, "x",  &x);
    JsonReadFloat(buf, "y",  &y);
    JsonReadFloat(buf, "z",  &z);
    JsonReadFloat(buf, "sx", &sx);
    JsonReadFloat(buf, "sy", &sy);
    JsonReadFloat(buf, "sz", &sz);

    boxes[count++] = (LevelBox){
        .pos   = {x, y, z},
        .scale = {sx, sy, sz},
    };
  }
  return count;
}

static int LoadSpawnersFromJSON(const char *text, LevelSpawner *spawners, int maxCount) {
  const char *p = strstr(text, "\"spawners\"");
  if (!p) return 0;
  p = strchr(p, '[');
  if (!p) return 0;
  p++;

  int count = 0;
  while (*p && *p != ']' && count < maxCount) {
    while (*p && *p != '{' && *p != ']') p++;
    if (*p != '{') break;
    const char *obj = p;
    while (*p && *p != '}') p++;
    if (!*p) break;
    p++;

    int len = (int)(p - obj);
    if (len >= 256) continue;
    char buf[256];
    memcpy(buf, obj, len);
    buf[len] = '\0';

    float x = 0, y = 0, z = 0, type = 0;
    JsonReadFloat(buf, "x",    &x);
    JsonReadFloat(buf, "y",    &y);
    JsonReadFloat(buf, "z",    &z);
    JsonReadFloat(buf, "type", &type);

    spawners[count++] = (LevelSpawner){.pos = {x, y, z}, .enemyType = (int)type};
  }
  return count;
}

static void SpawnLevelBase(world_t *world, GameWorld *gw) {
  gw->terrainHeightMap =
      HeightMap_FromMesh(gw->terrainModel.meshes[0], MatrixIdentity());
  NavGrid_LoadFromImage(&gw->navGrid, "assets/navmap.png", 2, (Vector3){-180, 0, -180});
  gw->player = SpawnPlayer(world, gw, (Vector3){0, 1.8f, 0});
  SpawnLevelModel(world, gw, gw->ruinsModel,     (Vector3){0,0,0}, (Vector3){0,0,0}, (Vector3){1,1,1});
  SpawnLevelModel(world, gw, gw->skyBox,         (Vector3){0,0,0}, (Vector3){0,0,0}, (Vector3){1,1,1});
  SpawnLevelModel(world, gw, gw->ArenaModel175,  (Vector3){0,0,0}, (Vector3){0,0,0}, (Vector3){1,1,1});
  SpawnBulletPool(world, gw);
}

void SpawnLevelFromFile(world_t *world, GameWorld *gw, const char *path) {
  SpawnLevelBase(world, gw);

  char *text = LoadFileText(path);
  if (!text) {
    printf("SpawnLevelFromFile: could not read %s\n", path);
    return;
  }

  static LevelBox    boxes[MAX_LEVEL_BOXES];
  static LevelSpawner spawners[MAX_LEVEL_SPAWNERS];
  int nBoxes    = LoadBoxesFromJSON(text, boxes, MAX_LEVEL_BOXES);
  int nSpawners = LoadSpawnersFromJSON(text, spawners, MAX_LEVEL_SPAWNERS);
  UnloadFileText(text);

  for (int i = 0; i < nBoxes; i++)
    SpawnBoxModel(world, gw, boxes[i].pos, boxes[i].scale);
  for (int i = 0; i < nSpawners; i++)
    SpawnEnemySpawner(world, gw, spawners[i].pos, spawners[i].enemyType);
}

// --- LEVEL 1 SPAWNER (hardcoded enemies) ---
void SpawnLevel01(world_t *world, GameWorld *gw) {
  SpawnLevelBase(world, gw);

  SpawnEnemyRanger(world, gw,
      (Vector3){2, HeightMap_GetHeightCatmullRom(&gw->terrainHeightMap, 2, 50), 50});
  SpawnEnemyGrunt(world, gw, (Vector3){2,  0, 23});
  SpawnEnemyGrunt(world, gw, (Vector3){35, 0, 16});
  SpawnEnemyGrunt(world, gw, (Vector3){25, 0, 10});
  SpawnEnemyGrunt(world, gw, (Vector3){12, 0, 15});
  SpawnEnemyGrunt(world, gw, (Vector3){12, 0, 26});

  // TODO: remove — temporary diagonal test wall
  SpawnWallSegment(world, gw, (Vector3){10, 0, 5},
                   (Vector3){0, 0, 0}, (Vector3){10, 0, 10},
                   0.0f, 4.0f, 0.5f);

  SpawnBoxModel(world, gw, (Vector3){0.72,    1.35,  65.223}, (Vector3){5.47, 5.47, 5.47});
  SpawnBoxModel(world, gw, (Vector3){14.533,  1.36,  78.97},  (Vector3){5.47, 5.47, 5.47});
  SpawnBoxModel(world, gw, (Vector3){-21,     1.35,  89.546}, (Vector3){5.47, 5.47, 5.47});
  SpawnBoxModel(world, gw, (Vector3){8.35,    2.4,   106.6},  (Vector3){6.55, 6.55, 6.55});
  SpawnBoxModel(world, gw, (Vector3){128.92,  20.071, -39.071}, (Vector3){11.6, 11.6, 11.6});
  SpawnBoxModel(world, gw, (Vector3){-108.29, 25.861, 81.147}, (Vector3){11.6, 11.6, 11.6});
  SpawnBoxModel(world, gw, (Vector3){-114.36, 20.509, -32.1},  (Vector3){11.6, 11.6, 11.6});
  SpawnBoxModel(world, gw, (Vector3){-0.581,  1.35,  -20.62},  (Vector3){4.85, 4.85, 4.85});
  SpawnBoxModel(world, gw, (Vector3){93.25,   14.5,  -74.4},   (Vector3){11,   4,    11});
  SpawnBoxModel(world, gw, (Vector3){73.775,  20.752, -84.136},(Vector3){11,   4,    11});
  SpawnBoxModel(world, gw, (Vector3){107.62,  22.3,  -80},     (Vector3){11,   4,    11});
  SpawnBoxModel(world, gw, (Vector3){105.39,  27,    -95.661}, (Vector3){11,   4,    11});
}

void SpawnLevel02(world_t *world, GameWorld *gw) { SpawnLevelBase(world, gw); }
void SpawnLevel03(world_t *world, GameWorld *gw) { SpawnLevelBase(world, gw); }
void SpawnLevel04(world_t *world, GameWorld *gw) { SpawnLevelBase(world, gw); }
