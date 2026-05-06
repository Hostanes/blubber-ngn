#include "world_spawn.h"
#include "../engine/util/json_reader.h"
#include "archetype_loader.h"
#include "level_creater_helper.h"
#include <stdio.h>
#include <string.h>

#define MAX_BULLETS   2048
#define MAX_PARTICLES  128

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
  gw->enemyMeleeArchId  = ArchetypeLoader_FromFile(world, reg, "assets/entities/melee.json");

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

  // Particle archetype (pooled, rendered as primitive spheres)
  {
    uint32_t bits[] = {COMP_ACTIVE, COMP_POSITION, COMP_VELOCITY, COMP_PARTICLE};
    bitset_t mask = MakeMask(bits, 4);
    gw->particleArchId = WorldCreateArchetype(world, &mask);
    archetype_t *arch  = WorldGetArchetype(world, gw->particleArchId);
    ArchetypeAddInline(arch, COMP_ACTIVE,   sizeof(Active));
    ArchetypeAddInline(arch, COMP_POSITION, sizeof(Position));
    ArchetypeAddInline(arch, COMP_VELOCITY, sizeof(Velocity));
    ArchetypeAddInline(arch, COMP_PARTICLE, sizeof(Particle));
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

  // InfoBox archetype (position + trigger + active)
  {
    uint32_t bits[] = {COMP_POSITION, COMP_INFOBOX, COMP_ACTIVE};
    bitset_t mask = MakeMask(bits, 3);
    gw->infoBoxArchId = WorldCreateArchetype(world, &mask);
    archetype_t *arch = WorldGetArchetype(world, gw->infoBoxArchId);
    ArchetypeAddInline(arch, COMP_POSITION, sizeof(Position));
    ArchetypeAddInline(arch, COMP_INFOBOX,  sizeof(InfoBox));
    ArchetypeAddInline(arch, COMP_ACTIVE,   sizeof(Active));
  }

  // Drone enemy archetype
  {
    uint32_t bits[] = {
      COMP_ACTIVE, COMP_POSITION, COMP_VELOCITY, COMP_ORIENTATION,
      COMP_MODEL, COMP_HEALTH, COMP_SHIELD, COMP_ONDEATH,
      COMP_SPHERE_COLLIDER, COMP_COLLISION_INSTANCE, COMP_DRONE_ENEMY
    };
    bitset_t mask = MakeMask(bits, 11);
    gw->enemyDroneArchId = WorldCreateArchetype(world, &mask);
    archetype_t *arch = WorldGetArchetype(world, gw->enemyDroneArchId);
    ArchetypeAddInline(arch, COMP_ACTIVE,             sizeof(Active));
    ArchetypeAddInline(arch, COMP_POSITION,           sizeof(Position));
    ArchetypeAddInline(arch, COMP_VELOCITY,           sizeof(Velocity));
    ArchetypeAddInline(arch, COMP_ORIENTATION,        sizeof(Orientation));
    ArchetypeAddInline(arch, COMP_MODEL,              sizeof(ModelCollection_t));
    ArchetypeAddInline(arch, COMP_HEALTH,             sizeof(Health));
    ArchetypeAddInline(arch, COMP_SHIELD,             sizeof(Shield));
    ArchetypeAddInline(arch, COMP_ONDEATH,            sizeof(OnDeath));
    ArchetypeAddInline(arch, COMP_SPHERE_COLLIDER,    sizeof(SphereCollider));
    ArchetypeAddInline(arch, COMP_COLLISION_INSTANCE, sizeof(CollisionInstance));
    ArchetypeAddInline(arch, COMP_DRONE_ENEMY,        sizeof(DroneEnemy));
  }

  // Coolant pickup archetype
  {
    uint32_t bits[] = {COMP_ACTIVE, COMP_POSITION, COMP_VELOCITY, COMP_COOLANT};
    bitset_t mask = MakeMask(bits, 4);
    gw->coolantArchId = WorldCreateArchetype(world, &mask);
    archetype_t *arch = WorldGetArchetype(world, gw->coolantArchId);
    ArchetypeAddInline(arch, COMP_ACTIVE,   sizeof(Active));
    ArchetypeAddInline(arch, COMP_POSITION, sizeof(Position));
    ArchetypeAddInline(arch, COMP_VELOCITY, sizeof(Velocity));
    ArchetypeAddInline(arch, COMP_COOLANT,  sizeof(Coolant));
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

  gw.fov        = 75.0f;
  gw.resWidth   = 1280;
  gw.resHeight  = 720;
  gw.fullscreen = false;

  gw.soundSystem = InitSoundSystem();

  gw.terrainModel  = LoadModel("assets/models/terrain-level1.glb");
  strncpy(gw.terrainModelPath, "assets/models/terrain-level1.glb",
          sizeof(gw.terrainModelPath) - 1);
  gw.obstacleModel      = LoadModel("assets/models/obstacle.glb");
  gw.infoBoxMarkerModel = LoadModel("assets/models/exclamation-mark.glb");
  gw.gunModel           = LoadModel("assets/models/gun1.glb");
  gw.plasmaGunModel     = LoadModel("assets/models/gun2-plasma.glb");
  gw.rocketLauncherModel = LoadModel("assets/models/gun3-rocketlauncher.glb");
  gw.blunderbussModel    = LoadModel("assets/models/gun4-blunderbus.glb");
  gw.bulletModel   = LoadModel("assets/models/bullet.glb");
  gw.shadowModel   = LoadModel("assets/models/shadow.glb");
  gw.enemyModel    = LoadModel("assets/models/enemy-target.glb");
  gw.gruntGun      = LoadModel("assets/models/enemies/grunt/grunt-gun.glb");
  gw.gruntSaw      = LoadModel("assets/models/enemies/grunt/saw.glb");
  gw.gruntLegs     = LoadModel("assets/models/enemies/grunt/grunt-legs.glb");
  gw.gruntTorso    = LoadModel("assets/models/enemies/grunt/grunt-torso.glb");

  gw.outlineShader       = LoadShader("assets/shaders/outline.vs",
                                      "assets/shaders/outline.fs");
  gw.outlineColorLoc     = GetShaderLocation(gw.outlineShader, "outlineColor");
  gw.outlineThicknessLoc = GetShaderLocation(gw.outlineShader, "outlineThickness");

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

// --- PARTICLE POOL SPAWN ---
static void SpawnParticlePool(world_t *world, GameWorld *gw) {
  archetype_t *arch = WorldGetArchetype(world, gw->particleArchId);
  bitset_t mask = arch->mask;
  for (int i = 0; i < MAX_PARTICLES; i++) {
    entity_t e = WorldCreateEntity(world, &mask);
    ECS_GET(world, e, Active, COMP_ACTIVE)->value = false;
  }
}

// --- COOLANT POOL SPAWN ---
#define MAX_COOLANTS 64
static void SpawnCoolantPool(world_t *world, GameWorld *gw) {
  archetype_t *arch = WorldGetArchetype(world, gw->coolantArchId);
  bitset_t mask = arch->mask;
  for (int i = 0; i < MAX_COOLANTS; i++) {
    entity_t e = WorldCreateEntity(world, &mask);
    ECS_GET(world, e, Active, COMP_ACTIVE)->value = false;
  }
}

// --- JSON LEVEL LOADING ---

#define MAX_LEVEL_BOXES      1024
#define MAX_LEVEL_SPAWNERS    256
#define MAX_LEVEL_PROPS       512
#define MAX_LEVEL_INFOBOXES    64
#define MAX_PROP_MODELS         32

typedef struct { Vector3 pos; Vector3 scale; } LevelBox;
typedef struct { Vector3 pos; int enemyType; } LevelSpawner;
typedef struct { Vector3 pos; float yaw; Vector3 scale; char modelPath[256]; } LevelProp;

/* ---- Per-level prop model cache (avoids reloading duplicates) ---- */

static char  s_propModelPaths[MAX_PROP_MODELS][256];
static Model s_propModels[MAX_PROP_MODELS];
static int   s_propModelCount = 0;

static void ClearPropModelCache(void) {
  for (int i = 0; i < s_propModelCount; i++)
    UnloadModel(s_propModels[i]);
  s_propModelCount = 0;
}

static Model GetOrLoadPropModel(const char *path) {
  for (int i = 0; i < s_propModelCount; i++)
    if (strcmp(s_propModelPaths[i], path) == 0) return s_propModels[i];
  if (s_propModelCount < MAX_PROP_MODELS) {
    strncpy(s_propModelPaths[s_propModelCount], path, 255);
    s_propModels[s_propModelCount] = LoadModel(path);
    return s_propModels[s_propModelCount++];
  }
  return s_propModels[0]; // cache full, reuse first
}

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

static int LoadPropsFromJSON(const char *text, LevelProp *props, int maxCount) {
  const char *p = strstr(text, "\"props\"");
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

    float x=0,y=0,z=0,yaw=0,sx=3,sy=3,sz=3;
    char modelPath[256] = "assets/models/obstacle.glb";
    JsonReadFloat(buf,"x",  &x);
    JsonReadFloat(buf,"y",  &y);
    JsonReadFloat(buf,"z",  &z);
    JsonReadFloat(buf,"yaw",&yaw);
    JsonReadFloat(buf,"sx", &sx);
    JsonReadFloat(buf,"sy", &sy);
    JsonReadFloat(buf,"sz", &sz);
    JsonReadString(buf,"model", modelPath, sizeof(modelPath));

    LevelProp *lp = &props[count++];
    lp->pos   = (Vector3){x,y,z};
    lp->yaw   = yaw;
    lp->scale = (Vector3){sx,sy,sz};
    strncpy(lp->modelPath, modelPath, 255);
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

typedef struct { Vector3 pos; float halfExtent; char message[256]; float duration; int triggerCount; float markerHeight; } LevelInfoBox;

static int LoadInfoBoxesFromJSON(const char *text, LevelInfoBox *boxes, int maxCount) {
  const char *p = strstr(text, "\"infoboxes\"");
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
    if (len >= 512) continue;
    char buf[512]; memcpy(buf, obj, len); buf[len] = '\0';
    LevelInfoBox ib = {0};
    ib.halfExtent = 2.5f; ib.duration = 5.0f; ib.triggerCount = 1;
    JsonReadFloat(buf, "x",   &ib.pos.x);
    JsonReadFloat(buf, "y",   &ib.pos.y);
    JsonReadFloat(buf, "z",   &ib.pos.z);
    JsonReadFloat(buf, "ext", &ib.halfExtent);
    JsonReadFloat(buf, "dur", &ib.duration);
    JsonReadString(buf, "msg", ib.message, sizeof(ib.message));
    { float tf = 1.0f; if (JsonReadFloat(buf, "trig", &tf)) ib.triggerCount = (int)tf; }
    JsonReadFloat(buf, "mh", &ib.markerHeight);
    boxes[count++] = ib;
  }
  return count;
}

static void SpawnLevelBase(world_t *world, GameWorld *gw, const char *navmapPath) {
  MessageSystem_Init(&gw->messageSystem);
  gw->terrainHeightMap =
      HeightMap_FromMesh(gw->terrainModel.meshes[0], MatrixIdentity());
  if (!NavGrid_LoadFromImage(&gw->navGrid, navmapPath, 2, (Vector3){-180, 0, -180}))
    NavGrid_Init(&gw->navGrid, 180, 180, 2.0f, (Vector3){-180, 0, -180});
  gw->player = SpawnPlayer(world, gw, (Vector3){0, 1.8f, 0});
  SpawnBulletPool(world, gw);
  SpawnParticlePool(world, gw);
  SpawnCoolantPool(world, gw);
}

// Derive a level-specific navmap path: "assets/levels/foo.json" -> "assets/levels/foo.navmap.png"
static void NavmapPathFromLevel(const char *levelPath, char *out, int maxLen) {
  strncpy(out, levelPath, maxLen - 1);
  out[maxLen - 1] = '\0';
  char *dot = strrchr(out, '.');
  if (dot) strcpy(dot, ".navmap.png");
  else strncat(out, ".navmap.png", maxLen - (int)strlen(out) - 1);
}

void SpawnLevelFromFile(world_t *world, GameWorld *gw, const char *path) {
  char *text = LoadFileText(path);
  if (!text) {
    printf("SpawnLevelFromFile: could not read %s\n", path);
    SpawnLevelBase(world, gw, "");
    return;
  }

  // Terrain model — parse before SpawnLevelBase so the heightmap uses the right mesh
  char terrainPath[256];
  strncpy(terrainPath, gw->terrainModelPath, 255);
  JsonReadString(text, "terrain", terrainPath, sizeof(terrainPath));
  if (strcmp(terrainPath, gw->terrainModelPath) != 0) {
    UnloadModel(gw->terrainModel);
    gw->terrainModel = LoadModel(terrainPath);
    strncpy(gw->terrainModelPath, terrainPath, sizeof(gw->terrainModelPath) - 1);
  }

  // Mission type — default to waves if not specified
  {
    char missionBuf[32] = "waves";
    JsonReadString(text, "mission", missionBuf, sizeof(missionBuf));
    gw->waveState.missionType =
        (strcmp(missionBuf, "exploration") == 0) ? MISSION_EXPLORATION : MISSION_WAVES;
  }

  // Navmap — use "navmap" field if present, otherwise derive from level path
  char navmapPath[256];
  NavmapPathFromLevel(path, navmapPath, sizeof(navmapPath));
  JsonReadString(text, "navmap", navmapPath, sizeof(navmapPath));

  // Player spawn point — parse before SpawnLevelBase call so we can apply after
  bool    hasSpawn = false;
  Vector3 spawnPos = {0, 1.8f, 0};
  {
    const char *sp = strstr(text, "\"spawn\"");
    if (sp) {
      sp = strchr(sp, '{');
      if (sp) {
        char buf[128] = {0};
        const char *end = strchr(sp, '}');
        if (end && (end - sp) < (int)sizeof(buf) - 1) {
          strncpy(buf, sp, (size_t)(end - sp + 1));
          float sx = 0, sy = 1.8f, sz = 0;
          JsonReadFloat(buf, "x", &sx);
          JsonReadFloat(buf, "y", &sy);
          JsonReadFloat(buf, "z", &sz);
          spawnPos = (Vector3){sx, sy, sz};
          hasSpawn = true;
        }
      }
    }
  }

  SpawnLevelBase(world, gw, navmapPath);

  // Override player spawn position if level defines one
  if (hasSpawn) {
    Position *ppos = ECS_GET(world, gw->player, Position, COMP_POSITION);
    if (ppos) ppos->value = spawnPos;
  }

  static LevelBox      boxes[MAX_LEVEL_BOXES];
  static LevelSpawner  spawners[MAX_LEVEL_SPAWNERS];
  static LevelProp     props[MAX_LEVEL_PROPS];
  static LevelInfoBox  infoboxes[MAX_LEVEL_INFOBOXES];
  int nBoxes     = LoadBoxesFromJSON(text,     boxes,     MAX_LEVEL_BOXES);
  int nSpawners  = LoadSpawnersFromJSON(text,  spawners,  MAX_LEVEL_SPAWNERS);
  int nProps     = LoadPropsFromJSON(text,     props,     MAX_LEVEL_PROPS);
  int nInfoBoxes = LoadInfoBoxesFromJSON(text, infoboxes, MAX_LEVEL_INFOBOXES);
  UnloadFileText(text);

  ClearPropModelCache();
  for (int i = 0; i < nBoxes; i++)
    SpawnBoxModel(world, gw, boxes[i].pos, boxes[i].scale);
  for (int i = 0; i < nSpawners; i++)
    SpawnEnemySpawner(world, gw, spawners[i].pos, spawners[i].enemyType);
  bool skyboxSpawned = false;
  for (int i = 0; i < nProps; i++) {
    bool isSkybox = (strstr(props[i].modelPath, "skybox") != NULL);
    if (isSkybox) {
      if (skyboxSpawned) continue;
      skyboxSpawned = true;
    }
    Model m = GetOrLoadPropModel(props[i].modelPath);
    SpawnProp(world, gw, m, props[i].pos, props[i].yaw, props[i].scale);
  }
  for (int i = 0; i < nInfoBoxes; i++)
    SpawnInfoBox(world, gw, infoboxes[i].pos, infoboxes[i].halfExtent,
                 infoboxes[i].message, infoboxes[i].duration,
                 infoboxes[i].triggerCount, infoboxes[i].markerHeight);
}

// --- LEVEL 1 SPAWNER (hardcoded enemies) ---
void SpawnLevel01(world_t *world, GameWorld *gw) {
  SpawnLevelBase(world, gw, "");

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

void SpawnLevel02(world_t *world, GameWorld *gw) { SpawnLevelBase(world, gw, ""); }
void SpawnLevel03(world_t *world, GameWorld *gw) { SpawnLevelBase(world, gw, ""); }
void SpawnLevel04(world_t *world, GameWorld *gw) { SpawnLevelBase(world, gw, ""); }
