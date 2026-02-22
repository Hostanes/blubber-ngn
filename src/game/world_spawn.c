#include "components/components.h"
#include "components/movement.h"
#include "components/muzzle.h"
#include "components/renderable.h"
#include "components/transform.h"
#include "ecs_get.h"
#include "game.h"
#include "nav_grid/nav.h"
#include <raylib.h>
#include <raymath.h>

#define BENCHMARK_ENTITY_COUNT 200000

GameWorld GameWorldCreate(Engine *engine, world_t *world) {
  GameWorld gw = {0};
  gw.gameState = GAMESTATE_MAINMENU;

  uint32_t benchBits[] = {COMP_POSITION, COMP_VELOCITY};

  bitset_t benchMask =
      MakeMask(benchBits, sizeof(benchBits) / sizeof(uint32_t));

  uint32_t benchArchId = WorldCreateArchetype(world, &benchMask);
  archetype_t *benchArch = WorldGetArchetype(world, benchArchId);

  ArchetypeAddInline(benchArch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(benchArch, COMP_VELOCITY, sizeof(Velocity));

  for (uint32_t i = 0; i < BENCHMARK_ENTITY_COUNT; i++) {
    entity_t e = WorldCreateEntity(world, &benchMask);

    Position *p = ECS_GET(world, e, Position, COMP_POSITION);
    Velocity *v = ECS_GET(world, e, Velocity, COMP_VELOCITY);

    p->value = (Vector3){0, 0, 0};

    v->value =
        (Vector3){(float)(i % 10) * 0.1f, 0.0f, (float)((i * 7) % 10) * 0.1f};
  }

  return gw;
}
