#include "game.h"

#define ENTITIES_PER_ARCH (TOTAL_ENTITIES / NUM_ARCHETYPES)

GameWorld GameWorldCreate(Engine *engine, world_t *world) {
  GameWorld gw = {0};
  gw.archCount = NUM_ARCHETYPES;

  uint32_t bits[] = {COMP_POSITION, COMP_VELOCITY, COMP_TIMER};
  bitset_t mask = MakeMask(bits, 3);

  for (uint32_t a = 0; a < NUM_ARCHETYPES; ++a) {
    uint32_t archId = WorldCreateArchetype(world, &mask);
    gw.archIds[a] = archId;

    archetype_t *arch = WorldGetArchetype(world, archId);

    ArchetypeAddInline(arch, COMP_POSITION, sizeof(Position));
    ArchetypeAddInline(arch, COMP_VELOCITY, sizeof(Velocity));
    ArchetypeAddHandle(arch, COMP_TIMER, &engine->timerPool);

    for (uint32_t i = 0; i < ENTITIES_PER_ARCH; ++i) {
      entity_t e = WorldCreateEntity(world, &mask);

      Position *p = ECS_GET(world, e, Position, COMP_POSITION);
      Velocity *v = ECS_GET(world, e, Velocity, COMP_VELOCITY);
      Timer *t = ECS_GET(world, e, Timer, COMP_TIMER);

      p->x = 0.0f;
      p->y = 0.0f;
      p->z = 0.0f;
      v->x = (float)(i % 10) * 0.1f;
      v->y = 0.0f;
      v->z = (float)((i * 7) % 10) * 0.1f;
      t->value = 5.0f;
    }
  }

  return gw;
}
