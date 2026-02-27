#include "world_spawn.h"

#define MAX_BULLETS 2048

// --- GLOBAL RESOURCE INIT ---
// Call this only ONCE at start of game
GameWorld GameWorldCreate(Engine *engine, world_t *world) {
  GameWorld gw = {0};
  gw.gameState = GAMESTATE_MAINMENU;
  gw.arenaRadius = 175.0;

  /* 1. Load Shared Models (Persistent) */
  gw.gunModel = LoadModel("assets/models/gun1.glb");
  gw.bulletModel = LoadModel("assets/models/bullet.glb");
  gw.enemyModel = LoadModel("assets/models/enemy-target.glb");
  gw.gruntGun = LoadModel("assets/models/enemies/grunt/grunt-gun.glb");
  gw.gruntLegs = LoadModel("assets/models/enemies/grunt/grunt-legs.glb");
  gw.gruntTorso = LoadModel("assets/models/enemies/grunt/grunt-torso.glb");

  /* 2. Register Archetypes (Persistent) */
  gw.playerArchId = RegisterPlayerArchetype(world, engine);
  gw.enemyGruntArchId = RegisterEnemyArchetype(world, engine);
  gw.obstacleArchId = RegisterBoxArchetype(world, engine);
  gw.levelModelArchId = RegisterLevelModelArchetype(world, engine);
  gw.tutorialBoxArchId = RegisterTriggerArchetype(world, engine);

  // Bullet Archetype setup (Same as before)
  uint32_t bulletBits[] = {
      COMP_POSITION,    COMP_VELOCITY,        COMP_ORIENTATION,
      COMP_MODEL,       COMP_BULLETTYPE,      COMP_TIMER,
      COMP_ACTIVE,      COMP_SPHERE_COLLIDER, COMP_COLLISION_INSTANCE,
      COMP_BULLET_OWNER};
  bitset_t bulletMask = MakeMask(bulletBits, 10);

  gw.bulletArchId = WorldCreateArchetype(world, &bulletMask);

  archetype_t *bulletArch = WorldGetArchetype(world, gw.bulletArchId);

  ArchetypeAddInline(bulletArch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(bulletArch, COMP_VELOCITY, sizeof(Velocity));
  ArchetypeAddInline(bulletArch, COMP_ORIENTATION, sizeof(Orientation));
  ArchetypeAddInline(bulletArch, COMP_BULLETTYPE, sizeof(BulletType));
  ArchetypeAddInline(bulletArch, COMP_ACTIVE, sizeof(Active));
  ArchetypeAddInline(bulletArch, COMP_SPHERE_COLLIDER, sizeof(SphereCollider));
  ArchetypeAddInline(bulletArch, COMP_COLLISION_INSTANCE,
                     sizeof(CollisionInstance));
  ArchetypeAddInline(bulletArch, COMP_BULLET_OWNER, sizeof(BulletOwner));

  ArchetypeAddHandle(bulletArch, COMP_MODEL, &engine->modelPool);
  ArchetypeAddHandle(bulletArch, COMP_TIMER, &engine->timerPool);

  for (int i = 0; i < MAX_BULLETS; i++) {
    entity_t b = WorldCreateEntity(world, &bulletMask);

    ECS_GET(world, b, Active, COMP_ACTIVE)->value = false;
    ECS_GET(world, b, BulletType, COMP_BULLETTYPE)->type = 0;

    Timer *life = ECS_GET(world, b, Timer, COMP_TIMER);
    life->value = 0.0f;

    ModelCollection_t *mc = ECS_GET(world, b, ModelCollection_t, COMP_MODEL);

    ModelCollectionInit(mc, 1);
    ModelCollectionAdd(mc, (ModelInstance_t){.model = gw.bulletModel,
                                             .scale = (Vector3){1, 1, 1},
                                             .rotationMode = MODEL_ROT_FULL,
                                             .isActive = true});

    SphereCollider *sphere =
        ECS_GET(world, b, SphereCollider, COMP_SPHERE_COLLIDER);

    sphere->radius = 0.25f;

    CollisionInstance *ci =
        ECS_GET(world, b, CollisionInstance, COMP_COLLISION_INSTANCE);

    ci->type = COLLIDER_SPHERE;
    ci->layerMask = 1 << LAYER_BULLET;
    ci->collideMask =
        (1 << LAYER_ENEMY) | (1 << LAYER_PLAYER) | (1 << LAYER_WORLD);
  }

  return gw;
}

// --- LEVEL 1 SPAWNER ---
void SpawnLevel01(world_t *world, GameWorld *gw) {
  gw->terrainModel = LoadModel("assets/models/terrain-level1.glb");
  gw->terrainHeightMap =
      HeightMap_FromMesh(gw->terrainModel.meshes[0], MatrixIdentity());
  NavGrid_LoadFromImage(&gw->navGrid, "navmap.png", 2,
                        (Vector3){-180, 0, -180});

  // Spawn Level Geo
  Model ArenaModel175 = LoadModel("assets/models/175-radius-arena.glb");
  SpawnLevelModel(world, gw, ArenaModel175, (Vector3){0, 0, 0},
                  (Vector3){PI, 0, 0}, (Vector3){1, 1, 1});

  // Spawn Entities
  gw->player = SpawnPlayer(world, gw, (Vector3){0, 1.8f, 0});
  SpawnEnemyGrunt(world, gw, (Vector3){10, 0, 10});

  // Bullet Archetype setup (Same as before)
  uint32_t bulletBits[] = {
      COMP_POSITION,    COMP_VELOCITY,        COMP_ORIENTATION,
      COMP_MODEL,       COMP_BULLETTYPE,      COMP_TIMER,
      COMP_ACTIVE,      COMP_SPHERE_COLLIDER, COMP_COLLISION_INSTANCE,
      COMP_BULLET_OWNER};
  bitset_t bulletMask = MakeMask(bulletBits, 10);

  archetype_t *bulletArch = WorldGetArchetype(world, gw->bulletArchId);

  for (int i = 0; i < MAX_BULLETS; i++) {
    entity_t b = WorldCreateEntity(world, &bulletMask);

    ECS_GET(world, b, Active, COMP_ACTIVE)->value = false;
    ECS_GET(world, b, BulletType, COMP_BULLETTYPE)->type = 0;

    Timer *life = ECS_GET(world, b, Timer, COMP_TIMER);
    life->value = 0.0f;

    ModelCollection_t *mc = ECS_GET(world, b, ModelCollection_t, COMP_MODEL);

    ModelCollectionInit(mc, 1);
    ModelCollectionAdd(mc, (ModelInstance_t){.model = gw->bulletModel,
                                             .scale = (Vector3){1, 1, 1},
                                             .rotationMode = MODEL_ROT_FULL,
                                             .isActive = true});

    SphereCollider *sphere =
        ECS_GET(world, b, SphereCollider, COMP_SPHERE_COLLIDER);

    sphere->radius = 0.25f;

    CollisionInstance *ci =
        ECS_GET(world, b, CollisionInstance, COMP_COLLISION_INSTANCE);

    ci->type = COLLIDER_SPHERE;
    ci->layerMask = 1 << LAYER_BULLET;
    ci->collideMask =
        (1 << LAYER_ENEMY) | (1 << LAYER_PLAYER) | (1 << LAYER_WORLD);
  }

  // Random Boxes
  Model cube = LoadModelFromMesh(GenMeshCube(5, 5, 5));
  for (int i = 0; i < 50; ++i) {
    float x = GetRandomValue(-172, 172);
    float z = GetRandomValue(-172, 172);
    float y = GetRandomValue(0, 15);

    float height = GetRandomValue(2, 6);
    float width = GetRandomValue(2, 6);
    float depth = GetRandomValue(2, 6);

    entity_t box = SpawnBox(world, gw, (Vector3){x, y, z},
                            (Vector3){width, height, depth});

    ModelCollection_t *mc = ECS_GET(world, box, ModelCollection_t, COMP_MODEL);

    ModelCollectionInit(mc, 1);
    ModelCollectionAdd(
        mc, (ModelInstance_t){
                .model = cube,
                .scale = (Vector3){width / 5.0f, height / 5.0f, depth / 5.0f},
                .rotationMode = MODEL_ROT_FULL,
                .isActive = true});

    CollisionInstance *ci =
        ECS_GET(world, box, CollisionInstance, COMP_COLLISION_INSTANCE);

    ci->type = COLLIDER_AABB;
    ci->layerMask = 1 << LAYER_WORLD;
    ci->collideMask = (1 << LAYER_PLAYER) | (1 << LAYER_BULLET);

    AABBCollider *aabb = ECS_GET(world, box, AABBCollider, COMP_AABB_COLLIDER);

    if (!aabb)
      break;

    Collision_UpdateAABB(ci, aabb, (Vector3){x, y, z});
  }
}
