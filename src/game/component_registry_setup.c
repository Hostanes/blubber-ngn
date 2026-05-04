#include "component_registry_setup.h"
#include "../engine/ecs/archetype_internal.h"
#include "../engine/ecs/component_registry.h"
#include "components/components.h"
#include "components/renderable.h"
#include "game.h"
#include "nav_grid/nav.h"

#define REG_INLINE(str, id, Type)                                              \
  ComponentRegistry_Add(reg, str, id, sizeof(Type), ArchetypeStorageInline,   \
                        NULL)

#define REG_HANDLE(str, id, Type, pool)                                        \
  ComponentRegistry_Add(reg, str, id, sizeof(Type), ArchetypeStorageHandle,   \
                        pool)

#define REG_TAG(str, id)                                                       \
  ComponentRegistry_Add(reg, str, id, 0, ArchetypeStorageInline, NULL)

void SetupComponentRegistry(ComponentRegistry *reg, Engine *engine) {

  /* ---- Transform ---- */
  REG_INLINE("Position",    COMP_POSITION,    Position);
  REG_INLINE("Velocity",    COMP_VELOCITY,    Velocity);
  REG_INLINE("Orientation", COMP_ORIENTATION, Orientation);

  /* ---- Stats ---- */
  REG_INLINE("Health",      COMP_HEALTH,      Health);
  REG_INLINE("Shield",      COMP_SHIELD,      Shield);
  REG_INLINE("Active",      COMP_ACTIVE,      Active);
  REG_INLINE("Particle",    COMP_PARTICLE,    Particle);
  REG_INLINE("MeleeEnemy",  COMP_MELEE_ENEMY, MeleeEnemy);

  /* ---- Collision ---- */
  REG_INLINE("CollisionInstance",    COMP_COLLISION_INSTANCE,    CollisionInstance);
  REG_INLINE("CapsuleCollider",      COMP_CAPSULE_COLLIDER,      CapsuleCollider);
  REG_INLINE("SphereCollider",       COMP_SPHERE_COLLIDER,       SphereCollider);
  REG_INLINE("AABBCollider",         COMP_AABB_COLLIDER,         AABBCollider);
  REG_INLINE("WallSegmentCollider",  COMP_WALL_SEGMENT_COLLIDER, WallSegmentCollider);

  /* ---- Player state ---- */
  REG_INLINE("IsGrounded",  COMP_ISGROUNDED,   bool);
  REG_INLINE("IsDashing",   COMP_ISDASHING,    bool);

  /* ---- Combat / AI ---- */
  REG_INLINE("MuzzleCollection", COMP_MUZZLES,       MuzzleCollection_t);
  REG_INLINE("CombatState",      COMP_COMBAT_STATE,  CombatState_t);
  REG_INLINE("NavPath",          COMP_NAVPATH,       NavPath);

  /* ---- Spawner ---- */
  REG_INLINE("EnemySpawner", COMP_ENEMY_SPAWNER, EnemySpawner);

  /* ---- Events ---- */
  REG_INLINE("OnDeath",     COMP_ONDEATH,     OnDeath);
  REG_INLINE("OnCollision", COMP_ONCOLLISION, OnCollision);

  /* ---- Bullets ---- */
  REG_INLINE("BulletType",  COMP_BULLETTYPE,   BulletType);
  REG_INLINE("BulletOwner", COMP_BULLET_OWNER, BulletOwner);
  REG_INLINE("HomingMissile", COMP_HOMINGMISSILE, HomingMissile);

  /* ---- Tag components (mask only, no data) ---- */
  REG_TAG("Gravity",    COMP_GRAVITY);
  REG_TAG("TypePlayer", COMP_TYPE_PLAYER);
  REG_TAG("TypeGrunt",  COMP_TYPE_GRUNT);
  REG_TAG("TypeRanger", COMP_TYPE_RANGER);
  REG_TAG("TypeMelee",  COMP_TYPE_MELEE);

  /* ---- Handle components (data lives in a shared pool) ---- */
  REG_HANDLE("Model",        COMP_MODEL,            ModelCollection_t, &engine->modelPool);
  REG_HANDLE("Timer",        COMP_TIMER,            Timer,             &engine->timerPool);
  REG_HANDLE("DashTimer",    COMP_DASHTIMER,        Timer,             &engine->timerPool);
  REG_HANDLE("CoyoteTimer",  COMP_COYOTETIMER,      Timer,             &engine->timerPool);
  REG_HANDLE("DashCooldown", COMP_DASHCOOLDOWN,     Timer,             &engine->timerPool);
  REG_HANDLE("FireTimer",    COMP_GRUNT_FIRE_TIMER, Timer,             &engine->timerPool);
  REG_HANDLE("MoveTimer",    COMP_MOVE_TIMER,       Timer,             &engine->timerPool);
}
