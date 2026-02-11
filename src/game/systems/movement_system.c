
#include "systems.h"

void MovementSystem(world_t *world, archetype_t *arch, float dt) {
#pragma omp parallel for if (arch->count >= OMP_MIN_ITERATIONS)
  for (uint32_t i = 0; i < arch->count; ++i) {
    entity_t e = arch->entities[i];

    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
    Velocity *vel = ECS_GET(world, e, Velocity, COMP_VELOCITY);

    pos->value = Vector3Add(pos->value, Vector3Scale(vel->value, dt));
  }
}
