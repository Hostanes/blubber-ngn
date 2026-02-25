
#include "level_creater_helper.h"
#include "components/components.h"
#include "components/renderable.h"
#include "ecs_get.h"
#include <raymath.h>

//  Simple Archetype Helper

uint32_t CreateArchetype(world_t *world, uint32_t *components, int count) {
  bitset_t mask = MakeMask(components, count);
  return WorldCreateArchetype(world, &mask);
}

// PLAYER ARCHETYPE

uint32_t RegisterPlayerArchetype(world_t *world, Engine *engine) {
  uint32_t comps[] = {COMP_POSITION,
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
                      COMP_COYOTETIMER,
                      COMP_ISDASHING,
                      COMP_DASHCOOLDOWN};

  uint32_t id = CreateArchetype(world, comps, sizeof(comps) / sizeof(uint32_t));

  archetype_t *arch = WorldGetArchetype(world, id);

  /* ---------- Inline Components ---------- */

  ArchetypeAddInline(arch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(arch, COMP_VELOCITY, sizeof(Velocity));
  ArchetypeAddInline(arch, COMP_ORIENTATION, sizeof(Orientation));
  ArchetypeAddInline(arch, COMP_ACTIVE, sizeof(Active));
  ArchetypeAddInline(arch, COMP_COLLISION_INSTANCE, sizeof(CollisionInstance));
  ArchetypeAddInline(arch, COMP_CAPSULE_COLLIDER, sizeof(CapsuleCollider));
  ArchetypeAddInline(arch, COMP_ISGROUNDED, sizeof(bool));
  ArchetypeAddInline(arch, COMP_ISDASHING, sizeof(bool));
  ArchetypeAddInline(arch, COMP_MUZZLES, sizeof(MuzzleCollection_t));

  /* ---------- Handle Components ---------- */

  ArchetypeAddHandle(arch, COMP_MODEL, &engine->modelPool);
  ArchetypeAddHandle(arch, COMP_TIMER, &engine->timerPool);
  ArchetypeAddHandle(arch, COMP_DASHTIMER, &engine->timerPool);
  ArchetypeAddHandle(arch, COMP_COYOTETIMER, &engine->timerPool);
  ArchetypeAddHandle(arch, COMP_DASHCOOLDOWN, &engine->timerPool);

  return id;
}

// LEVEL MODEL

uint32_t RegisterLevelModelArchetype(world_t *world, Engine *engine) {
  uint32_t comps[] = {COMP_POSITION, COMP_ORIENTATION, COMP_MODEL, COMP_ACTIVE};

  uint32_t id = CreateArchetype(world, comps, sizeof(comps) / sizeof(uint32_t));

  archetype_t *arch = WorldGetArchetype(world, id);

  /* Inline */
  ArchetypeAddInline(arch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(arch, COMP_ORIENTATION, sizeof(Orientation));
  ArchetypeAddInline(arch, COMP_ACTIVE, sizeof(Active));

  /* Handle */
  ArchetypeAddHandle(arch, COMP_MODEL, &engine->modelPool);

  return id;
}
// ENEMY ARCHETYPE

uint32_t RegisterEnemyArchetype(world_t *world, Engine *engine) {
  uint32_t comps[] = {COMP_POSITION,         COMP_VELOCITY,
                      COMP_ORIENTATION,      COMP_MODEL,
                      COMP_ACTIVE,           COMP_COLLISION_INSTANCE,
                      COMP_CAPSULE_COLLIDER, COMP_HEALTH,
                      COMP_NAVPATH,          COMP_GRUNT_FIRE_TIMER,
                      COMP_MUZZLES};

  uint32_t id = CreateArchetype(world, comps, sizeof(comps) / sizeof(uint32_t));

  archetype_t *arch = WorldGetArchetype(world, id);

  ArchetypeAddInline(arch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(arch, COMP_VELOCITY, sizeof(Velocity));
  ArchetypeAddInline(arch, COMP_ORIENTATION, sizeof(Orientation));
  ArchetypeAddInline(arch, COMP_ACTIVE, sizeof(Active));
  ArchetypeAddInline(arch, COMP_HEALTH, sizeof(Health));
  ArchetypeAddInline(arch, COMP_NAVPATH, sizeof(NavPath));
  ArchetypeAddInline(arch, COMP_COLLISION_INSTANCE, sizeof(CollisionInstance));
  ArchetypeAddInline(arch, COMP_CAPSULE_COLLIDER, sizeof(CapsuleCollider));
  ArchetypeAddInline(arch, COMP_MUZZLES, sizeof(MuzzleCollection_t));

  ArchetypeAddHandle(arch, COMP_MODEL, &engine->modelPool);
  ArchetypeAddHandle(arch, COMP_GRUNT_FIRE_TIMER, &engine->timerPool);

  return id;
}

// BOX ARCHETYPE

uint32_t RegisterBoxArchetype(world_t *world, Engine *engine) {
  uint32_t comps[] = {
      COMP_POSITION, COMP_ORIENTATION,        COMP_MODEL,
      COMP_ACTIVE,   COMP_COLLISION_INSTANCE, COMP_AABB_COLLIDER};

  uint32_t id = CreateArchetype(world, comps, sizeof(comps) / sizeof(uint32_t));

  archetype_t *arch = WorldGetArchetype(world, id);

  ArchetypeAddInline(arch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(arch, COMP_ORIENTATION, sizeof(Orientation));
  ArchetypeAddInline(arch, COMP_ACTIVE, sizeof(Active));
  ArchetypeAddInline(arch, COMP_COLLISION_INSTANCE, sizeof(CollisionInstance));
  ArchetypeAddInline(arch, COMP_AABB_COLLIDER, sizeof(AABBCollider));

  ArchetypeAddHandle(arch, COMP_MODEL, &engine->modelPool);

  return id;
}

// FACTORY SPAWN FUNCTIONS

entity_t SpawnPlayer(world_t *world, GameWorld *gw, Vector3 position) {
  archetype_t *arch = WorldGetArchetype(world, gw->playerArchId);
  entity_t e = WorldCreateEntity(world, &arch->mask);

  ECS_GET(world, e, Position, COMP_POSITION)->value = position;
  ECS_GET(world, e, Active, COMP_ACTIVE)->value = true;

  /* ---------------- Model Setup ---------------- */

  ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);

  ModelCollectionInit(mc, 2);

  Model playerBody = LoadModelFromMesh(GenMeshCube(.2f, 2, .2f));

  ModelCollectionAdd(mc, (ModelInstance_t){.model = playerBody,
                                           .offset = (Vector3){0, -1.5f, 0},
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_YAW_ONLY});

  ModelCollectionAdd(mc, (ModelInstance_t){.model = gw->gunModel,
                                           .offset = (Vector3){0, -0.5f, 0},
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_FULL});

  /* ---------------- Collision ---------------- */

  CapsuleCollider *cap =
      ECS_GET(world, e, CapsuleCollider, COMP_CAPSULE_COLLIDER);

  cap->radius = 0.35f;
  cap->a = (Vector3){0, -1.55f, 0};
  cap->b = (Vector3){0, 0, 0};

  CollisionInstance *ci =
      ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);

  ci->owner = e;
  ci->type = COLLIDER_CAPSULE;
  ci->layerMask = 1 << LAYER_PLAYER;
  ci->collideMask = 1 << LAYER_WORLD;
  ci->worldBounds = Capsule_ComputeAABB(cap);

  /* ---------------- Movement State ---------------- */

  bool *isGrounded = ECS_GET(world, e, bool, COMP_ISGROUNDED);
  *isGrounded = false;

  bool *isDashing = ECS_GET(world, e, bool, COMP_ISDASHING);
  *isDashing = false;

  /* ---------------- Muzzle Setup ---------------- */

  MuzzleCollection_t *muzzles =
      ECS_GET(world, e, MuzzleCollection_t, COMP_MUZZLES);

  muzzles->count = 1;
  muzzles->Muzzles = malloc(sizeof(Muzzle_t));

  muzzles->Muzzles[0] =
      (Muzzle_t){.positionOffset = {.value = {0.25f, -0.3f, 1.5f}},
                 .oriOffset = {.yaw = 0.0f, .pitch = 0.0f},
                 .bulletType = 1};

  return e;
}

// ---------------- Enemy Grunt ----------------

entity_t SpawnEnemyGrunt(world_t *world, GameWorld *game, Vector3 position) {
  archetype_t *arch = WorldGetArchetype(world, game->enemyGruntArchId);

  entity_t e = WorldCreateEntity(world, &arch->mask);

  /* -------- Basic State -------- */

  ECS_GET(world, e, Position, COMP_POSITION)->value = position;
  ECS_GET(world, e, Active, COMP_ACTIVE)->value = true;

  Health *hp = ECS_GET(world, e, Health, COMP_HEALTH);
  hp->max = 150.0f;
  hp->current = 150.0f;

  Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
  ori->yaw = PI / 4;
  ori->pitch = 0.0f;

  /* -------- Model -------- */

  ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);

  ModelCollectionInit(mc, 3);

  ModelCollectionAdd(mc, (ModelInstance_t){.model = game->gruntLegs,
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_YAW_ONLY});

  ModelCollectionAdd(mc, (ModelInstance_t){.model = game->gruntGun,
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_WORLD});

  ModelCollectionAdd(mc, (ModelInstance_t){.model = game->gruntTorso,
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_WORLD});

  /* -------- Collider -------- */

  CapsuleCollider *cap =
      ECS_GET(world, e, CapsuleCollider, COMP_CAPSULE_COLLIDER);

  cap->radius = 1.2f;
  cap->a = Vector3Add(position, (Vector3){0, 0.0f, 0});
  cap->b = Vector3Add(position, (Vector3){0, 2.5f, 0});

  CollisionInstance *ci =
      ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);

  ci->owner = e;
  ci->type = COLLIDER_CAPSULE;
  ci->layerMask = 1 << LAYER_ENEMY;
  ci->collideMask = 1 << LAYER_BULLET;

  /* -------- Fire Timer -------- */

  Timer *fire = ECS_GET(world, e, Timer, COMP_GRUNT_FIRE_TIMER);
  fire->value = 1.0f;

  /* -------- Nav -------- */

  NavPath *nav = ECS_GET(world, e, NavPath, COMP_NAVPATH);
  NavPath_Init(nav, 32);

  /* -------- Muzzle -------- */

  MuzzleCollection_t *muzzles =
      ECS_GET(world, e, MuzzleCollection_t, COMP_MUZZLES);

  muzzles->count = 1;
  muzzles->Muzzles = malloc(sizeof(Muzzle_t));

  muzzles->Muzzles[0] =
      (Muzzle_t){.positionOffset = {.value = {0.0f, 3.0f, 1.2f}},
                 .oriOffset = {.yaw = 0.0f, .pitch = 0.0f},
                 .bulletType = BULLET_TYPE_STANDARD};

  return e;
}

// ---------------- Enemy Missile ----------------

entity_t SpawnEnemyMissile(world_t *world, GameWorld *game, Vector3 position) {
  archetype_t *arch = WorldGetArchetype(world, game->enemyMissileArchId);

  entity_t e = WorldCreateEntity(world, &arch->mask);

  /* -------- Basic State -------- */

  ECS_GET(world, e, Position, COMP_POSITION)->value = position;
  ECS_GET(world, e, Active, COMP_ACTIVE)->value = true;

  Health *hp = ECS_GET(world, e, Health, COMP_HEALTH);
  hp->max = 250.0f;
  hp->current = 250.0f;

  Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
  ori->yaw = 0.0f;
  ori->pitch = 0.0f;

  /* -------- Model -------- */

  ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);

  ModelCollectionInit(mc, 1);

  ModelCollectionAdd(mc, (ModelInstance_t){.model = game->missileEnemyModel,
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_YAW_ONLY});

  /* -------- Collider -------- */

  CapsuleCollider *cap =
      ECS_GET(world, e, CapsuleCollider, COMP_CAPSULE_COLLIDER);

  cap->radius = 1.5f;
  cap->a = Vector3Add(position, (Vector3){0, 0.5f, 0});
  cap->b = Vector3Add(position, (Vector3){0, 1.0f, 0});

  CollisionInstance *ci =
      ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);

  ci->owner = e;
  ci->type = COLLIDER_CAPSULE;
  ci->layerMask = 1 << LAYER_ENEMY;
  ci->collideMask = 1 << LAYER_BULLET;

  /* -------- Fire Timer -------- */

  Timer *fire = ECS_GET(world, e, Timer, COMP_GRUNT_FIRE_TIMER);
  fire->value = 2.5f;

  /* -------- Nav -------- */

  NavPath *nav = ECS_GET(world, e, NavPath, COMP_NAVPATH);
  NavPath_Init(nav, 32);

  /* -------- Muzzle -------- */

  MuzzleCollection_t *muzzles =
      ECS_GET(world, e, MuzzleCollection_t, COMP_MUZZLES);

  muzzles->count = 1;
  muzzles->Muzzles = malloc(sizeof(Muzzle_t));

  muzzles->Muzzles[0] =
      (Muzzle_t){.positionOffset = {.value = {0.0f, 3.0f, 0.0f}},
                 .oriOffset = {.yaw = 0.0f, .pitch = 0.0f},
                 .bulletType = BULLET_TYPE_STANDARD};

  return e;
}

// ---------------- Box ----------------

entity_t SpawnBox(world_t *world, GameWorld *gw, Vector3 position,
                  Vector3 size) {
  archetype_t *arch = WorldGetArchetype(world, gw->obstacleArchId);

  entity_t e = WorldCreateEntity(world, &arch->mask);

  ECS_GET(world, e, Position, COMP_POSITION)->value = position;
  ECS_GET(world, e, Active, COMP_ACTIVE)->value = true;

  AABBCollider *aabb = ECS_GET(world, e, AABBCollider, COMP_AABB_COLLIDER);

  aabb->halfExtents = Vector3Scale(size, 0.5f);

  return e;
}

entity_t SpawnBoxModel(world_t *world, GameWorld *gw, Vector3 position,
                       Vector3 size) {
  archetype_t *arch = WorldGetArchetype(world, gw->obstacleArchId);

  entity_t e = WorldCreateEntity(world, &arch->mask);

  /* -------- Basic -------- */

  ECS_GET(world, e, Position, COMP_POSITION)->value = position;
  ECS_GET(world, e, Active, COMP_ACTIVE)->value = true;

  /* -------- Model -------- */

  Model boxModel = LoadModelFromMesh(GenMeshCube(size.x, size.y, size.z));

  ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);

  ModelCollectionInit(mc, 1);

  ModelCollectionAdd(mc, (ModelInstance_t){.model = boxModel,
                                           .offset = (Vector3){0, 0, 0},
                                           .rotation = (Vector3){0, 0, 0},
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_FULL});

  /* -------- Collider -------- */

  AABBCollider *aabb = ECS_GET(world, e, AABBCollider, COMP_AABB_COLLIDER);

  aabb->halfExtents = (Vector3){size.x * 0.5f, size.y * 0.5f, size.z * 0.5f};

  CollisionInstance *ci =
      ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);

  ci->owner = e;
  ci->type = COLLIDER_AABB;
  ci->layerMask = 1 << LAYER_WORLD;
  ci->collideMask = (1 << LAYER_PLAYER) | (1 << LAYER_BULLET);

  return e;
}

entity_t SpawnLevelModel(world_t *world, GameWorld *gw, Model model,
                         Vector3 position, Vector3 rotation, Vector3 scale) {
  archetype_t *arch = WorldGetArchetype(world, gw->levelModelArchId);

  entity_t e = WorldCreateEntity(world, &arch->mask);

  /* -------- Transform -------- */

  ECS_GET(world, e, Position, COMP_POSITION)->value = position;

  Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
  active->value = true;

  Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);

  /* -------- Model -------- */

  ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);

  ModelCollectionInit(mc, 1);

  ModelCollectionAdd(mc, (ModelInstance_t){.model = model,
                                           .offset = (Vector3){0, 0, 0},
                                           .rotation = (Vector3){rotation.y, rotation.x, 0},
                                           .scale = scale,
                                           .rotationMode = MODEL_ROT_FULL});

  return e;
}
