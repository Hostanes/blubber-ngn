#include "../game.h"
#include "message_system.h"
#include <math.h>

void InfoBoxTriggerSystem(world_t *world, GameWorld *game) {
  Position *playerPos = ECS_GET(world, game->player, Position, COMP_POSITION);
  if (!playerPos) return;

  archetype_t *arch = WorldGetArchetype(world, game->infoBoxArchId);
  if (!arch) return;

  for (uint32_t i = 0; i < arch->count; i++) {
    entity_t e = arch->entities[i];
    Active *act = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!act || !act->value) continue;
    InfoBox *ib = ECS_GET(world, e, InfoBox, COMP_INFOBOX);
    if (!ib || ib->triggersLeft == 0) continue;
    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
    if (!pos) continue;

    bool inside = fabsf(playerPos->value.x - pos->value.x) < ib->halfExtent &&
                  fabsf(playerPos->value.y - pos->value.y) < ib->halfExtent &&
                  fabsf(playerPos->value.z - pos->value.z) < ib->halfExtent;

    if (inside && !ib->playerInside) {
      MessageSystem_Push(&game->messageSystem, ib->message, ib->duration);
      if (ib->triggersLeft > 0) ib->triggersLeft--;
    }
    ib->playerInside = inside;
  }
}
