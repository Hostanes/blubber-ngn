
#include "level_creater_helper.h"
#include "components/components.h"
#include "components/muzzle.h"
#include "components/renderable.h"
#include "ecs_get.h"
#include <raymath.h>
#include <string.h>

static GameWorld *s_game = NULL;

void LevelHelper_SetGame(GameWorld *game) { s_game = game; }

void SpawnCoolant(world_t *world, GameWorld *game, Vector3 pos) {
  archetype_t *arch = WorldGetArchetype(world, game->coolantArchId);
  if (!arch) return;
  for (uint32_t i = 0; i < arch->count; i++) {
    entity_t e = arch->entities[i];
    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || active->value) continue;

    active->value = true;
    ECS_GET(world, e, Position, COMP_POSITION)->value = pos;

    float vx = GetRandomValue(-150, 150) / 100.0f;
    float vy = GetRandomValue(1200, 1600) / 100.0f;
    float vz = GetRandomValue(-150, 150) / 100.0f;
    ECS_GET(world, e, Velocity, COMP_VELOCITY)->value = (Vector3){vx, vy, vz};

    Coolant *co = ECS_GET(world, e, Coolant, COMP_COOLANT);
    co->lifetime      = 2.0f;
    co->particleTimer = 0.0f;
    return;
  }
}

void SpawnHealthOrb(world_t *world, GameWorld *game, Vector3 pos) {
  archetype_t *arch = WorldGetArchetype(world, game->healthOrbArchId);
  if (!arch) return;
  for (uint32_t i = 0; i < arch->count; i++) {
    entity_t e = arch->entities[i];
    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || active->value) continue;

    active->value = true;
    ECS_GET(world, e, Position, COMP_POSITION)->value = pos;

    float vx = GetRandomValue(-150, 150) / 100.0f;
    float vy = GetRandomValue(1200, 1600) / 100.0f;
    float vz = GetRandomValue(-150, 150) / 100.0f;
    ECS_GET(world, e, Velocity, COMP_VELOCITY)->value = (Vector3){vx, vy, vz};

    HealthOrb *ho = ECS_GET(world, e, HealthOrb, COMP_HEALTH_ORB);
    ho->lifetime      = 2.5f;
    ho->particleTimer = 0.0f;
    return;
  }
}

void Player_OnDeath(world_t *world, entity_t entity) {
  printf("killing player\n");
  MuzzleCollection_t *m =
      ECS_GET(world, entity, MuzzleCollection_t, COMP_MUZZLES);

  if (m && m->Muzzles) {
    free(m->Muzzles);
    m->Muzzles = NULL;
    m->count = 0;
  }

  ModelCollection_t *mc = ECS_GET(world, entity, ModelCollection_t, COMP_MODEL);

  if (mc) {
    ModelCollectionFree(mc);
  }
}

void Grunt_OnDeath(world_t *world, entity_t entity) {
  printf("killing grunt\n");
  NavPath *nav = ECS_GET(world, entity, NavPath, COMP_NAVPATH);

  if (nav && nav->points) {
    NavPath_Destroy(nav);
  }

  MuzzleCollection_t *m =
      ECS_GET(world, entity, MuzzleCollection_t, COMP_MUZZLES);

  if (m && m->Muzzles) {
    free(m->Muzzles);
    m->Muzzles = NULL;
    m->count = 0;
  }

  ModelCollection_t *mc = ECS_GET(world, entity, ModelCollection_t, COMP_MODEL);
  if (mc) {
    ModelCollectionFree(mc);
  }

  if (s_game) {
    Position *pos = ECS_GET(world, entity, Position, COMP_POSITION);
    if (pos) {
      for (int i = 0; i < 3; i++)
        SpawnCoolant(world, s_game, pos->value);
      if (GetRandomValue(0, 99) < 30)
        SpawnHealthOrb(world, s_game, pos->value);
    }
  }
}

void Ranger_OnDeath(world_t *world, entity_t entity) {
  printf("killing ranger\n");
  NavPath *nav = ECS_GET(world, entity, NavPath, COMP_NAVPATH);

  if (nav && nav->points) {
    NavPath_Destroy(nav);
  }

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

  if (s_game) {
    Position *pos = ECS_GET(world, entity, Position, COMP_POSITION);
    if (pos) {
      for (int i = 0; i < 3; i++)
        SpawnCoolant(world, s_game, pos->value);
      if (GetRandomValue(0, 99) < 45)
        SpawnHealthOrb(world, s_game, pos->value);
    }
  }
}

void OnMissileDeath(world_t *world, entity_t e) {

  Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
  if (!active || !active->value)
    return;

  active->value = false;

  Velocity *vel = ECS_GET(world, e, Velocity, COMP_VELOCITY);
  if (vel)
    vel->value = (Vector3){0, 0, 0};

  CollisionInstance *ci =
      ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);
  if (ci) {
    ci->layerMask = 0;
    ci->collideMask = 0;
  }

  Timer *life = ECS_GET(world, e, Timer, COMP_TIMER);
  if (life)
    life->value = 0.0f;

  ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);
  if (mc && mc->count > 0)
    mc->models[0].isActive = false;
}

void Trigger_OnCollision(world_t *world, entity_t self, entity_t other) {
  printf("trigger collided\n");
}

entity_t SpawnPlayer(world_t *world, GameWorld *gw, Vector3 position) {
  archetype_t *arch = WorldGetArchetype(world, gw->playerArchId);
  entity_t e = WorldCreateEntity(world, &arch->mask);

  ECS_GET(world, e, Position, COMP_POSITION)->value = position;
  ECS_GET(world, e, Active, COMP_ACTIVE)->value = true;
  ECS_GET(world, e, Health, COMP_HEALTH)->current = 100;
  ECS_GET(world, e, Health, COMP_HEALTH)->max = 100;
  ECS_GET(world, e, Shield, COMP_SHIELD)->current = 0.0f;
  ECS_GET(world, e, Shield, COMP_SHIELD)->max = 0.0f;

  /* ---------------- Model Setup ---------------- */

  ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);

  ModelCollectionInit(mc, 2);

  Model playerBody = LoadModelFromMesh(GenMeshCube(.05f, .05f, .05f));

  ModelCollectionInit(mc, 5); // body + 4 guns

  // Body (index 0)
  ModelCollectionAdd(mc, (ModelInstance_t){.model = playerBody,
                                           .offset = (Vector3){0, -1.5f, 0},
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_YAW_ONLY,
                                           .parentIndex = -1,
                                           .isActive = true});

  // Gun 1 (index 1)
  ModelCollectionAdd(mc, (ModelInstance_t){.model = gw->gunModel,
                                           .offset = (Vector3){0, -0.5f, 0},
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_FULL,
                                           .parentIndex = -1,
                                           .isActive = true});

  // Gun 2 (index 2)
  ModelCollectionAdd(mc, (ModelInstance_t){.model = gw->plasmaGunModel,
                                           .offset = (Vector3){0, -0.5f, 0},
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_FULL,
                                           .parentIndex = -1,
                                           .isActive = false});

  // Gun 3 — rocket launcher (index 3)
  ModelCollectionAdd(mc, (ModelInstance_t){.model = gw->rocketLauncherModel,
                                           .offset = (Vector3){0, -0.5f, 0},
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_FULL,
                                           .parentIndex = -1,
                                           .isActive = false});

  // Gun 4 — blunderbuss (index 4)
  ModelCollectionAdd(mc, (ModelInstance_t){.model = gw->blunderbussModel,
                                           .offset = (Vector3){0, -0.5f, 0},
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_FULL,
                                           .parentIndex = -1,
                                           .isActive = false});

  /* ---------------- Collision ---------------- */

  CapsuleCollider *cap =
      ECS_GET(world, e, CapsuleCollider, COMP_CAPSULE_COLLIDER);

  cap->radius = 0.5f;
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

  muzzles->count = 4;
  muzzles->Muzzles = calloc(4, sizeof(Muzzle_t));

  // Weapon 1: anti-health machine gun — low heat per shot, fast cooldown
  muzzles->Muzzles[0] = (Muzzle_t){
      .positionOffset = {.value = {0.25f, -0.3f, 1.5f}},
      .bulletType = BULLET_TYPE_STANDARD,
      .shieldMult = 0.1f,
      .healthMult = 2.5f,
      .pierce = false,
      .spreadCount = 1,
      .spreadAngle = 0.0f,
      .fireRate = 3.0f,
      .heatPerShot = 0.12f,
      .coolRate = 0.20f,
      .coolRateOverheated = 0.12f,
      .overheatThreshold = 0.40f,
      .coolDelay = 1.2f,
  };

  // Weapon 2: anti-shield plasma — high heat per shot, slow cooldown
  muzzles->Muzzles[1] = (Muzzle_t){
      .positionOffset = {.value = {0.25f, -0.3f, 1.5f}},
      .bulletType = BULLET_TYPE_PLASMA,
      .shieldMult = 2.5f,
      .healthMult = 0.5f,
      .pierce = false,
      .spreadCount = 1,
      .spreadAngle = 0.05f,
      .fireRate = 15.0f,
      .heatPerShot = 0.04f,
      .coolRate = 0.25f,
      .coolRateOverheated = 0.12f,
      .overheatThreshold = 0.40f,
      .coolDelay = 0.8f,
  };

  // Weapon 3: rocket launcher — handled by RocketLauncherSystem, not PlayerShootSystem
  muzzles->Muzzles[2] = (Muzzle_t){
      .positionOffset      = {.value = {0.25f, -0.3f, 1.5f}},
      .bulletType          = BULLET_TYPE_MISSILE,
      .shieldMult          = 1.0f,
      .healthMult          = 1.0f,
      .fireRate            = 0.0f,
      .heatPerShot         = 0.2f,   // 3 missiles × 0.2 = 0.6 per burst
      .coolRate            = 0.04f,  // very slow dissipation
      .coolRateOverheated  = 0.02f,
      .overheatThreshold   = 0.40f,
      .coolDelay           = 2.0f,
  };

  // Weapon 4: blunderbuss — tight buckshot, handled by BlunderbussSystem
  muzzles->Muzzles[3] = (Muzzle_t){
      .positionOffset = {.value = {0.25f, -0.3f, 1.5f}},
      .bulletType     = BULLET_TYPE_BUCKSHOT,
      .shieldMult     = 0.4f,
      .healthMult     = 2.0f,
      .pierce         = false,
      .spreadCount    = 10,
      .spreadAngle    = 0.14f,
      .fireRate       = 0.0f,
      .heatPerShot    = 0.45f,
      .coolRate       = 0.18f,
      .coolRateOverheated = 0.08f,
      .overheatThreshold  = 0.40f,
      .coolDelay      = 1.5f,
  };

  OnDeath *od = ECS_GET(world, e, OnDeath, COMP_ONDEATH);
  od->fn = Player_OnDeath;

  return e;
}

// ---------------- Enemy Grunt ----------------

entity_t SpawnEnemyGrunt(world_t *world, GameWorld *game, Vector3 position) {
  archetype_t *arch = WorldGetArchetype(world, game->enemyGruntArchId);

  entity_t e = WorldCreateEntity(world, &arch->mask);

  position.y = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap,
                                             position.x, position.z);

  /* -------- Basic State -------- */

  ECS_GET(world, e, Position, COMP_POSITION)->value = position;
  ECS_GET(world, e, Active, COMP_ACTIVE)->value = true;

  Health *hp = ECS_GET(world, e, Health, COMP_HEALTH);
  hp->max = 150.0f;
  hp->current = 150.0f;

  Shield *sh = ECS_GET(world, e, Shield, COMP_SHIELD);
  sh->max = 50.0f;
  sh->current = 50.0f;

  Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
  ori->yaw = PI / 4;
  ori->pitch = 0.0f;

  /* -------- Model -------- */

  ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);

  ModelCollectionInit(mc, 3);

  ModelCollectionAdd(mc, (ModelInstance_t){.model = game->gruntLegs,
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_YAW_ONLY,
                                           .parentIndex = -1,
                                           .isActive = true});

  ModelCollectionAdd(mc, (ModelInstance_t){.model = game->gruntTorso,
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_FULL,
                                           .parentIndex = -1,
                                           .isActive = true});

  ModelCollectionAdd(mc, (ModelInstance_t){.model = game->gruntGun,
                                           .scale = (Vector3){1, 1, 1},
                                           .pivot = (Vector3){0, 3, 0},
                                           .rotationMode = MODEL_ROT_FULL,
                                           .parentIndex = -1,
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
                 .bulletType = BULLET_TYPE_ENEMY};

  CombatState_t *combat = ECS_GET(world, e, CombatState_t, COMP_COMBAT_STATE);
  if (combat) {
    combat->combatYaw            = PI / 4;
    combat->aimPitch             = 0.0f;
    combat->moveYaw              = PI / 4;
    combat->isAiming             = false;
    combat->state                = ENEMY_AI_SUPPRESS;
    combat->settleTimer          = 1.0f + GetRandomValue(0, 10) * 0.1f;
    combat->pathPending          = false;
    combat->burstShotsRemaining  = 0;
    combat->burstTimer           = 0.0f;
    combat->burstType            = 0;
    combat->claimedCX            = -1;
    combat->claimedCY            = -1;
    combat->repositionTimer      = 0.5f + GetRandomValue(0, 20) * 0.1f;
    combat->losCheckTimer        = 0.0f;
    combat->hasLOS               = true;
  }

  OnDeath *od = ECS_GET(world, e, OnDeath, COMP_ONDEATH);
  od->fn = Grunt_OnDeath;

  return e;
}

entity_t SpawnEnemyRanger(world_t *world, GameWorld *game, Vector3 position) {
  printf("SpawnEnemyRanger called\n");
  archetype_t *arch = WorldGetArchetype(world, game->enemyRangerArchId);
  entity_t e = WorldCreateEntity(world, &arch->mask);

  position.y = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap,
                                             position.x, position.z);

  ECS_GET(world, e, Position, COMP_POSITION)->value = position;
  ECS_GET(world, e, Active, COMP_ACTIVE)->value = true;

  Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
  ori->yaw = 0;
  ori->pitch = 0;

  Health *hp = ECS_GET(world, e, Health, COMP_HEALTH);
  hp->max = 200;
  hp->current = 200;

  Shield *sh = ECS_GET(world, e, Shield, COMP_SHIELD);
  sh->max = 100.0f;
  sh->current = 100.0f;

  /* --- Model --- */
  ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);

  ModelCollectionInit(mc, 3);

  ModelCollectionAdd(mc, (ModelInstance_t){.model = game->rangerLegs,
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_YAW_ONLY,
                                           .parentIndex = -1,
                                           .isActive = true});

  ModelCollectionAdd(mc, (ModelInstance_t){.model = game->rangerTorso,
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_FULL,
                                           .parentIndex = -1,
                                           .isActive = true});

  ModelCollectionAdd(mc, (ModelInstance_t){.model = game->gruntGun,
                                           .scale = (Vector3){1, 1, 1},
                                           .pivot = (Vector3){0, 3, 0},
                                           .rotationMode = MODEL_ROT_FULL,
                                           .parentIndex = -1,
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
                                   .bulletType = BULLET_TYPE_ENEMY};

  // Missile
  muzzles->Muzzles[1] = (Muzzle_t){.positionOffset = {.value = {0, 4.5f, -0.5}},
                                   .bulletType = BULLET_TYPE_MISSILE};

  muzzles->Muzzles[1].worldPosition.y += 2;

  CombatState_t *combat = ECS_GET(world, e, CombatState_t, COMP_COMBAT_STATE);
  if (combat) {
    combat->combatYaw            = PI / 4;
    combat->aimPitch             = 0.0f;
    combat->moveYaw              = PI / 4;
    combat->isAiming             = false;
    combat->state                = ENEMY_AI_ADVANCE;
    combat->settleTimer          = 0.5f;
    combat->pathPending          = false;
    combat->burstShotsRemaining  = 0;
    combat->burstTimer           = 0.0f;
    combat->burstType            = 0;
    combat->claimedCX            = -1;
    combat->claimedCY            = -1;
    combat->repositionTimer      = GetRandomValue(5, 15) * 0.1f;
    combat->losCheckTimer        = 0.0f;
    combat->hasLOS               = true;
  }

  OnDeath *od = ECS_GET(world, e, OnDeath, COMP_ONDEATH);
  od->fn = Ranger_OnDeath;
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
                                           .parentIndex = -1,
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
                 .bulletType = BULLET_TYPE_ENEMY};

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

  ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);

  ModelCollectionInit(mc, 1);

  ModelCollectionAdd(mc, (ModelInstance_t){.model = gw->obstacleModel,
                                           .offset = (Vector3){0, 0, 0},
                                           .rotation = (Vector3){0, 0, 0},
                                           .scale = size,
                                           .rotationMode = MODEL_ROT_FULL,
                                           .parentIndex = -1,
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
  Collision_UpdateAABB(ci, aabb, position);
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
                            .parentIndex = -1,
                            .isActive = true});

  return e;
}

entity_t SpawnProp(world_t *world, GameWorld *gw, Model model, Vector3 position,
                   float yaw, Vector3 scale) {
  archetype_t *arch = WorldGetArchetype(world, gw->levelModelArchId);
  entity_t e = WorldCreateEntity(world, &arch->mask);

  ECS_GET(world, e, Position, COMP_POSITION)->value = position;
  Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
  ori->yaw = yaw;
  ori->pitch = 0.0f;
  ECS_GET(world, e, Active, COMP_ACTIVE)->value = true;

  ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);
  ModelCollectionInit(mc, 1);
  ModelCollectionAdd(mc, (ModelInstance_t){
                             .model = model,
                             .offset = (Vector3){0, 0, 0},
                             .rotation = (Vector3){0, 0, 0},
                             .scale = scale,
                             .rotationMode = MODEL_ROT_YAW_ONLY,
                             .parentIndex = -1,
                             .isActive = true,
                         });
  return e;
}

void SpawnHomingMissile(world_t *world, GameWorld *game, entity_t shooter,
                        entity_t target, Vector3 position, Vector3 forward,
                        bool guided, float turnSpeed) {

  archetype_t *arch = WorldGetArchetype(world, game->missileArchId);
  entity_t m = WorldCreateEntity(world, &arch->mask);

  ECS_GET(world, m, Active, COMP_ACTIVE)->value = true;

  /* --- Transform --- */
  ECS_GET(world, m, Position, COMP_POSITION)->value = position;

  float speed = 20.0f;

  ECS_GET(world, m, Velocity, COMP_VELOCITY)->value =
      Vector3Scale(forward, speed);

  Orientation *ori = ECS_GET(world, m, Orientation, COMP_ORIENTATION);
  ori->yaw = atan2f(forward.x, forward.z);
  ori->pitch = asinf(forward.y);

  /* --- Model --- */
  ModelCollection_t *mc = ECS_GET(world, m, ModelCollection_t, COMP_MODEL);

  ModelCollectionInit(mc, 1);
  ModelCollectionAdd(mc, (ModelInstance_t){.model = game->missileModel,
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_FULL,
                                           .parentIndex = -1,
                                           .isActive = true});

  mc->models[0].rotation = (Vector3){-ori->pitch, 0.0f, 0.0f};

  /* --- Homing Data --- */
  HomingMissile *hm = ECS_GET(world, m, HomingMissile, COMP_HOMINGMISSILE);
  hm->owner       = shooter;
  hm->target      = target;
  hm->turnSpeed   = turnSpeed;
  hm->maxSpeed    = 50.0f;
  hm->blastDamage = (shooter.id == game->player.id) ? 80.0f : 28.0f;
  hm->armed       = false;
  hm->guided      = guided;

  /* --- Lifetime --- */
  Timer *life = ECS_GET(world, m, Timer, COMP_TIMER);
  life->value = 15.0f;

  /* --- Collider --- */
  SphereCollider *sphere =
      ECS_GET(world, m, SphereCollider, COMP_SPHERE_COLLIDER);

  sphere->radius = 0.35f;
  sphere->center = position;

  CollisionInstance *ci =
      ECS_GET(world, m, CollisionInstance, COMP_COLLISION_INSTANCE);

  ci->owner = m;
  ci->type = COLLIDER_SPHERE;
  ci->layerMask = 1 << LAYER_BULLET;
  ci->collideMask =
      (1 << LAYER_PLAYER) | (1 << LAYER_ENEMY) | (1 << LAYER_WORLD);

  OnDeath *od = ECS_GET(world, m, OnDeath, COMP_ONDEATH);
  od->fn = OnMissileDeath;
}

entity_t SpawnEnemySpawner(world_t *world, GameWorld *gw, Vector3 position,
                           int enemyType) {
  archetype_t *arch = WorldGetArchetype(world, gw->spawnerArchId);
  entity_t e = WorldCreateEntity(world, &arch->mask);

  ECS_GET(world, e, Position, COMP_POSITION)->value = position;
  ECS_GET(world, e, Active, COMP_ACTIVE)->value = true;
  ECS_GET(world, e, EnemySpawner, COMP_ENEMY_SPAWNER)->enemyType = enemyType;

  return e;
}

entity_t SpawnWallSegment(world_t *world, GameWorld *gw, Vector3 position,
                          Vector3 localA, Vector3 localB, float localYBottom,
                          float localYTop, float radius,
                          bool blockPlayer, bool blockProjectiles) {
  archetype_t *arch = WorldGetArchetype(world, gw->wallSegArchId);
  entity_t e = WorldCreateEntity(world, &arch->mask);

  ECS_GET(world, e, Position, COMP_POSITION)->value = position;
  ECS_GET(world, e, Active, COMP_ACTIVE)->value = true;

  WallSegmentCollider *wall =
      ECS_GET(world, e, WallSegmentCollider, COMP_WALL_SEGMENT_COLLIDER);
  wall->localA = localA;
  wall->localB = localB;
  wall->localYBottom = localYBottom;
  wall->localYTop = localYTop;
  wall->radius = radius;

  CollisionInstance *ci =
      ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);
  ci->owner = e;
  ci->type = COLLIDER_WALL_SEGMENT;
  ci->layerMask = 1 << LAYER_WORLD;
  ci->collideMask = 1 << LAYER_ENEMY;
  if (blockPlayer)      ci->collideMask |= 1 << LAYER_PLAYER;
  if (blockProjectiles) ci->collideMask |= 1 << LAYER_BULLET;

  Collision_UpdateWallSegment(ci, wall, position);

  return e;
}

/* ------------------------------------------------------------------ */
/*  Melee enemy                                                        */
/* ------------------------------------------------------------------ */

static void Melee_OnDeath(world_t *world, entity_t entity) {
  NavPath *nav = ECS_GET(world, entity, NavPath, COMP_NAVPATH);
  if (nav && nav->points)
    NavPath_Destroy(nav);

  ModelCollection_t *mc = ECS_GET(world, entity, ModelCollection_t, COMP_MODEL);
  if (mc)
    ModelCollectionFree(mc);

  if (s_game) {
    Position *pos = ECS_GET(world, entity, Position, COMP_POSITION);
    if (pos) {
      for (int i = 0; i < 3; i++)
        SpawnCoolant(world, s_game, pos->value);
      if (GetRandomValue(0, 99) < 35)
        SpawnHealthOrb(world, s_game, pos->value);
    }
  }
}

entity_t SpawnEnemyMelee(world_t *world, GameWorld *game, Vector3 position) {
  archetype_t *arch = WorldGetArchetype(world, game->enemyMeleeArchId);
  entity_t e = WorldCreateEntity(world, &arch->mask);

  position.y = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap,
                                             position.x, position.z);

  ECS_GET(world, e, Active, COMP_ACTIVE)->value = true;
  ECS_GET(world, e, Position, COMP_POSITION)->value = position;
  ECS_GET(world, e, Health, COMP_HEALTH)->current = 60.0f;
  ECS_GET(world, e, Health, COMP_HEALTH)->max = 60.0f;
  ECS_GET(world, e, Shield, COMP_SHIELD)->current = 0.0f;
  ECS_GET(world, e, Shield, COMP_SHIELD)->max = 0.0f;

  /* --- Model: torso + legs, no gun --- */
  ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);
  ModelCollectionInit(mc, 2);
  ModelCollectionAdd(mc, (ModelInstance_t){
                             .model = game->gruntLegs,
                             .scale = (Vector3){1, 1, 1},
                             .rotationMode = MODEL_ROT_YAW_ONLY,
                             .parentIndex = -1,
                             .isActive = true,
                         });
  ModelCollectionAdd(mc, (ModelInstance_t){
                             .model = game->gruntTorso,
                             .scale = (Vector3){1, 1, 1},
                             .rotationMode = MODEL_ROT_YAW_ONLY,
                             .parentIndex = -1,
                             .isActive = true,
                         });
  ModelCollectionAdd(mc, (ModelInstance_t){
                             .model = game->gruntSaw,
                             .scale = (Vector3){1, 1, 1},
                             .pivot = (Vector3){0, 3, 0},
                             .rotationMode = MODEL_ROT_YAW_ONLY,
                             .parentIndex = -1,
                             .isActive = true,
                         });

  /* --- Collider --- */
  CapsuleCollider *cap =
      ECS_GET(world, e, CapsuleCollider, COMP_CAPSULE_COLLIDER);
  cap->radius = 1.0f;
  cap->localA = (Vector3){0, 0.0f, 0};
  cap->localB = (Vector3){0, 2.5f, 0};
  Capsule_UpdateWorld(cap, position);

  CollisionInstance *ci =
      ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);
  ci->owner = e;
  ci->type = COLLIDER_CAPSULE;
  ci->layerMask = 1 << LAYER_ENEMY;
  ci->collideMask = 1 << LAYER_BULLET;

  /* --- Melee state --- */
  MeleeEnemy *me = ECS_GET(world, e, MeleeEnemy, COMP_MELEE_ENEMY);
  me->state = MELEE_CHASING;
  me->windupTimer = 0.0f;
  me->lungeTimer = 0.0f;
  me->recoverTimer = 0.0f;
  me->lungeTarget = (Vector3){0, 0, 0};
  me->hasHit = false;
  me->pathPending = false;
  me->repathTimer = 0.0f;

  /* --- Nav --- */
  NavPath *nav = ECS_GET(world, e, NavPath, COMP_NAVPATH);
  NavPath_Init(nav, 32);

  Timer *moveTimer = ECS_GET(world, e, Timer, COMP_MOVE_TIMER);
  moveTimer->value = 0.0f;

  OnDeath *od = ECS_GET(world, e, OnDeath, COMP_ONDEATH);
  od->fn = Melee_OnDeath;

  return e;
}

entity_t SpawnInfoBox(world_t *world, GameWorld *gw,
                      Vector3 position, float halfExtent,
                      const char *message, float duration,
                      int maxTriggers, float markerHeight, int fontSize) {
  archetype_t *arch = WorldGetArchetype(world, gw->infoBoxArchId);
  entity_t e = WorldCreateEntity(world, &arch->mask);

  Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
  if (pos) pos->value = position;

  InfoBox *ib = ECS_GET(world, e, InfoBox, COMP_INFOBOX);
  if (ib) {
    ib->halfExtent    = halfExtent;
    strncpy(ib->message, message, sizeof(ib->message) - 1);
    ib->message[sizeof(ib->message) - 1] = '\0';
    ib->duration      = duration;
    ib->triggersLeft  = (maxTriggers == 0) ? -1 : maxTriggers;
    ib->markerHeight  = markerHeight;
    ib->fontSize      = fontSize;
  }

  Active *act = ECS_GET(world, e, Active, COMP_ACTIVE);
  if (act) act->value = true;

  return e;
}

/* ------------------------------------------------------------------ */
/*  Drone enemy                                                        */
/* ------------------------------------------------------------------ */

static void Drone_OnDeath(world_t *world, entity_t entity) {
  ModelCollection_t *mc = ECS_GET(world, entity, ModelCollection_t, COMP_MODEL);
  if (mc) ModelCollectionFree(mc);

  if (s_game) {
    Position *pos = ECS_GET(world, entity, Position, COMP_POSITION);
    if (pos) {
      for (int i = 0; i < 3; i++)
        SpawnCoolant(world, s_game, pos->value);
      if (GetRandomValue(0, 99) < 25)
        SpawnHealthOrb(world, s_game, pos->value);
    }
  }
}

entity_t SpawnEnemyDrone(world_t *world, GameWorld *game, Vector3 position) {
  archetype_t *arch = WorldGetArchetype(world, game->enemyDroneArchId);
  entity_t e = WorldCreateEntity(world, &arch->mask);

  position.y = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap,
                                             position.x, position.z) + 3.0f;

  ECS_GET(world, e, Active,    COMP_ACTIVE)->value    = true;
  ECS_GET(world, e, Position,  COMP_POSITION)->value  = position;
  ECS_GET(world, e, Velocity,  COMP_VELOCITY)->value  = (Vector3){0, 0, 0};
  ECS_GET(world, e, Orientation, COMP_ORIENTATION)->yaw = 0.0f;

  Health *hp = ECS_GET(world, e, Health, COMP_HEALTH);
  hp->current = 25.0f;
  hp->max     = 25.0f;

  Shield *sh = ECS_GET(world, e, Shield, COMP_SHIELD);
  sh->current = 120.0f;
  sh->max     = 120.0f;

  ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);
  ModelCollectionInit(mc, 1);
  ModelCollectionAdd(mc, (ModelInstance_t){
      .model        = game->gruntTorso,
      .scale        = (Vector3){0.5f, 0.5f, 0.5f},
      .offset       = (Vector3){0, 0, 0},
      .rotation     = (Vector3){0, 0, 0},
      .rotationMode = MODEL_ROT_YAW_ONLY,
      .parentIndex  = -1,
      .isActive     = true,
  });

  SphereCollider *sc = ECS_GET(world, e, SphereCollider, COMP_SPHERE_COLLIDER);
  sc->radius = 0.7f;

  CollisionInstance *ci = ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);
  ci->type        = COLLIDER_SPHERE;
  ci->layerMask   = 1 << LAYER_ENEMY;
  ci->collideMask = (1 << LAYER_BULLET) | (1 << LAYER_PLAYER);

  DroneEnemy *dr = ECS_GET(world, e, DroneEnemy, COMP_DRONE_ENEMY);
  dr->hasTarget      = false;
  dr->retargetTimer  = 0.0f;
  dr->bobTimer       = (float)GetRandomValue(0, 628) / 100.0f;

  OnDeath *od = ECS_GET(world, e, OnDeath, COMP_ONDEATH);
  od->fn = Drone_OnDeath;

  return e;
}

/* ------------------------------------------------------------------ */
/*  Target dummies                                                      */
/* ------------------------------------------------------------------ */

static void Target_OnDeath(world_t *world, entity_t entity) {
  ModelCollection_t *mc = ECS_GET(world, entity, ModelCollection_t, COMP_MODEL);
  if (mc) for (int i = 0; i < mc->count; i++) mc->models[i].isActive = false;

  CollisionInstance *ci = ECS_GET(world, entity, CollisionInstance, COMP_COLLISION_INSTANCE);
  if (ci) { ci->layerMask = 0; ci->collideMask = 0; }

  TargetDummy *td = ECS_GET(world, entity, TargetDummy, COMP_TARGET_DUMMY);
  if (td) {
    td->respawnTimer = 5.0f;
    Position *pos = ECS_GET(world, entity, Position, COMP_POSITION);
    if (pos && s_game) {
      for (int n = 0; n < td->healthDropCount; n++)
        SpawnHealthOrb(world, s_game, pos->value);
      for (int n = 0; n < td->coolantDropCount; n++)
        SpawnCoolant(world, s_game, pos->value);
    }
  }
}

static void SpawnTargetCommon(world_t *world, GameWorld *game, entity_t e,
                              Vector3 position, float health, float shield, float yaw) {
  position.y = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap,
                                             position.x, position.z);

  ECS_GET(world, e, Active,      COMP_ACTIVE)->value      = true;
  ECS_GET(world, e, Position,    COMP_POSITION)->value    = position;
  ECS_GET(world, e, Orientation, COMP_ORIENTATION)->yaw   = yaw;
  ECS_GET(world, e, Orientation, COMP_ORIENTATION)->pitch = 0.0f;

  Health *hp = ECS_GET(world, e, Health, COMP_HEALTH);
  hp->max = health; hp->current = health;

  Shield *sh = ECS_GET(world, e, Shield, COMP_SHIELD);
  sh->max = shield; sh->current = shield;

  ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);
  ModelCollectionInit(mc, 2);
  ModelCollectionAdd(mc, (ModelInstance_t){
    .model        = game->gruntLegs,
    .scale        = (Vector3){1, 1, 1},
    .rotationMode = MODEL_ROT_YAW_ONLY,
    .parentIndex  = -1,
    .isActive     = true,
  });
  ModelCollectionAdd(mc, (ModelInstance_t){
    .model        = game->gruntTorso,
    .scale        = (Vector3){1, 1, 1},
    .rotationMode = MODEL_ROT_YAW_ONLY,
    .parentIndex  = -1,
    .isActive     = true,
  });

  CapsuleCollider *cap = ECS_GET(world, e, CapsuleCollider, COMP_CAPSULE_COLLIDER);
  cap->radius = 1.0f;
  cap->localA = (Vector3){0, 0.0f, 0};
  cap->localB = (Vector3){0, 2.5f, 0};
  Capsule_UpdateWorld(cap, position);

  CollisionInstance *ci = ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);
  ci->owner       = e;
  ci->type        = COLLIDER_CAPSULE;
  ci->layerMask   = 1 << LAYER_ENEMY;
  ci->collideMask = 1 << LAYER_BULLET;
  ci->worldBounds = Capsule_ComputeAABB(cap);

  TargetDummy *td = ECS_GET(world, e, TargetDummy, COMP_TARGET_DUMMY);
  td->respawnTimer = 0.0f;
  td->maxHealth    = health;
  td->maxShield    = shield;
  td->spawnPos     = position;
  td->spawnYaw     = yaw;

  OnDeath *od = ECS_GET(world, e, OnDeath, COMP_ONDEATH);
  od->fn = Target_OnDeath;
}

entity_t SpawnTargetStatic(world_t *world, GameWorld *game, Vector3 position,
                           float health, float shield, float yaw,
                           int healthDropCount, int coolantDropCount) {
  archetype_t *arch = WorldGetArchetype(world, game->targetStaticArchId);
  entity_t e = WorldCreateEntity(world, &arch->mask);
  SpawnTargetCommon(world, game, e, position, health, shield, yaw);
  TargetDummy *td = ECS_GET(world, e, TargetDummy, COMP_TARGET_DUMMY);
  td->healthDropCount  = healthDropCount;
  td->coolantDropCount = coolantDropCount;
  return e;
}

entity_t SpawnTargetPatrol(world_t *world, GameWorld *game,
                           Vector3 posA, Vector3 posB,
                           float health, float shield, float speed, float yaw,
                           int healthDropCount, int coolantDropCount) {
  archetype_t *arch = WorldGetArchetype(world, game->targetPatrolArchId);
  entity_t e = WorldCreateEntity(world, &arch->mask);

  posA.y = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap, posA.x, posA.z);
  posB.y = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap, posB.x, posB.z);

  SpawnTargetCommon(world, game, e, posA, health, shield, yaw);

  TargetPatrol *tp = ECS_GET(world, e, TargetPatrol, COMP_TARGET_PATROL);
  tp->pointA = posA;
  tp->pointB = posB;
  tp->speed  = speed;
  tp->t      = 0.0f;
  tp->dir    = 1;

  TargetDummy *td = ECS_GET(world, e, TargetDummy, COMP_TARGET_DUMMY);
  td->healthDropCount  = healthDropCount;
  td->coolantDropCount = coolantDropCount;

  return e;
}
