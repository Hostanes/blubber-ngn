
#ifndef ENTITY_FACTORY_H
#define ENTITY_FACTORY_H

#include "game.h"
#include <raylib.h>

/* ===================================================== */
/*  Archetype Creation Helper                           */
/* ===================================================== */

uint32_t CreateArchetype(world_t *world, uint32_t *components, int count);

/* ===================================================== */
/*  Archetype Registration                              */
/* ===================================================== */

uint32_t RegisterPlayerArchetype(world_t *world, Engine *engine);

uint32_t RegisterEnemyArchetype(world_t *world, Engine *engine);

uint32_t RegisterBoxArchetype(world_t *world, Engine *engine);

uint32_t RegisterLevelModelArchetype(world_t *world, Engine *engine);

uint32_t RegisterTriggerArchetype(world_t *world, Engine *engine);

/* ===================================================== */
/*  Factory Spawn Functions                             */
/* ===================================================== */

entity_t SpawnPlayer(world_t *world, GameWorld *gw, Vector3 position);

entity_t SpawnEnemyGrunt(world_t *world, GameWorld *gw, Vector3 position);

entity_t SpawnEnemyMissile(world_t *world, GameWorld *gw, Vector3 position);

entity_t SpawnBox(world_t *world, GameWorld *gw, Vector3 position,
                  Vector3 size);

entity_t SpawnBoxModel(world_t *world, GameWorld *gw, Vector3 position,
                       Vector3 size);

entity_t SpawnLevelModel(world_t *world, GameWorld *gw, Model model,
                         Vector3 position, Vector3 rotation, Vector3 scale);

entity_t SpawnTrigger(world_t *world, uint32_t triggerArchId, Vector3 position,
                      Vector3 size);

#endif
