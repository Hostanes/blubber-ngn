
#ifndef ENTITY_FACTORY_H
#define ENTITY_FACTORY_H

#include "game.h"
#include <raylib.h>

/* ===================================================== */
/*  Factory Spawn Functions                             */
/* ===================================================== */

entity_t SpawnPlayer(world_t *world, GameWorld *gw, Vector3 position);

entity_t SpawnEnemyGrunt(world_t *world, GameWorld *gw, Vector3 position);
entity_t SpawnEnemyRanger(world_t *world, GameWorld *game, Vector3 position);
entity_t SpawnEnemyMelee(world_t *world, GameWorld *game, Vector3 position);

entity_t SpawnEnemyMissile(world_t *world, GameWorld *gw, Vector3 position);

entity_t SpawnBox(world_t *world, GameWorld *gw, Vector3 position,
                  Vector3 size);

entity_t SpawnBoxModel(world_t *world, GameWorld *gw, Vector3 position,
                       Vector3 size);

entity_t SpawnLevelModel(world_t *world, GameWorld *gw, Model model,
                         Vector3 position, Vector3 rotation, Vector3 scale);

entity_t SpawnProp(world_t *world, GameWorld *gw, Model model,
                   Vector3 position, float yaw, Vector3 scale);

entity_t SpawnTrigger(world_t *world, uint32_t triggerArchId, Vector3 position,
                      Vector3 size);

void SpawnHomingMissile(world_t *world, GameWorld *game, entity_t shooter,
                        entity_t target, Vector3 position, Vector3 forward,
                        bool guided, float turnSpeed);

entity_t SpawnWallSegment(world_t *world, GameWorld *gw, Vector3 position,
                          Vector3 localA, Vector3 localB,
                          float localYBottom, float localYTop, float radius,
                          bool blockPlayer, bool blockProjectiles);

entity_t SpawnEnemySpawner(world_t *world, GameWorld *gw,
                           Vector3 position, int enemyType);

entity_t SpawnInfoBox(world_t *world, GameWorld *gw,
                      Vector3 position, float halfExtent,
                      const char *message, float duration,
                      int maxTriggers, float markerHeight, int fontSize);

void LevelHelper_SetGame(GameWorld *game);
void SpawnCoolant(world_t *world, GameWorld *game, Vector3 pos);
void SpawnHealthOrb(world_t *world, GameWorld *game, Vector3 pos);

entity_t SpawnEnemyDrone(world_t *world, GameWorld *game, Vector3 position);

entity_t SpawnTargetStatic(world_t *world, GameWorld *game, Vector3 position,
                           float health, float shield, float yaw,
                           int healthDropCount, int coolantDropCount);
entity_t SpawnTargetPatrol(world_t *world, GameWorld *game,
                           Vector3 posA, Vector3 posB,
                           float health, float shield, float speed, float yaw,
                           int healthDropCount, int coolantDropCount);

#endif
