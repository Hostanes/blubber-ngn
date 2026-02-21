#include "components/components.h"
#include "components/movement.h"
#include "components/muzzle.h"
#include "components/renderable.h"
#include "components/transform.h"
#include "ecs_get.h"
#include "game.h"
#include "nav_grid/nav.h"
#include <raylib.h>
#include <raymath.h>

#define MAX_BULLETS 2048

void Enemy_OnCollision(world_t *world, entity_t self, entity_t other) {}

entity_t SpawnEnemyCapsule(world_t *world, GameWorld *game, Vector3 position,
                           bitset_t *mask) {
  entity_t e = WorldCreateEntity(world, mask);

  ECS_GET(world, e, Position, COMP_POSITION)->value = position;

  Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
  ori->yaw = 0.0f;
  ori->pitch = 0.0f;

  ECS_GET(world, e, Active, COMP_ACTIVE)->value = true;

  Health *hp = ECS_GET(world, e, Health, COMP_HEALTH);
  hp->max = 150.0f;
  hp->current = 150.0f;

  ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);

  ModelCollectionInit(mc, 3);
  ModelCollectionAdd(mc, (ModelInstance_t){.model = game->gruntLegs,
                                           .scale = (Vector3){1, 1, 1},
                                           .offset = (Vector3){0, 0, 0},
                                           .rotationMode = MODEL_ROT_YAW_ONLY});
  ModelCollectionAdd(mc, (ModelInstance_t){.model = game->gruntGun,
                                           .scale = (Vector3){1, 1, 1},
                                           .offset = (Vector3){0, 0, 0},
                                           .rotationMode = MODEL_ROT_WORLD});
  ModelCollectionAdd(mc, (ModelInstance_t){.model = game->gruntTorso,
                                           .scale = (Vector3){1, 1, 1},
                                           .offset = (Vector3){0, 0, 0},
                                           .rotationMode = MODEL_ROT_WORLD});

  CapsuleCollider *cap =
      ECS_GET(world, e, CapsuleCollider, COMP_CAPSULE_COLLIDER);

  cap->radius = 1.2f;
  cap->a = Vector3Add(position, (Vector3){0, 0.5f, 0});
  cap->b = Vector3Add(position, (Vector3){0, 3.5f, 0});

  CollisionInstance *ci =
      ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);

  ci->owner = e;
  ci->type = COLLIDER_CAPSULE;
  ci->layerMask = 1 << LAYER_ENEMY;
  ci->collideMask = 1 << LAYER_BULLET;

  CollisionResponse *resp =
      ECS_GET(world, e, CollisionResponse, COMP_ON_COLLISION);

  resp->onCollision = Enemy_OnCollision;

  NavPath *navPath = ECS_GET(world, e, NavPath, COMP_NAVPATH);
  NavPath_Init(navPath, 32);

  return e;
}

GameWorld GameWorldCreate(Engine *engine, world_t *world) {
  GameWorld gw = {0};
  gw.gameState = GAMESTATE_MAINMENU;

  gw.terrainModel = LoadModel("assets/models/terrain-level1.glb");
  // gw.terrainModel = LoadModel("assets/models/terrain-level1.glb");
  gw.terrainHeightMap =
      HeightMap_FromMesh(gw.terrainModel.meshes[0], MatrixIdentity());

  float worldWidth = 360;
  float cellSize = 2;
  int cellCount = worldWidth / cellSize;

  NavGrid_LoadFromImage(&gw.navGrid, "navmap.png", 2, (Vector3){-180, 0, -180});

  // NavGrid_Init(&gw.navGrid, cellCount, cellCount, cellSize,
  //              (Vector3){-180, 0, -180});

  // for (int i = 50; i < 100; i++) {
  //   NavGrid_SetCell(&gw.navGrid, i, 50, NAV_CELL_WALL);
  // }

  /* ---------- Component pools ---------- */

  static componentPool_t modelPool;
  static componentPool_t timerPool;

  ComponentPoolInit(&modelPool, sizeof(ModelCollection_t));
  ComponentPoolInit(&timerPool, sizeof(Timer));

  Model cube = LoadModelFromMesh(GenMeshCube(5, 5, 5));
  gw.gunModel = LoadModel("assets/models/gun1.glb");
  gw.bulletModel = LoadModel("assets/models/bullet.glb");

  gw.enemyModel = LoadModel("assets/models/enemy-target.glb");

  gw.gruntGun = LoadModel("assets/models/enemies/grunt/grunt-gun.glb");
  gw.gruntLegs = LoadModel("assets/models/enemies/grunt/grunt-legs.glb");
  gw.gruntTorso = LoadModel("assets/models/enemies/grunt/grunt-torso.glb");

  /* ---------- Player archetype ---------- */

  uint32_t playerBits[] = {COMP_POSITION,
                           COMP_VELOCITY,
                           COMP_ORIENTATION,
                           COMP_MODEL,
                           COMP_TIMER,
                           COMP_GRAVITY,
                           COMP_ACTIVE,
                           COMP_COLLISION_INSTANCE,
                           COMP_CAPSULE_COLLIDER,
                           COMP_ISGROUNDED,
                           COMP_MUZZLES,
                           COMP_DASHTIMER,
                           COMP_ISDASHING,
                           COMP_DASHCOOLDOWN};

  bitset_t playerMask =
      MakeMask(playerBits, sizeof(playerBits) / sizeof(uint32_t));

  gw.playerArchId = WorldCreateArchetype(world, &playerMask);
  archetype_t *playerArch = WorldGetArchetype(world, gw.playerArchId);

  ArchetypeAddInline(playerArch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(playerArch, COMP_VELOCITY, sizeof(Velocity));
  ArchetypeAddInline(playerArch, COMP_ORIENTATION, sizeof(Orientation));
  ArchetypeAddInline(playerArch, COMP_ACTIVE, sizeof(Active));
  ArchetypeAddInline(playerArch, COMP_ISGROUNDED, sizeof(bool));
  ArchetypeAddInline(playerArch, COMP_ISDASHING, sizeof(bool));

  ArchetypeAddHandle(playerArch, COMP_MODEL, &engine->modelPool);
  ArchetypeAddHandle(playerArch, COMP_TIMER, &engine->timerPool);
  ArchetypeAddHandle(playerArch, COMP_COYOTETIMER, &engine->timerPool);
  ArchetypeAddHandle(playerArch, COMP_DASHTIMER, &engine->timerPool);
  ArchetypeAddHandle(playerArch, COMP_DASHCOOLDOWN, &engine->timerPool);

  ArchetypeAddInline(playerArch, COMP_COLLISION_INSTANCE,
                     sizeof(CollisionInstance));
  ArchetypeAddInline(playerArch, COMP_CAPSULE_COLLIDER,
                     sizeof(CapsuleCollider));
  ArchetypeAddInline(playerArch, COMP_MUZZLES, sizeof(MuzzleCollection_t));

  gw.player = WorldCreateEntity(world, &playerMask);

  ECS_GET(world, gw.player, Position, COMP_POSITION)->value =
      (Vector3){0, 1.8f, 0};

  Active *active = ECS_GET(world, gw.player, Active, COMP_ACTIVE);
  active->value = true;

  ModelCollection_t *mc =
      ECS_GET(world, gw.player, ModelCollection_t, COMP_MODEL);

  ModelCollectionInit(mc, 2);

  Model playerBody = LoadModelFromMesh(GenMeshCube(.2f, 2, .2f));

  ModelCollectionAdd(mc, (ModelInstance_t){.model = playerBody,
                                           .offset = (Vector3){0, -1.5f, 0},
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_YAW_ONLY});

  ModelCollectionAdd(mc, (ModelInstance_t){.model = gw.gunModel,
                                           .offset = (Vector3){0, -0.5f, 0},
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_FULL});

  CapsuleCollider *cap =
      ECS_GET(world, gw.player, CapsuleCollider, COMP_CAPSULE_COLLIDER);

  bool *isgrounded = ECS_GET(world, gw.player, bool, COMP_ISGROUNDED);
  *isgrounded = false;

  cap->radius = 0.35f;
  cap->a = (Vector3){0, -1.55f, 0}; // will be updated per-frame
  cap->b = (Vector3){0, 0, 0};

  CollisionInstance *ci =
      ECS_GET(world, gw.player, CollisionInstance, COMP_COLLISION_INSTANCE);

  ci->owner = gw.player;
  ci->type = COLLIDER_CAPSULE;
  ci->layerMask = 1 << LAYER_PLAYER;
  ci->collideMask = 1 << LAYER_WORLD;
  ci->worldBounds = Capsule_ComputeAABB(cap);

  MuzzleCollection_t *muzzles =
      ECS_GET(world, gw.player, MuzzleCollection_t, COMP_MUZZLES);

  muzzles->count = 1;
  muzzles->Muzzles = malloc(sizeof(Muzzle_t) * 1);

  muzzles->Muzzles[0] =
      (Muzzle_t){.positionOffset = {.value = {0.25f, -0.3f, 1.5}},
                 .oriOffset = {.yaw = 0.0f, .pitch = 0.0f},
                 .bulletType = 1};

  /* ---------- Bullet archetype ---------- */
  uint32_t bulletBits[] = {
      COMP_POSITION, COMP_VELOCITY,        COMP_ORIENTATION,
      COMP_MODEL,    COMP_BULLETTYPE,      COMP_TIMER,
      COMP_ACTIVE,   COMP_SPHERE_COLLIDER, COMP_COLLISION_INSTANCE};

  bitset_t bulletMask =
      MakeMask(bulletBits, sizeof(bulletBits) / sizeof(uint32_t));
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
    Active *active = ECS_GET(world, b, Active, COMP_ACTIVE);
    life->value = 0.0f;
    active->value = false;

    ModelCollection_t *mc = ECS_GET(world, b, ModelCollection_t, COMP_MODEL);
    ModelCollectionInit(mc, 1);
    ModelCollectionAdd(mc, (ModelInstance_t){.model = gw.bulletModel,
                                             .scale = (Vector3){1, 1, 1},
                                             .offset = (Vector3){0, 0, 0},
                                             .rotationMode = MODEL_ROT_FULL});
    mc->models[0].offset = (Vector3){0, 0, 0};
    mc->models[0].rotation = (Vector3){0, 0, 0};

    SphereCollider *sphere =
        ECS_GET(world, b, SphereCollider, COMP_SPHERE_COLLIDER);

    sphere->radius = 0.25f; // bullet radius

    CollisionInstance *ci =
        ECS_GET(world, b, CollisionInstance, COMP_COLLISION_INSTANCE);

    ci->type = COLLIDER_SPHERE;
    ci->layerMask = 1 << LAYER_BULLET;
    ci->collideMask = (1 << LAYER_ENEMY) | (1 << LAYER_WORLD);
  }

  /* ---------- enemy archetype ---------- */

  uint32_t enemyCapsuleBits[] = {COMP_POSITION,         COMP_VELOCITY,
                                 COMP_ORIENTATION,      COMP_MODEL,
                                 COMP_ACTIVE,           COMP_COLLISION_INSTANCE,
                                 COMP_CAPSULE_COLLIDER, COMP_HEALTH,
                                 COMP_ON_COLLISION,     COMP_NAVPATH,
                                 COMP_GRUNT_FIRE_TIMER};

  bitset_t enemyCapsuleMask =
      MakeMask(enemyCapsuleBits, sizeof(enemyCapsuleBits) / sizeof(uint32_t));

  gw.enemyCapsuleArchId = WorldCreateArchetype(world, &enemyCapsuleMask);

  archetype_t *enemyCapsuleArch =
      WorldGetArchetype(world, gw.enemyCapsuleArchId);

  ArchetypeAddInline(enemyCapsuleArch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(enemyCapsuleArch, COMP_VELOCITY, sizeof(Velocity));
  ArchetypeAddInline(enemyCapsuleArch, COMP_ORIENTATION, sizeof(Orientation));
  ArchetypeAddInline(enemyCapsuleArch, COMP_ACTIVE, sizeof(Active));
  ArchetypeAddInline(enemyCapsuleArch, COMP_HEALTH, sizeof(Health));
  ArchetypeAddInline(enemyCapsuleArch, COMP_NAVPATH, sizeof(NavPath));

  ArchetypeAddHandle(enemyCapsuleArch, COMP_MODEL, &engine->modelPool);

  ArchetypeAddHandle(enemyCapsuleArch, COMP_GRUNT_FIRE_TIMER,
                     &engine->timerPool);

  ArchetypeAddInline(enemyCapsuleArch, COMP_COLLISION_INSTANCE,
                     sizeof(CollisionInstance));

  ArchetypeAddInline(enemyCapsuleArch, COMP_CAPSULE_COLLIDER,
                     sizeof(CapsuleCollider));

  ArchetypeAddInline(enemyCapsuleArch, COMP_ON_COLLISION,
                     sizeof(CollisionResponse));

  SpawnEnemyCapsule(world, &gw, (Vector3){-5, 1, 5}, &enemyCapsuleMask);

  /* ---------- Box archetype ---------- */

  uint32_t boxBits[] = {
      COMP_POSITION, COMP_ORIENTATION,        COMP_MODEL,
      COMP_ACTIVE,   COMP_COLLISION_INSTANCE, COMP_AABB_COLLIDER};

  bitset_t boxMask = MakeMask(boxBits, 6);

  gw.obstacleArchId = WorldCreateArchetype(world, &boxMask);
  archetype_t *obsatcleArch = WorldGetArchetype(world, gw.obstacleArchId);

  ArchetypeAddInline(obsatcleArch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(obsatcleArch, COMP_ORIENTATION, sizeof(Orientation));
  ArchetypeAddInline(obsatcleArch, COMP_ACTIVE, sizeof(Active));
  ArchetypeAddInline(obsatcleArch, COMP_COLLISION_INSTANCE,
                     sizeof(CollisionInstance));
  ArchetypeAddInline(obsatcleArch, COMP_AABB_COLLIDER, sizeof(AABBCollider));
  ArchetypeAddHandle(obsatcleArch, COMP_MODEL, &engine->modelPool);

  for (int i = 0; i < 150; ++i) {

    entity_t box = WorldCreateEntity(world, &boxMask);

    // --- Random position ---
    float x = GetRandomValue(-200, 200);
    float z = GetRandomValue(-200, 200);
    float y = GetRandomValue(0, 10);

    // --- Random height ---
    float height = GetRandomValue(2, 2); // box height range
    float width = GetRandomValue(2, 6);
    float depth = GetRandomValue(2, 6);

    float halfHeight = height * 0.5f;

    ECS_GET(world, box, Position, COMP_POSITION)->value = (Vector3){x, y, z};

    Active *active = ECS_GET(world, box, Active, COMP_ACTIVE);
    active->value = true;

    ModelCollection_t *mc = ECS_GET(world, box, ModelCollection_t, COMP_MODEL);
    ModelCollectionInit(mc, 1);

    ModelCollectionAdd(
        mc, (ModelInstance_t){
                .model = cube,
                .offset = (Vector3){0, 0, 0},
                .rotation = (Vector3){0, 0, 0},
                .scale = (Vector3){width / 5.0f, height / 5.0f, depth / 5.0f},
                .rotationMode = MODEL_ROT_FULL});

    // --- Collider ---
    AABBCollider *aabb = ECS_GET(world, box, AABBCollider, COMP_AABB_COLLIDER);

    aabb->halfExtents = (Vector3){width * 0.5f, height * 0.5f, depth * 0.5f};

    CollisionInstance *ci =
        ECS_GET(world, box, CollisionInstance, COMP_COLLISION_INSTANCE);

    ci->type = COLLIDER_AABB;
    ci->layerMask = 1 << LAYER_WORLD; // WORLD
    ci->collideMask = (1 << LAYER_PLAYER) | (1 << LAYER_BULLET);
  }

  float width = 3;
  float height = 13;
  float length = 160;

  Model rampWall = LoadModelFromMesh(GenMeshCube(width, height, length));

  entity_t rampCollider1 = WorldCreateEntity(world, &boxMask);

  Active *rampActive = ECS_GET(world, rampCollider1, Active, COMP_ACTIVE);
  rampActive->value = true;

  ECS_GET(world, rampCollider1, Position, COMP_POSITION)->value =
      (Vector3){80, -6.0, 0};

  ModelCollection_t *rampmc =
      ECS_GET(world, rampCollider1, ModelCollection_t, COMP_MODEL);
  ModelCollectionInit(rampmc, 1);

  ModelCollectionAdd(rampmc, (ModelInstance_t){.model = rampWall,
                                               .offset = (Vector3){0, 0, 0},
                                               .rotation = (Vector3){0, 0, 0},
                                               .scale = (Vector3){1, 1, 1},
                                               .rotationMode = MODEL_ROT_FULL});

  // --- Collider ---
  AABBCollider *aabb =
      ECS_GET(world, rampCollider1, AABBCollider, COMP_AABB_COLLIDER);

  aabb->halfExtents = (Vector3){width * 0.5f, height * 0.5f, length * 0.5f};

  CollisionInstance *rampCI =
      ECS_GET(world, rampCollider1, CollisionInstance, COMP_COLLISION_INSTANCE);

  rampCI->type = COLLIDER_AABB;
  rampCI->layerMask = 1 << LAYER_WORLD; // WORLD
  rampCI->collideMask = (1 << LAYER_PLAYER) | (1 << LAYER_BULLET);

  //

  entity_t rampCollider2 = WorldCreateEntity(world, &boxMask);

  Active *rampActive2 = ECS_GET(world, rampCollider2, Active, COMP_ACTIVE);
  rampActive2->value = true;

  ECS_GET(world, rampCollider2, Position, COMP_POSITION)->value =
      (Vector3){125, -6.0, 0};

  ModelCollection_t *rampmc2 =
      ECS_GET(world, rampCollider2, ModelCollection_t, COMP_MODEL);
  ModelCollectionInit(rampmc2, 1);

  ModelCollectionAdd(rampmc2,
                     (ModelInstance_t){.model = rampWall,
                                       .offset = (Vector3){0, 0, 0},
                                       .rotation = (Vector3){0, 0, 0},
                                       .scale = (Vector3){1, 1, 1},
                                       .rotationMode = MODEL_ROT_FULL});

  // --- Collider ---
  AABBCollider *aabb2 =
      ECS_GET(world, rampCollider2, AABBCollider, COMP_AABB_COLLIDER);

  aabb2->halfExtents = (Vector3){width * 0.5f, height * 0.5f, length * 0.5f};

  CollisionInstance *rampCI2 =
      ECS_GET(world, rampCollider2, CollisionInstance, COMP_COLLISION_INSTANCE);

  rampCI2->type = COLLIDER_AABB;
  rampCI2->layerMask = 1 << LAYER_WORLD; // WORLD
  rampCI2->collideMask = (1 << LAYER_PLAYER) | (1 << LAYER_BULLET);

  return gw;
}
