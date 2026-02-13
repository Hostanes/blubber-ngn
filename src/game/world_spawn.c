#include "components/components.h"
#include "game.h"
#include <raylib.h>
#include <raymath.h>

#define MAX_BULLETS 2048

GameWorld GameWorldCreate(Engine *engine, world_t *world) {
  GameWorld gw = {0};
  gw.gameState = GAMESTATE_MAINMENU;

  gw.terrainModel = LoadModel("assets/models/terrain-tutorial.glb");
  // gw.terrainModel = LoadModel("assets/models/terrain-level1.glb");
  gw.terrainHeightMap =
      HeightMap_FromMesh(gw.terrainModel.meshes[0], MatrixIdentity());

  /* ---------- Component pools ---------- */

  static componentPool_t modelPool;
  static componentPool_t timerPool;

  ComponentPoolInit(&modelPool, sizeof(ModelCollection_t));
  ComponentPoolInit(&timerPool, sizeof(Timer));

  Model cube = LoadModelFromMesh(GenMeshCube(5, 5, 5));
  Model gun = LoadModel("assets/models/gun1.glb");

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
                           COMP_ISGROUNDED};

  bitset_t playerMask = MakeMask(playerBits, 8);

  gw.playerArchId = WorldCreateArchetype(world, &playerMask);
  archetype_t *playerArch = WorldGetArchetype(world, gw.playerArchId);

  ArchetypeAddInline(playerArch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(playerArch, COMP_VELOCITY, sizeof(Velocity));
  ArchetypeAddInline(playerArch, COMP_ORIENTATION, sizeof(Orientation));
  ArchetypeAddInline(playerArch, COMP_ACTIVE, sizeof(Active));
  ArchetypeAddInline(playerArch, COMP_ISGROUNDED, sizeof(bool));
  ArchetypeAddHandle(playerArch, COMP_MODEL, &engine->modelPool);
  ArchetypeAddHandle(playerArch, COMP_TIMER, &engine->timerPool);
  ArchetypeAddHandle(playerArch, COMP_COYOTETIMER, &engine->timerPool);
  ArchetypeAddInline(playerArch, COMP_COLLISION_INSTANCE,
                     sizeof(CollisionInstance));
  ArchetypeAddInline(playerArch, COMP_CAPSULE_COLLIDER,
                     sizeof(CapsuleCollider));

  gw.player = WorldCreateEntity(world, &playerMask);

  ECS_GET(world, gw.player, Position, COMP_POSITION)->value =
      (Vector3){0, 1.8f, 0};

  Active *active = ECS_GET(world, gw.player, Active, COMP_ACTIVE);
  active->value = true;

  ModelCollection_t *mc =
      ECS_GET(world, gw.player, ModelCollection_t, COMP_MODEL);

  ModelCollectionInit(mc, 2);

  Model playerBody = LoadModelFromMesh(GenMeshCube(1, 2, 1));

  ModelCollectionAdd(mc, (ModelInstance_t){.model = playerBody,
                                           .offset = (Vector3){0, -2.0f, 0},
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_YAW_ONLY});

  ModelCollectionAdd(mc, (ModelInstance_t){.model = gun,
                                           .offset = (Vector3){0, 5, 0},
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_FULL});

  CapsuleCollider *cap =
      ECS_GET(world, gw.player, CapsuleCollider, COMP_CAPSULE_COLLIDER);

  bool *isgrounded = ECS_GET(world, gw.player, bool, COMP_ISGROUNDED);
  *isgrounded = false;

  cap->radius = 0.35f;
  cap->a = (Vector3){0, 0, 0}; // will be updated per-frame
  cap->b = (Vector3){0, 1.8f, 0};

  CollisionInstance *ci =
      ECS_GET(world, gw.player, CollisionInstance, COMP_COLLISION_INSTANCE);

  ci->owner = gw.player;
  ci->type = COLLIDER_CAPSULE;
  ci->shape = cap;
  ci->layerMask = 1 << 0;   // PLAYER
  ci->collideMask = 1 << 1; // WORLD
  ci->worldBounds = Capsule_ComputeAABB(cap);

  /* ---------- Bullet archetype ---------- */

  Model bulletModel = LoadModel("assets/models/bullet.glb");

  uint32_t bulletBits[] = {COMP_POSITION, COMP_VELOCITY,   COMP_ORIENTATION,
                           COMP_MODEL,    COMP_BULLETTYPE, COMP_TIMER,
                           COMP_ACTIVE};

  bitset_t bulletMask = MakeMask(bulletBits, 7);
  gw.bulletArchId = WorldCreateArchetype(world, &bulletMask);
  archetype_t *bulletArch = WorldGetArchetype(world, gw.bulletArchId);

  ArchetypeAddInline(bulletArch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(bulletArch, COMP_VELOCITY, sizeof(Velocity));
  ArchetypeAddInline(bulletArch, COMP_ORIENTATION, sizeof(Orientation));
  ArchetypeAddInline(bulletArch, COMP_BULLETTYPE, sizeof(int));
  ArchetypeAddInline(bulletArch, COMP_ACTIVE, sizeof(Active));
  ArchetypeAddInline(bulletArch, COMP_ACTIVE, sizeof(SphereCollider));

  ArchetypeAddHandle(bulletArch, COMP_MODEL, &engine->modelPool);
  ArchetypeAddHandle(bulletArch, COMP_TIMER, &engine->timerPool);

  for (int i = 0; i < MAX_BULLETS; i++) {
    entity_t b = WorldCreateEntity(world, &bulletMask);

    ECS_GET(world, b, Active, COMP_ACTIVE)->value = false;

    Timer *life = ECS_GET(world, b, Timer, COMP_TIMER);
    Active *active = ECS_GET(world, b, Active, COMP_ACTIVE);
    life->value = 0.0f;
    active->value = false;

    ModelCollection_t *mc = ECS_GET(world, b, ModelCollection_t, COMP_MODEL);
    ModelCollectionInit(mc, 1);
    ModelCollectionAdd(mc, (ModelInstance_t){.model = bulletModel,
                                             .scale = (Vector3){1, 1, 1},
                                             .offset = (Vector3){0, 0, 0},
                                             .rotationMode = MODEL_ROT_FULL});
    mc->models[0].offset = (Vector3){0, 0, 0};
    mc->models[0].rotation = (Vector3){0, 0, 0};
  }

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
    float x = GetRandomValue(-150, 150);
    float z = GetRandomValue(-150, 150);
    float y = GetRandomValue(0, 10);

    // --- Random height ---
    float height = GetRandomValue(2, 2); // box height range
    float width = GetRandomValue(2, 6);
    float depth = GetRandomValue(2, 6);

    float halfHeight = height * 0.5f;

    ECS_GET(world, box, Position, COMP_POSITION)->value =
        (Vector3){x, y, z};

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
    ci->shape = aabb;
    ci->layerMask = 1 << 1;   // WORLD
    ci->collideMask = 1 << 0; // PLAYER
  }

  return gw;
}
