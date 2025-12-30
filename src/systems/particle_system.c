#include "systems.h"

void spawnParticle(Engine_t *eng, Vector3 pos, float lifetime, int type) {

  static int cursor = 0;

  for (int i = 0; i < MAX_PARTICLES; i++) {
    // int i = cursor;
    if (eng->particles.active[i])
      continue;

    eng->particles.active[i] = true;
    eng->particles.types[i] = type;
    eng->particles.positions[i] = pos;
    eng->particles.lifetimes[i] = lifetime;
    eng->particles.startLifetimes[i] = lifetime;

    // printf("PARTICLE spawned particle at index %d\n", i);
    break;
  }
}

// =======================================
// PARTICLE SPAWN HELPERS
// =======================================

// --- General spawn wrapper ---
static inline void SpawnParticleTyped(Engine_t *eng, Vector3 pos, float life,
                                      int type) {
  spawnParticle(eng, pos, life, type);
}

// --- Dust puff (type 0) ---
void SpawnDust(Engine_t *eng, Vector3 pos) {
  SpawnParticleTyped(eng, pos, 2.0f, 2);
}

// --- Metal dust / debris (type 1) ---
void SpawnMetalDust(Engine_t *eng, Vector3 pos) {
  SpawnParticleTyped(eng, pos, 1.2f, 1);
}

// --- Sparks (type 2) ---
void SpawnSpark(Engine_t *eng, Vector3 pos) {
  SpawnParticleTyped(eng, pos, 0.6f, 0);
}

// --- Sand / dirt burst (type 3) ---
void SpawnSandBurst(Engine_t *eng, Vector3 pos) {
  SpawnParticleTyped(eng, pos, 1.8f, 3);
}

// --- Smoke plume (type 4) ---
void SpawnSmoke(Engine_t *eng, Vector3 pos) {
  SpawnParticleTyped(eng, pos, 3.0f, 4);
}

void UpdateParticles(Engine_t *eng, float dt) {

  for (int i = 0; i < MAX_PARTICLES; i++) {
    if (!eng->particles.active[i])
      continue;

    int type = eng->particles.types[i];
    eng->particles.lifetimes[i] -= dt;

    if (eng->particles.lifetimes[i] <= 0.0f) {
      eng->particles.active[i] = false;
      continue;
    }

    Vector3 *pos = &eng->particles.positions[i];

    // ==============================
    // TYPE-SPECIFIC PARTICLE BEHAVIOR
    // ==============================

    switch (type) {

    // ----------------------
    // Dust → small upward drift
    // ----------------------
    case 0:
      pos->y += 2.0f * dt;
      break;

    // ----------------------
    // Metal dust → slight outward push
    // ----------------------
    case 1:
      pos->x += ((float)GetRandomValue(-20, 20) / 200.0f) * dt;
      pos->y += 1.0f * dt;
      pos->z += ((float)GetRandomValue(-20, 20) / 200.0f) * dt;
      break;

    // ----------------------
    // Sparks → fast outward motion + gravity
    // ----------------------
    case 2:
      pos->y += 2.0f * dt;
      pos->x += ((float)GetRandomValue(-30, 30) / 100.0f) * dt;
      pos->z += ((float)GetRandomValue(-30, 30) / 100.0f) * dt;
      break;

    // ----------------------
    // Sand burst → strong upward and outward
    // ----------------------
    case 3:
      pos->y += 10.0f * dt;
      pos->x += ((float)GetRandomValue(-10, 10) / 20.0f) * dt;
      pos->z += ((float)GetRandomValue(-10, 10) / 20.0f) * dt;
      break;

    // ----------------------
    // Smoke → slow rise + sideways drifting
    // ----------------------
    case 4:
      pos->y += 10.0f * dt;
      pos->x += ((float)GetRandomValue(-10, 10) / 200.0f);
      pos->z += ((float)GetRandomValue(-10, 10) / 200.0f);
      break;

    } // end switch
  }
}
