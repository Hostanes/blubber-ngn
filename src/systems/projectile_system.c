#include "systems.h"
#include <raylib.h>

typedef struct ExplosionDef {
  float radius;
  float maxDamage; // damage at center
  float minDamage; // damage at edge (usually 0)
  float impulse;   // optional (can ignore for now)
  int particleType;
  float particleLife;
  SoundType_t soundType;
  float soundVol;
  float soundPitch;
} ExplosionDef;

static inline ExplosionDef GetExplosionDef(int projectileType) {
  // Tune these however you want
  switch (projectileType) {
  case P_PLASMA: // big shell
    return (ExplosionDef){.radius = 150.0f,
                          .maxDamage = 100.0f,
                          .minDamage = 0.0f,
                          .impulse = 0.0f,
                          .particleType = 5,
                          .particleLife = 1.2f,
                          .soundType = SOUND_EXPLOSION,
                          .soundVol = 1.0f,
                          .soundPitch = 0.95f};
  case P_ROCKET: // rocket
    return (ExplosionDef){.radius = 80.0f,
                          .maxDamage = 35.0f,
                          .minDamage = 0.0f,
                          .impulse = 0.0f,
                          .particleType = 8,
                          .particleLife = 0.5f,
                          .soundType = SOUND_EXPLOSION,
                          .soundVol = 1.0f,
                          .soundPitch = 1.05f};
  case P_MISSILE: // rocket
    return (ExplosionDef){.radius = 80.0f,
                          .maxDamage = 35.0f,
                          .minDamage = 0.0f,
                          .impulse = 0.0f,
                          .particleType = 8,
                          .particleLife = 0.5f,
                          .soundType = SOUND_EXPLOSION,
                          .soundVol = 1.0f,
                          .soundPitch = 1.05f};
  default:
    // non-explosive
    return (ExplosionDef){0};
  }
}

static inline float DamageFalloffLinear(float dist, float radius, float maxDmg,
                                        float minDmg) {
  if (dist >= radius)
    return 0.0f;
  float t = 1.0f - (dist / radius); // 1 at center -> 0 at edge
  float dmg = minDmg + t * (maxDmg - minDmg);
  return (dmg < 0.0f) ? 0.0f : dmg;
}

static void SpawnExplosion(GameState_t *gs, Engine_t *eng,
                           SoundSystem_t *soundSys, Vector3 pos,
                           int projectileType, entity_t owner) {
  ExplosionDef def = GetExplosionDef(projectileType);
  if (def.radius <= 0.0f)
    return; // not explosive

  // 1) VFX + SFX
  spawnParticle(eng, pos, def.particleLife, def.particleType);
  spawnParticle(eng, pos, 1.5, 6);
  QueueSound(soundSys, def.soundType, pos, def.soundVol, def.soundPitch);

  // 2) Damage nearby actors using grid
  EntityGrid_t *grid = &gs->grid;

  int minX = (int)((pos.x - def.radius - grid->minX) / grid->cellSize);
  int maxX = (int)((pos.x + def.radius - grid->minX) / grid->cellSize);
  int minZ = (int)((pos.z - def.radius - grid->minZ) / grid->cellSize);
  int maxZ = (int)((pos.z + def.radius - grid->minZ) / grid->cellSize);

  if (minX < 0)
    minX = 0;
  if (minZ < 0)
    minZ = 0;
  if (maxX >= grid->width)
    maxX = grid->width - 1;
  if (maxZ >= grid->length)
    maxZ = grid->length - 1;

  for (int gx = minX; gx <= maxX; gx++) {
    for (int gz = minZ; gz <= maxZ; gz++) {
      GridNode_t *node = &grid->nodes[gx][gz];

      for (int n = 0; n < node->count; n++) {
        entity_t eid = node->entities[n];
        if (eid == GRID_EMPTY)
          continue;

        // Ignore projectiles
        if (GetEntityCategory(eid) != ET_ACTOR) {
          // printf("Not actor, skipping\n");
          continue;
        }

        int idx = GetEntityIndex(eid);
        if (idx < 0 || idx >= eng->em.count)
          continue;
        if (!eng->em.alive[idx])
          continue;

        // only damage entities with hitpoints
        if (!(eng->em.masks[idx] & C_HITPOINT_TAG))
          continue;

        if (idx == gs->playerId) {
          QueueSound(soundSys, SOUND_CLANG,
                     *(Vector3 *)getComponent(&eng->actors, gs->playerId,
                                              gs->compReg.cid_Positions),
                     0.2f, 1.0f);
        }

        Vector3 *tpos = (Vector3 *)getComponent(&eng->actors, eid,
                                                gs->compReg.cid_Positions);
        if (!tpos)
          continue;

        // distance in 3D or XZ; choose XZ if you want
        Vector3 d = Vector3Subtract(*tpos, pos);
        float dist = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);

        if (dist > def.radius)
          continue;

        float dmg =
            DamageFalloffLinear(dist, def.radius, def.maxDamage, def.minDamage);
        if (dmg <= 0.0f)
          continue;

        eng->actors.hitPoints[idx] -= dmg;

        if (eng->actors.hitPoints[idx] <= 0) {
          KillEntity(gs, eng, soundSys, MakeEntityID(ET_ACTOR, idx));
        }
      }
    }
  }
}

// ------------------------------------------------------------
// Helpers (keep static in same .c file)
// ------------------------------------------------------------
static inline bool ProjectileIsActive(const ProjectilePool_t *pp, int i) {
  return pp->active[i];
}

static inline entity_t ProjectileID(int i) {
  return MakeEntityID(ET_PROJECTILE, i);
}

static inline void DeactivateProjectile(Engine_t *eng, int i) {
  eng->projectiles.active[i] = false;
}

static inline void UpdateProjectileGrid(GameState_t *gs, Engine_t *eng, int i,
                                        Vector3 prevPos, Vector3 nextPos) {
  GridRemoveEntity(&gs->grid, ProjectileID(i), prevPos);
  GridAddEntity(&gs->grid, ProjectileID(i), nextPos);
}

static inline void GetCellCoords(const EntityGrid_t *grid, Vector3 p, int *cx,
                                 int *cz) {
  *cx = (int)((p.x - grid->minX) / grid->cellSize);
  *cz = (int)((p.z - grid->minZ) / grid->cellSize);
}

// Returns true if hit happened (projectile should stop processing)
static bool CheckStaticHit(GameState_t *gs, Engine_t *eng, int projIdx,
                           Vector3 prevPos, Vector3 nextPos) {
  int cx, cz;
  GetCellCoords(&gs->grid, nextPos, &cx, &cz);

  for (int dx = -1; dx <= 1; dx++) {
    for (int dz = -1; dz <= 1; dz++) {
      int nx = cx + dx, nz = cz + dz;
      if (!IsCellValid(&gs->grid, nx, nz))
        continue;

      GridNode_t *node = &gs->grid.nodes[nx][nz];
      for (int n = 0; n < node->count; n++) {
        entity_t e = node->entities[n];
        if (GetEntityCategory(e) != ET_STATIC)
          continue;

        int s = GetEntityIndex(e);
        ModelCollection_t *hb = &eng->statics.hitboxCollections[s];

        if (hb->countModels <= 0 || !hb->isActive)
          continue;

        for (int m = 0; m < hb->countModels; m++) {
          if (!hb->isActive[m])
            continue;

          if (SegmentIntersectsOBB(prevPos, nextPos, hb, m)) {
            // printf("Projectile hit STATIC %d (hb %d)\n", s, m);
            SpawnMetalDust(eng, prevPos);
            DeactivateProjectile(eng, projIdx);
            return true;
          }
        }
      }
    }
  }

  return false;
}

// Returns true if hit happened (projectile should stop processing)
static bool CheckActorHit(GameState_t *gs, Engine_t *eng,
                          SoundSystem_t *soundSys, int projIdx, Vector3 prevPos,
                          Vector3 nextPos) {
  int cx, cz;
  GetCellCoords(&gs->grid, nextPos, &cx, &cz);

  int pType = eng->projectiles.types[projIdx];
  entity_t owner = eng->projectiles.owners[projIdx];

  for (int dx = -1; dx <= 1; dx++) {
    for (int dz = -1; dz <= 1; dz++) {
      int nx = cx + dx, nz = cz + dz;
      if (!IsCellValid(&gs->grid, nx, nz))
        continue;

      GridNode_t *node = &gs->grid.nodes[nx][nz];
      for (int n = 0; n < node->count; n++) {
        entity_t e = node->entities[n];
        if (e == GRID_EMPTY)
          continue;
        if (GetEntityCategory(e) != ET_ACTOR)
          continue;
        if (e == owner)
          continue;

        int idx = GetEntityIndex(e);
        if (!eng->em.alive[idx])
          continue;
        if (!(eng->em.masks[idx] & C_HITBOX))
          continue;

        ModelCollection_t *hb = &eng->actors.hitboxCollections[idx];
        if (hb->countModels <= 0 || !hb->isActive)
          continue;

        for (int m = 0; m < hb->countModels; m++) {
          if (!hb->isActive[m])
            continue;

          if (SegmentIntersectsOBB(prevPos, nextPos, hb, m)) {
            // printf("Projectile hit ACTOR %d (hb %d)\n", idx, m);
            DeactivateProjectile(eng, projIdx);

            // Explosive types: explosion only
            ExplosionDef def = GetExplosionDef(pType);
            if (def.radius > 0.0f) {
              SpawnExplosion(gs, eng, soundSys, prevPos, pType, owner);
              return true;
            }

            SpawnSmoke(eng, prevPos);

            if (eng->em.masks[idx] & C_HITPOINT_TAG) {
              if (e != MakeEntityID(ET_ACTOR, gs->playerId)) {
                QueueSound(soundSys, SOUND_HITMARKER,
                           *(Vector3 *)getComponent(&eng->actors, gs->playerId,
                                                    gs->compReg.cid_Positions),
                           0.4f, 1.0f);
              } else {
                QueueSound(soundSys, SOUND_CLANG,
                           *(Vector3 *)getComponent(&eng->actors, gs->playerId,
                                                    gs->compReg.cid_Positions),
                           0.2f, 1.0f);
              }

              int damageDealt = projectileDamage[pType];
              eng->actors.hitPoints[idx] -= damageDealt;

              if (eng->actors.hitPoints[idx] <= 0.0f) {
                KillEntity(gs, eng, soundSys, MakeEntityID(ET_ACTOR, idx));
              }
            }

            return true;
          }
        }
      }
    }
  }

  return false;
}

static bool CheckTerrainHit(GameState_t *gs, Engine_t *eng,
                            SoundSystem_t *soundSys, int projIdx,
                            Vector3 prevPos, Vector3 nextPos) {
  float terrainY = GetTerrainHeightAtXZ(&gs->terrain, nextPos.x, nextPos.z);
  if (terrainY < nextPos.y)
    return false;

  int pType = eng->projectiles.types[projIdx];
  entity_t owner = eng->projectiles.owners[projIdx];

  ExplosionDef def = GetExplosionDef(pType);
  if (def.radius > 0.0f)
    SpawnExplosion(gs, eng, soundSys, prevPos, pType, owner);
  else
    SpawnDust(eng, prevPos);

  DeactivateProjectile(eng, projIdx);
  return true;
}

// Movement step that can be extended per projectile type.
// Returns computed next position, and updates velocity/aux state.
static Vector3 StepProjectile(GameState_t *gs, Engine_t *eng, int i, float dt) {
  Vector3 prevPos = eng->projectiles.positions[i];

  switch (eng->projectiles.types[i]) {

  case P_ROCKET: {
    // --- your existing rocket VFX + no gravity ---
    eng->projectiles.thrusterTimers[i] -= dt;
    if (eng->projectiles.thrusterTimers[i] <= 0.0f) {
      eng->projectiles.thrusterTimers[i] = 0.05f; // 20 Hz

      Vector3 v = eng->projectiles.velocities[i];
      float speed = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
      Vector3 dir = (speed > 0.001f)
                        ? (Vector3){v.x / speed, v.y / speed, v.z / speed}
                        : (Vector3){0, 0, 1};

      float length = 35.0f;
      Vector3 back = Vector3Add(prevPos, Vector3Scale(dir, -length * 0.5f));
      Vector3 thrusterPos = Vector3Add(back, Vector3Scale(dir, -2.0f));

      float randSpread = 1.0f;
      Vector3 jitter = {
          ((float)GetRandomValue(-100, 100) / 100.0f) * randSpread,
          ((float)GetRandomValue(-100, 100) / 100.0f) * randSpread,
          ((float)GetRandomValue(-100, 100) / 100.0f) * randSpread};

      Vector3 spawnPos = Vector3Add(thrusterPos, jitter);

      float life = 0.4f + ((float)GetRandomValue(0, 1000) / 1000.0f) * 0.4f;
      spawnParticle(eng, spawnPos, life, 1);
    }
    // no gravity for rockets
  } break;

  case P_MISSILE: {
    // --- homing missile: go straight up, then turn toward player ---
    Vector3 *playerPos = (Vector3 *)getComponent(&eng->actors, gs->playerId,
                                                 gs->compReg.cid_Positions);

    Vector3 v = eng->projectiles.velocities[i];
    float speed = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (speed < 0.001f)
      speed = 0.001f;

    // Optional: thruster VFX reuse
    eng->projectiles.thrusterTimers[i] -= dt;
    if (eng->projectiles.thrusterTimers[i] <= 0.0f) {
      eng->projectiles.thrusterTimers[i] = 0.05f;

      Vector3 dir = (Vector3){v.x / speed, v.y / speed, v.z / speed};
      float length = 35.0f;
      Vector3 back = Vector3Add(prevPos, Vector3Scale(dir, -length * 0.5f));
      Vector3 thrusterPos = Vector3Add(back, Vector3Scale(dir, -2.0f));

      Vector3 jitter = {((float)GetRandomValue(-100, 100) / 100.0f) * 1.0f,
                        ((float)GetRandomValue(-100, 100) / 100.0f) * 1.0f,
                        ((float)GetRandomValue(-100, 100) / 100.0f) * 1.0f};

      spawnParticle(eng, Vector3Add(thrusterPos, jitter), 0.4f, 1);
    }

    if (!playerPos) {
      // no target -> just keep current velocity
      // no gravity
      break;
    }

    // Phase 1: boost up
    if (eng->projectiles.homingDelays[i] > 0.0f) {
      eng->projectiles.homingDelays[i] -= dt;

      // force mostly upward motion
      eng->projectiles.velocities[i].x *= 0.90f;
      eng->projectiles.velocities[i].z *= 0.90f;
      eng->projectiles.velocities[i].y = speed; // keep upward

      // no gravity
      break;
    }

    // Phase 2: home toward player (smooth turning)
    Vector3 desiredDir = Vector3Normalize(Vector3Subtract(*playerPos, prevPos));
    Vector3 curDir = Vector3Normalize(v);

    float turn = eng->projectiles.homingTurnRates[i] * dt;
    if (turn > 1.0f)
      turn = 1.0f;

    // simple stable "lerp then renormalize"
    Vector3 newDir = Vector3Normalize((Vector3){
        curDir.x + (desiredDir.x - curDir.x) * turn,
        curDir.y + (desiredDir.y - curDir.y) * turn,
        curDir.z + (desiredDir.z - curDir.z) * turn,
    });

    eng->projectiles.velocities[i] = Vector3Scale(newDir, speed);

    // no gravity for missiles
  } break;

  default: {
    // Gravity drop acceleration for unguided bullets/plasma/etc.
    eng->projectiles.dropRates[i] += 0.5f;
    eng->projectiles.velocities[i].y -= eng->projectiles.dropRates[i] * dt;
  } break;
  }

  // integrate
  Vector3 vel = eng->projectiles.velocities[i];
  return Vector3Add(prevPos, Vector3Scale(vel, dt));
}

// ------------------------------------------------------------
// Refactored UpdateProjectiles
// ------------------------------------------------------------
void UpdateProjectiles(GameState_t *gs, Engine_t *eng, SoundSystem_t *soundSys,
                       float dt) {
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    if (!ProjectileIsActive(&eng->projectiles, i))
      continue;

    // Lifetime
    eng->projectiles.lifetimes[i] -= dt;
    if (eng->projectiles.lifetimes[i] <= 0.0f) {
      DeactivateProjectile(eng, i);
      continue;
    }

    Vector3 prevPos = eng->projectiles.positions[i];
    Vector3 nextPos = StepProjectile(gs, eng, i, dt);

    // Commit position
    eng->projectiles.positions[i] = nextPos;

    // Update projectile grid entry (you can remove projectile grid entirely
    // later if you want)
    UpdateProjectileGrid(gs, eng, i, prevPos, nextPos);

    // Collisions (early-out style instead of goto)
    if (CheckStaticHit(gs, eng, i, prevPos, nextPos))
      continue;
    if (CheckActorHit(gs, eng, soundSys, i, prevPos, nextPos))
      continue;
    if (CheckTerrainHit(gs, eng, soundSys, i, prevPos, nextPos))
      continue;
  }
}

void spawnProjectile(Engine_t *eng, Vector3 pos, Vector3 velocity,
                     float lifetime, float radius, float dropRate, int owner,
                     int type) {
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    if (eng->projectiles.active[i])
      continue;

    eng->projectiles.active[i] = true;
    eng->projectiles.positions[i] = pos;
    eng->projectiles.velocities[i] = velocity;
    eng->projectiles.lifetimes[i] = lifetime;
    eng->projectiles.radii[i] = radius;
    eng->projectiles.owners[i] = owner;
    eng->projectiles.types[i] = type;
    eng->projectiles.dropRates[i] = dropRate;
    eng->projectiles.thrusterTimers[i] = 0;

    if (type == P_MISSILE) {
      eng->projectiles.homingDelays[i] = 0.01f;   // goes straight up for 0.6s
      eng->projectiles.homingTurnRates[i] = 2.4f; // rad/sec, tune
    }
    break;
  }
}

void FireProjectile(Engine_t *eng, entity_t shooter, int rayIndex, int gunId,
                    int projType) {
  if (!eng->actors.raycasts[shooter][rayIndex].active)
    return;

  Ray *ray = &eng->actors.raycasts[shooter][rayIndex].ray;

  Vector3 origin = ray->position;
  Vector3 dir = Vector3Normalize(ray->direction);

  if (projType == P_MISSILE) {
    dir = (Vector3){0.0f, 1.0f, 0.0f}; // straight up
  }

  // Load weapon stats from components
  float muzzleVel = eng->actors.muzzleVelocities[shooter][gunId]
                        ? eng->actors.muzzleVelocities[shooter][gunId]
                        : 10.0f; // default fallback

  float drop = eng->actors.dropRates[shooter][gunId]
                   ? eng->actors.dropRates[shooter][gunId]
                   : 1.0f;

  if (projType == P_MISSILE)
    drop = 0.0f;

  Vector3 vel = Vector3Scale(dir, muzzleVel);
  // printf("spawning projectile\n");

  // printf("Fire vel: (%.3f, %.3f, %.3f)\n", vel.x, vel.y, vel.z);

  spawnProjectile(eng, origin, vel,
                  10.0f,     // lifetime sec
                  0.5f,      // radius
                  drop,      // drop rate
                  shooter,   // owner
                  projType); // type
}
