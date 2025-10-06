
// game.c
// Implements game state initialization

#include "game.h"
#include "raylib.h" 

GameState_t InitGame(void) {
  GameState_t gs = {0};

  gs.entities.count = MAX_ENTITIES;

  gs.entities.types = (EntityType_t*)MemAlloc(sizeof(EntityType_t) * MAX_ENTITIES);


  gs.entities.positions = (Vector3 *)MemAlloc(sizeof(Vector3) * MAX_ENTITIES);
  gs.entities.velocities = (Vector3 *)MemAlloc(sizeof(Vector3) * MAX_ENTITIES);
  gs.entities.legOrientation =
      (Orientation *)MemAlloc(sizeof(Orientation) * MAX_ENTITIES);
  gs.entities.torsoOrientation =
      (Orientation *)MemAlloc(sizeof(Orientation) * MAX_ENTITIES);
  gs.entities.stepCycle = (float *)MemAlloc(sizeof(float) * MAX_ENTITIES);
  gs.entities.prevStepCycle = (float *)MemAlloc(sizeof(float) * MAX_ENTITIES);
  gs.entities.stepRate = (float *)MemAlloc(sizeof(float) * MAX_ENTITIES);

  // Init player (entity 0)
  gs.playerId = 0;
  gs.entities.types[0] = ENTITY_PLAYER;
  gs.entities.positions[0] = (Vector3){0, 1, 0};
  gs.entities.velocities[0] = (Vector3){0, 0, 0};
  gs.entities.legOrientation[0] = (Orientation){0, 0, 0};
  gs.entities.torsoOrientation[0] = (Orientation){0, 0, 0};
  gs.entities.stepCycle[0] = 0.0f;
  gs.entities.prevStepCycle[0] = 0.0f;
  gs.entities.stepRate[0] = 2.0f;
  gs.pHeadbobTimer = 0.0f;

  return gs;
}
