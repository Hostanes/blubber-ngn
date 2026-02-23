#include "components/components.h"
#include "components/movement.h"
#include "components/renderable.h"
#include "components/transform.h"
#include "ecs_get.h"
#include "game.h"
#include "nav_grid/nav.h"
#include <raylib.h>
#include <raymath.h>

#define BENCHMARK_ENTITY_COUNT 2000000

GameWorld GameWorldCreate(Engine *engine, world_t *world) {
  GameWorld gw = {0};

  // ---- Base movement archetype ----
  uint32_t moveBits[] = {COMP_POSITION, COMP_VELOCITY};
  bitset_t moveMask = MakeMask(moveBits, 2);

  gw.moveArchId = WorldCreateArchetype(world, &moveMask);
  archetype_t *moveArch = WorldGetArchetype(world, gw.moveArchId);

  ArchetypeAddInline(moveArch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(moveArch, COMP_VELOCITY, sizeof(Velocity));

  // ---- Movement + Timer archetype ----
  uint32_t timerBits[] = {COMP_POSITION, COMP_VELOCITY, COMP_TIMER};
  bitset_t timerMask = MakeMask(timerBits, 3);

  gw.timerArchId = WorldCreateArchetype(world, &timerMask);
  archetype_t *timerArch = WorldGetArchetype(world, gw.timerArchId);

  ArchetypeAddInline(timerArch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(timerArch, COMP_VELOCITY, sizeof(Velocity));
  ArchetypeAddInline(timerArch, COMP_TIMER, sizeof(Timer));

  // ---- Create benchmark entities ----
  for (uint32_t i = 0; i < BENCHMARK_ENTITY_COUNT; i++) {

    if ((i & 1) == 0) {
      entity_t e = WorldCreateEntity(world, &moveMask);

      Position *p = ECS_GET(world, e, Position, COMP_POSITION);
      Velocity *v = ECS_GET(world, e, Velocity, COMP_VELOCITY);

      p->value = (Vector3){0.0f, 0.0f, 0.0f};
      v->value = (Vector3){0.1f, 0.0f, 0.1f};

    } else {
      entity_t e = WorldCreateEntity(world, &timerMask);

      Position *p = ECS_GET(world, e, Position, COMP_POSITION);
      Velocity *v = ECS_GET(world, e, Velocity, COMP_VELOCITY);
      Timer *t = ECS_GET(world, e, Timer, COMP_TIMER);

      p->value = (Vector3){0.0f, 0.0f, 0.0f};
      v->value = (Vector3){0.1f, 0.0f, 0.1f};

      t->value = 5.0f;
    }
  }

  return gw;
}
