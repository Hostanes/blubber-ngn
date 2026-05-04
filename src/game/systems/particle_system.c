#include "../game.h"
#include "systems.h"

void SpawnParticle(world_t *world, GameWorld *game, Vector3 pos, Vector3 vel,
                   float radius, float lifetime, Color color) {
  archetype_t *arch = WorldGetArchetype(world, game->particleArchId);
  for (uint32_t i = 0; i < arch->count; i++) {
    entity_t e = arch->entities[i];
    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || active->value) continue;

    active->value = true;
    ECS_GET(world, e, Position, COMP_POSITION)->value = pos;
    ECS_GET(world, e, Velocity, COMP_VELOCITY)->value = vel;

    Particle *p    = ECS_GET(world, e, Particle, COMP_PARTICLE);
    p->lifetime    = lifetime;
    p->maxLifetime = lifetime;
    p->radius      = radius;
    p->color       = color;
    return;
  }
}

void ParticleSystem(world_t *world, archetype_t *arch, float dt) {
  for (uint32_t i = 0; i < arch->count; i++) {
    entity_t e = arch->entities[i];

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value) continue;

    Particle *p = ECS_GET(world, e, Particle, COMP_PARTICLE);
    if (!p) continue;

    p->lifetime -= dt;
    if (p->lifetime <= 0.0f) {
      active->value = false;
      continue;
    }

    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
    Velocity *vel = ECS_GET(world, e, Velocity, COMP_VELOCITY);
    if (pos && vel)
      pos->value = Vector3Add(pos->value, Vector3Scale(vel->value, dt));
  }
}
