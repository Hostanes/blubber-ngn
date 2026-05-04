#pragma once
#include "../../engine/ecs/world.h"
#include "../../engine/math/collision_instance.h"
#include "../../engine/math/heightmap.h"
#include "../components/components.h"
#include "../components/movement.h"
#include "../components/transform.h"
#include "../ecs_get.h"
#include "../nav_grid/nav.h"
#include "raylib.h"
#include "raymath.h"
#include <stdint.h>

typedef struct GameWorld GameWorld;

void PlayerControlSystem(world_t *world, GameWorld *game, entity_t player,
                         float dt);
void PlayerWeaponSwitchSystem(world_t *world, GameWorld *game, entity_t player);
void PlayerWeaponSystem(world_t *world, GameWorld *game, entity_t player, float dt);
void PlayerShootSystem(world_t *world, GameWorld *game, entity_t player, float dt);

void FireMuzzle(world_t *world, GameWorld *game, entity_t shooter,
                int shooterArchId, Muzzle_t *m);
void BulletSystem(world_t *world, GameWorld *game, archetype_t *bulletArch,
                  float dt);
void HomingMissileSystem(world_t *world, GameWorld *game, archetype_t *arch,
                         float dt);

void ApplyGravity(world_t *world, GameWorld *game, float dt);
void MovementSystem(world_t *world, archetype_t *arch, float dt);

void PlayerMoveAndCollide(world_t *world, GameWorld *game, float dt);
void UpdateCollisionBounds(world_t *world);
void UpdatePlayerCollision(world_t *world, entity_t e);
void UpdateObstacleCollision(world_t *world, archetype_t *obstacleArch);
void UpdateBulletCollision(world_t *world, archetype_t *bulletArch);

void RenderLevelSystem(world_t *world, GameWorld *game, Camera *camera);

void RenderMainMenu(GameWorld *game);

void TimerSystem(componentPool_t *timerPool, float dt);
void CollisionSystem(world_t *world);

void EnemyGruntAISystem(world_t *world, GameWorld *game, archetype_t *enemyArch,
                        float dt);
bool EnemyFollowPath(world_t *world, GameWorld *game, entity_t e,
                     float maxSpeed, float rotateSpeed, float dt);
bool EnemyPathQueue_Submit(NavGrid *grid, Vector3 start, Vector3 goal,
                           NavPath *outPath, bool *pendingFlag,
                           CombatState_t *combat);
#define NAV_PATHS_PER_FRAME 2
void EnemyPathQueue_Flush(int maxPerFrame);
void EnemyAimSystem(world_t *world, GameWorld *game, archetype_t *enemyArch,
                    float dt);
void EnemyFireSystem(world_t *world, GameWorld *game, archetype_t *enemyArch);

void EnemyRangerAISystem(world_t *world, GameWorld *game,
                         archetype_t *enemyArch, float dt);
void EnemyRangerFireSystem(world_t *world, GameWorld *game,
                           archetype_t *enemyArch, float dt);
void EnemyRangerAimSystem(world_t *world, GameWorld *game,
                          archetype_t *enemyArch, float dt);

// void Grunt_SetTargets(world_t *world, entity_t e, GameWorld *game);
// void Tank_SetTargets(world_t *world, entity_t e, GameWorld *game);
// void MissileBot_SetTargets(world_t *world, entity_t e, GameWorld *game);

void CollisionSyncSystem(world_t *world);

void SpawnParticle(world_t *world, GameWorld *game, Vector3 pos, Vector3 vel,
                   float radius, float lifetime, Color color);
void ParticleSystem(world_t *world, archetype_t *arch, float dt);

void EnemyMeleeAISystem(world_t *world, GameWorld *game,
                         archetype_t *arch, float dt);

void DrawSpawnerWireframes(world_t *world, uint32_t spawnerArchId);
