
#include "level_creater_helper.h"
#include "components/components.h"
#include "components/muzzle.h"
#include "components/renderable.h"
#include "ecs_get.h"
#include <raymath.h>

void Player_OnDeath(world_t *world, entity_t e) { printf("player dead\n"); }

void Grunt_OnDeath(world_t *world, entity_t entity) {
  /* -------- NavPath -------- */
  NavPath *nav = ECS_GET(world, entity, NavPath, COMP_NAVPATH);

  if (nav && nav->points) {
    NavPath_Destroy(nav);
  }

  /* -------- Muzzles -------- */
  MuzzleCollection_t *m =
      ECS_GET(world, entity, MuzzleCollection_t, COMP_MUZZLES);

  if (m && m->Muzzles) {
    free(m->Muzzles);
    m->Muzzles = NULL;
    m->count = 0;
  }

  /* -------- Model Collection -------- */
  ModelCollection_t *mc = ECS_GET(world, entity, ModelCollection_t, COMP_MODEL);

  if (mc) {
    ModelCollectionFree(mc);
  }
}

void Trigger_OnCollision(world_t *world, entity_t self, entity_t other) {
  printf("trigger collided\n");
}

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
                      COMP_HEALTH,
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
                      COMP_DASHCOOLDOWN,
                      COMP_ONDEATH,
                      COMP_TYPE_PLAYER};

  uint32_t id = CreateArchetype(world, comps, sizeof(comps) / sizeof(uint32_t));

  archetype_t *arch = WorldGetArchetype(world, id);
  arch->id = id;

  /* ---------- Inline Components ---------- */

  ArchetypeAddInline(arch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(arch, COMP_VELOCITY, sizeof(Velocity));
  ArchetypeAddInline(arch, COMP_ORIENTATION, sizeof(Orientation));
  ArchetypeAddInline(arch, COMP_HEALTH, sizeof(Health));
  ArchetypeAddInline(arch, COMP_ACTIVE, sizeof(Active));
  ArchetypeAddInline(arch, COMP_COLLISION_INSTANCE, sizeof(CollisionInstance));
  ArchetypeAddInline(arch, COMP_CAPSULE_COLLIDER, sizeof(CapsuleCollider));
  ArchetypeAddInline(arch, COMP_ISGROUNDED, sizeof(bool));
  ArchetypeAddInline(arch, COMP_ISDASHING, sizeof(bool));
  ArchetypeAddInline(arch, COMP_MUZZLES, sizeof(MuzzleCollection_t));
  ArchetypeAddInline(arch, COMP_ONDEATH, sizeof(OnDeath));

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
                      COMP_MUZZLES,          COMP_MOVE_TIMER,
                      COMP_ONDEATH,          COMP_COMBAT_STATE,
                      COMP_TYPE_GRUNT};

  uint32_t id = CreateArchetype(world, comps, sizeof(comps) / sizeof(uint32_t));

  archetype_t *arch = WorldGetArchetype(world, id);
  arch->id = id;

  ArchetypeAddInline(arch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(arch, COMP_VELOCITY, sizeof(Velocity));
  ArchetypeAddInline(arch, COMP_ORIENTATION, sizeof(Orientation));
  ArchetypeAddInline(arch, COMP_ACTIVE, sizeof(Active));
  ArchetypeAddInline(arch, COMP_HEALTH, sizeof(Health));
  ArchetypeAddInline(arch, COMP_COLLISION_INSTANCE, sizeof(CollisionInstance));
  ArchetypeAddInline(arch, COMP_NAVPATH, sizeof(NavPath));
  ArchetypeAddInline(arch, COMP_CAPSULE_COLLIDER, sizeof(CapsuleCollider));
  ArchetypeAddInline(arch, COMP_MUZZLES, sizeof(MuzzleCollection_t));
  ArchetypeAddInline(arch, COMP_ONDEATH, sizeof(OnDeath));
  ArchetypeAddInline(arch, COMP_COMBAT_STATE, sizeof(CombatState_t));

  ArchetypeAddHandle(arch, COMP_MODEL, &engine->modelPool);
  ArchetypeAddHandle(arch, COMP_GRUNT_FIRE_TIMER, &engine->timerPool);
  ArchetypeAddHandle(arch, COMP_MOVE_TIMER, &engine->timerPool);

  return id;
}

// MESSAGE ARCHETYPE
uint32_t RegisterTriggerArchetype(world_t *world, Engine *engine) {
  uint32_t comps[] = {COMP_POSITION, COMP_ACTIVE, COMP_COLLISION_INSTANCE,
                      COMP_AABB_COLLIDER, COMP_ONCOLLISION};

  uint32_t id = CreateArchetype(world, comps, sizeof(comps) / sizeof(uint32_t));

  archetype_t *arch = WorldGetArchetype(world, id);
  arch->id = id;

  ArchetypeAddInline(arch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(arch, COMP_ACTIVE, sizeof(Active));
  ArchetypeAddInline(arch, COMP_COLLISION_INSTANCE, sizeof(CollisionInstance));
  ArchetypeAddInline(arch, COMP_AABB_COLLIDER, sizeof(AABBCollider));
  ArchetypeAddInline(arch, COMP_ONCOLLISION, sizeof(OnCollision));

  return id;
}

// BOX ARCHETYPE

uint32_t RegisterBoxArchetype(world_t *world, Engine *engine) {
  uint32_t comps[] = {
      COMP_POSITION, COMP_ORIENTATION,        COMP_MODEL,
      COMP_ACTIVE,   COMP_COLLISION_INSTANCE, COMP_AABB_COLLIDER};

  uint32_t id = CreateArchetype(world, comps, sizeof(comps) / sizeof(uint32_t));

  archetype_t *arch = WorldGetArchetype(world, id);
  arch->id = id;

  ArchetypeAddInline(arch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(arch, COMP_ORIENTATION, sizeof(Orientation));
  ArchetypeAddInline(arch, COMP_ACTIVE, sizeof(Active));
  ArchetypeAddInline(arch, COMP_COLLISION_INSTANCE, sizeof(CollisionInstance));
  ArchetypeAddInline(arch, COMP_AABB_COLLIDER, sizeof(AABBCollider));

  ArchetypeAddHandle(arch, COMP_MODEL, &engine->modelPool);

  return id;
}

uint32_t RegisterEnemyRangerArchetype(world_t *world, Engine *engine) {
  uint32_t comps[] = {COMP_POSITION,         COMP_VELOCITY,
                      COMP_ORIENTATION,      COMP_MODEL,
                      COMP_ACTIVE,           COMP_COLLISION_INSTANCE,
                      COMP_CAPSULE_COLLIDER, COMP_HEALTH,
                      COMP_NAVPATH,          COMP_GRUNT_FIRE_TIMER,
                      COMP_MUZZLES,          COMP_MOVE_TIMER,
                      COMP_ONDEATH,          COMP_COMBAT_STATE,
                      COMP_TYPE_RANGER};

  uint32_t id = CreateArchetype(world, comps, sizeof(comps) / sizeof(uint32_t));

  archetype_t *arch = WorldGetArchetype(world, id);
  arch->id = id;

  ArchetypeAddInline(arch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(arch, COMP_VELOCITY, sizeof(Velocity));
  ArchetypeAddInline(arch, COMP_ORIENTATION, sizeof(Orientation));
  ArchetypeAddInline(arch, COMP_ACTIVE, sizeof(Active));
  ArchetypeAddInline(arch, COMP_HEALTH, sizeof(Health));
  ArchetypeAddInline(arch, COMP_COLLISION_INSTANCE, sizeof(CollisionInstance));
  ArchetypeAddInline(arch, COMP_NAVPATH, sizeof(NavPath));
  ArchetypeAddInline(arch, COMP_CAPSULE_COLLIDER, sizeof(CapsuleCollider));
  ArchetypeAddInline(arch, COMP_MUZZLES, sizeof(MuzzleCollection_t));
  ArchetypeAddInline(arch, COMP_ONDEATH, sizeof(OnDeath));
  ArchetypeAddInline(arch, COMP_COMBAT_STATE, sizeof(CombatState_t));

  ArchetypeAddHandle(arch, COMP_MODEL, &engine->modelPool);
  ArchetypeAddHandle(arch, COMP_GRUNT_FIRE_TIMER, &engine->timerPool);
  ArchetypeAddHandle(arch, COMP_MOVE_TIMER, &engine->timerPool);

  return id;
}

// FACTORY SPAWN FUNCTIONS

entity_t SpawnPlayer(world_t *world, GameWorld *gw, Vector3 position) {
  archetype_t *arch = WorldGetArchetype(world, gw->playerArchId);
  entity_t e = WorldCreateEntity(world, &arch->mask);

  ECS_GET(world, e, Position, COMP_POSITION)->value = position;
  ECS_GET(world, e, Active, COMP_ACTIVE)->value = true;
  ECS_GET(world, e, Health, COMP_HEALTH)->current = 100;
  ECS_GET(world, e, Health, COMP_HEALTH)->max = 100;

  /* ---------------- Model Setup ---------------- */

  ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);

  ModelCollectionInit(mc, 2);

  Model playerBody = LoadModelFromMesh(GenMeshCube(.2f, 2, .2f));

  ModelCollectionInit(mc, 3); // body + 2 guns

  // Body (index 0)
  ModelCollectionAdd(mc, (ModelInstance_t){.model = playerBody,
                                           .offset = (Vector3){0, -1.5f, 0},
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_YAW_ONLY,
                                           .isActive = true});

  // Gun 1 (index 1)
  ModelCollectionAdd(mc, (ModelInstance_t){.model = gw->gunModel,
                                           .offset = (Vector3){0, -0.5f, 0},
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_FULL,
                                           .isActive = true});

  // Gun 2 (index 2)
  ModelCollectionAdd(mc, (ModelInstance_t){.model = gw->gunModel,
                                           .offset = (Vector3){0, -0.5f, 0},
                                           .scale = (Vector3){1, 1, 2},
                                           .rotationMode = MODEL_ROT_FULL,
                                           .isActive = false});

  /* ---------------- Collision ---------------- */

  CapsuleCollider *cap =
      ECS_GET(world, e, CapsuleCollider, COMP_CAPSULE_COLLIDER);

  cap->radius = 0.35f;
  cap->localA = (Vector3){0, -1.55f, 0};
  cap->localB = (Vector3){0, 0, 0};

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
  muzzles->Muzzles = calloc(1, sizeof(Muzzle_t));

  muzzles->Muzzles[0] =
      (Muzzle_t){.positionOffset = {.value = {0.25f, -0.3f, 1.5f}},
                 .weaponOffset = {0, -PI / 2},
                 .bulletType = BULLET_TYPE_AUTOCANNON};

  OnDeath *od = ECS_GET(world, e, OnDeath, COMP_ONDEATH);
  od->fn = Player_OnDeath;

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
                                           .rotationMode = MODEL_ROT_YAW_ONLY,
                                           .isActive = true});

  ModelCollectionAdd(mc, (ModelInstance_t){.model = game->gruntTorso,
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_FULL,
                                           .isActive = true});

  ModelCollectionAdd(mc, (ModelInstance_t){.model = game->gruntGun,
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_FULL,
                                           .isActive = true});

  /* -------- Collider -------- */

  CapsuleCollider *cap =
      ECS_GET(world, e, CapsuleCollider, COMP_CAPSULE_COLLIDER);

  cap->radius = 1.2f;
  cap->localA = (Vector3){0, 0.0f, 0};
  cap->localB = (Vector3){0, 2.5f, 0};

  Capsule_UpdateWorld(cap, position);

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
  muzzles->Muzzles = calloc(1, sizeof(Muzzle_t));

  muzzles->Muzzles[0] =
      (Muzzle_t){.positionOffset = {.value = {0.0f, 3.0f, 1.5f}},
                 .bulletType = BULLET_TYPE_STANDARD};

  CombatState_t *combat = ECS_GET(world, e, CombatState_t, COMP_COMBAT_STATE);
  if (combat) {
    combat->combatYaw = PI / 4;
    combat->aimPitch = 0.0f;
    combat->moveYaw = PI / 4;
    combat->isAiming = false;
    combat->state = ENEMY_STATE_MOVING;
  }

  OnDeath *od = ECS_GET(world, e, OnDeath, COMP_ONDEATH);
  od->fn = Grunt_OnDeath;

  return e;
}

entity_t SpawnEnemyRanger(world_t *world, GameWorld *game, Vector3 position) {
  printf("SpawnEnemyRanger called\n");
  archetype_t *arch = WorldGetArchetype(world, game->enemyRangerArchId);
  entity_t e = WorldCreateEntity(world, &arch->mask);

  ECS_GET(world, e, Position, COMP_POSITION)->value = position;
  ECS_GET(world, e, Active, COMP_ACTIVE)->value = true;

  Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
  ori->yaw = 0;
  ori->pitch = 0;

  Health *hp = ECS_GET(world, e, Health, COMP_HEALTH);
  hp->max = 200;
  hp->current = 200;

  /* --- Model --- */
  ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);

  ModelCollectionInit(mc, 3);

  ModelCollectionAdd(mc, (ModelInstance_t){.model = game->gruntLegs,
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_YAW_ONLY,
                                           .isActive = true});

  ModelCollectionAdd(mc, (ModelInstance_t){.model = game->gruntTorso,
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_FULL,
                                           .isActive = true});

  ModelCollectionAdd(mc, (ModelInstance_t){.model = game->gruntGun,
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_FULL,
                                           .isActive = true});

  /* --- Collider --- */
  CapsuleCollider *cap =
      ECS_GET(world, e, CapsuleCollider, COMP_CAPSULE_COLLIDER);
  cap->radius = 1.5f;
  cap->localA = (Vector3){0, 0, 0};
  cap->localB = (Vector3){0, 3.0f, 0};
  Capsule_UpdateWorld(cap, position);

  CollisionInstance *ci =
      ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);
  ci->owner = e;
  ci->type = COLLIDER_CAPSULE;
  ci->layerMask = 1 << LAYER_ENEMY;
  ci->collideMask = 1 << LAYER_BULLET;

  /* --- Timers --- */
  Timer *fire = ECS_GET(world, e, Timer, COMP_GRUNT_FIRE_TIMER);
  fire->value = 2.0f;

  Timer *moveTimer = ECS_GET(world, e, Timer, COMP_MOVE_TIMER);
  moveTimer->value = 0;

  /* --- Nav --- */
  NavPath *nav = ECS_GET(world, e, NavPath, COMP_NAVPATH);
  NavPath_Init(nav, 32);

  /* --- Muzzles (2) --- */
  MuzzleCollection_t *muzzles =
      ECS_GET(world, e, MuzzleCollection_t, COMP_MUZZLES);

  muzzles->count = 2;
  muzzles->Muzzles = calloc(2, sizeof(Muzzle_t));
  // Gun
  muzzles->Muzzles[0] = (Muzzle_t){.positionOffset = {.value = {0, 3.0f, 1.5f}},
                                   .bulletType = BULLET_TYPE_STANDARD};

  // Missile
  muzzles->Muzzles[1] = (Muzzle_t){.positionOffset = {.value = {0, 4.5f, -0.5}},
                                   .bulletType = BULLET_TYPE_MISSILE};

  muzzles->Muzzles[1].worldPosition.y += 2;

  CombatState_t *combat = ECS_GET(world, e, CombatState_t, COMP_COMBAT_STATE);
  if (combat) {
    combat->combatYaw = PI / 4;
    combat->aimPitch = 0.0f;
    combat->moveYaw = PI / 4;
    combat->isAiming = false;
    combat->state = ENEMY_STATE_MOVING;
  }

  printf("Ranger position: %f %f %f\n", position.x, position.y, position.z);
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
                                           .rotationMode = MODEL_ROT_YAW_ONLY,
                                           .isActive = true});

  /* -------- Collider -------- */

  CapsuleCollider *cap =
      ECS_GET(world, e, CapsuleCollider, COMP_CAPSULE_COLLIDER);

  cap->radius = 1.5f;
  cap->localA = Vector3Add(position, (Vector3){0, 0.5f, 0});
  cap->localB = Vector3Add(position, (Vector3){0, 1.0f, 0});

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
  muzzles->Muzzles = calloc(1, sizeof(Muzzle_t));

  muzzles->Muzzles[0] =
      (Muzzle_t){.positionOffset = {.value = {0.0f, 3.0f, 0.0f}},
                 .bulletType = BULLET_TYPE_STANDARD};

  ECS_GET(world, e, CombatState_t, COMP_COMBAT_STATE)->combatYaw = 0;
  ECS_GET(world, e, CombatState_t, COMP_COMBAT_STATE)->moveYaw = 0;
  ECS_GET(world, e, CombatState_t, COMP_COMBAT_STATE)->isAiming = false;
  ECS_GET(world, e, CombatState_t, COMP_COMBAT_STATE)->aimPitch = 0;

  return e;
}

// tutorial box

entity_t SpawnTrigger(world_t *world, uint32_t triggerArchId, Vector3 position,
                      Vector3 size) {
  archetype_t *arch = WorldGetArchetype(world, triggerArchId);

  entity_t e = WorldCreateEntity(world, &arch->mask);

  ECS_GET(world, e, Position, COMP_POSITION)->value = position;
  ECS_GET(world, e, Active, COMP_ACTIVE)->value = true;

  AABBCollider *aabb = ECS_GET(world, e, AABBCollider, COMP_AABB_COLLIDER);

  aabb->halfExtents = Vector3Scale(size, 0.5f);

  CollisionInstance *ci =
      ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);

  ci->owner = e;
  ci->type = COLLIDER_AABB;
  ci->layerMask = 1 << LAYER_TRIGGER; // define if needed
  ci->collideMask = 0xFFFFFFFF;       // detect everything

  Position *pos = ECS_GET(world, e, Position, COMP_POSITION);

  ci->worldBounds.min = Vector3Subtract(pos->value, aabb->halfExtents);
  ci->worldBounds.max = Vector3Add(pos->value, aabb->halfExtents);

  OnCollision *oc = ECS_GET(world, e, OnCollision, COMP_ONCOLLISION);

  oc->fn = Trigger_OnCollision;

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
                                           .rotationMode = MODEL_ROT_FULL,
                                           .isActive = true});

  /* -------- Collider -------- */

  AABBCollider *aabb = ECS_GET(world, e, AABBCollider, COMP_AABB_COLLIDER);
  aabb->halfExtents = (Vector3){size.x * 0.5f, size.y * 0.5f, size.z * 0.5f};

  CollisionInstance *ci =
      ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);

  ci->owner = e;
  ci->type = COLLIDER_AABB;
  ci->layerMask = 1 << LAYER_WORLD;
  ci->collideMask = (1 << LAYER_PLAYER) | (1 << LAYER_BULLET);

  Position *pos = ECS_GET(world, e, Position, COMP_POSITION);

  ci->worldBounds.min = Vector3Subtract(pos->value, aabb->halfExtents);
  ci->worldBounds.max = Vector3Add(pos->value, aabb->halfExtents);
  return e;
}

entity_t SpawnLevelModel(world_t *world, GameWorld *gw, Model model,
                         Vector3 position, Vector3 rotation, Vector3 scale) {
  archetype_t *arch = WorldGetArchetype(world, gw->levelModelArchId);

  entity_t e = WorldCreateEntity(world, &arch->mask);

  /* -------- Transform -------- */

  ECS_GET(world, e, Position, COMP_POSITION)->value = position;
  Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
  ori->yaw = 0.0f;
  ori->pitch = 0.0f;

  Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
  active->value = true;

  /* -------- Model -------- */

  ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);

  ModelCollectionInit(mc, 1);

  ModelCollectionAdd(
      mc, (ModelInstance_t){.model = model,
                            .offset = (Vector3){0, 0, 0},
                            .rotation = (Vector3){rotation.y, rotation.x, 0},
                            .scale = scale,
                            .rotationMode = MODEL_ROT_FULL,
                            .isActive = true});

  return e;
}
