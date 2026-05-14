#include "game.h"

#define BENCHMARK_ENTITY_COUNT 2000000

GameWorld GameWorldCreate(Engine *engine, world_t *world) {
  GameWorld gw = {0};

  uint32_t moveBits[] = {COMP_POSITION, COMP_VELOCITY};
  bitset_t moveMask = MakeMask(moveBits, 2);

  gw.moveArchId = WorldCreateArchetype(world, &moveMask);
  archetype_t *moveArch = WorldGetArchetype(world, gw.moveArchId);

  ArchetypeAddInline(moveArch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(moveArch, COMP_VELOCITY, sizeof(Velocity));

  uint32_t timerBits[] = {COMP_POSITION, COMP_VELOCITY, COMP_TIMER};
  bitset_t timerMask = MakeMask(timerBits, 3);

  gw.timerArchId = WorldCreateArchetype(world, &timerMask);
  archetype_t *timerArch = WorldGetArchetype(world, gw.timerArchId);

  ArchetypeAddInline(timerArch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(timerArch, COMP_VELOCITY, sizeof(Velocity));
  ArchetypeAddHandle(timerArch, COMP_TIMER, &engine->timerPool);

  for (uint32_t i = 0; i < BENCHMARK_ENTITY_COUNT; i++) {
    if ((i & 1) == 0) {
      entity_t e = WorldCreateEntity(world, &moveMask);
      Position *p = ECS_GET(world, e, Position, COMP_POSITION);
      Velocity *v = ECS_GET(world, e, Velocity, COMP_VELOCITY);
      p->x = 0.0f; p->y = 0.0f; p->z = 0.0f; p->w = 0.0f;
      v->x = 0.1f; v->y = 0.0f; v->z = 0.1f; v->w = 0.0f;
    } else {
      entity_t e = WorldCreateEntity(world, &timerMask);
      Position *p = ECS_GET(world, e, Position, COMP_POSITION);
      Velocity *v = ECS_GET(world, e, Velocity, COMP_VELOCITY);
      float *t = ECS_GET(world, e, float, COMP_TIMER);
      p->x = 0.0f; p->y = 0.0f; p->z = 0.0f; p->w = 0.0f;
      v->x = 0.1f; v->y = 0.0f; v->z = 0.1f; v->w = 0.0f;
      *t = 5.0f;
    }
  }

  return gw;
}
