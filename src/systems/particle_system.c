#include "systems.h"

void spawnParticle(Engine_t *eng, Vector3 pos, float lifetime, int type) {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (eng->particles.active[i])
      continue;

    eng->particles.active[i] = true;
    eng->particles.types[i] = type;
    eng->particles.positions[i] = pos;
    eng->particles.lifetimes[i] = lifetime;
    eng->particles.startLifetimes[i] = lifetime;

    printf("PARTICLE spawned particle at index %d\n", i);
    break;
  }
}

void UpdateParticles(Engine_t *eng, float dt) {
  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!eng->particles.active[i])
      continue;

    eng->particles.lifetimes[i] -= dt;

    if (eng->particles.lifetimes[i] <= 0.0f) {
      printf("PARTICLE died\n");
      eng->particles.active[i] = false;
    }
  }
}
