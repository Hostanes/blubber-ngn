
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

entity_t SpawnEnemyMissile(world_t *world, GameWorld *gw, Vector3 position);

entity_t SpawnBox(world_t *world, GameWorld *gw, Vector3 position,
                  Vector3 size);

entity_t SpawnBoxModel(world_t *world, GameWorld *gw, Vector3 position,
                       Vector3 size);

entity_t SpawnLevelModel(world_t *world, GameWorld *gw, Model model,
                         Vector3 position, Vector3 rotation, Vector3 scale);

entity_t SpawnTrigger(world_t *world, uint32_t triggerArchId, Vector3 position,
                      Vector3 size);

void SpawnHomingMissile(world_t *world, GameWorld *game, entity_t shooter,
                        entity_t target, Vector3 position, Vector3 forward);

#endif
