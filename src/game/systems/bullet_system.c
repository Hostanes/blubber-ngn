
#include "../game.h"
#include "systems.h"
#include <stdint.h>

void BulletSystem(world_t *world, archetype_t *arch, float dt) {
  for (uint32_t i = 0; i < arch->count; i++) {
    entity_t e = arch->entities[i];

    Active *a = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!a->value)
      continue;

    Timer *life = ECS_GET(world, e, Timer, COMP_TIMER);
    if (life->value <= 0.0f) {
      printf("destroyed bullet\n");
      a->value = false;
      continue;
    }

    Position *p = ECS_GET(world, e, Position, COMP_POSITION);
    Velocity *v = ECS_GET(world, e, Velocity, COMP_VELOCITY);

    p->value = Vector3Add(p->value, Vector3Scale(v->value, dt));
  }
}
