#include "systems.h"

void UpdateProjectiles(GameState_t *gs, Engine_t *eng, float dt) {
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    if (!eng->projectiles.active[i])
      continue;

    // Lifetime
    eng->projectiles.lifetimes[i] -= dt;
    if (eng->projectiles.lifetimes[i] <= 0.0f) {
      eng->projectiles.active[i] = false;
      continue;
    }

    Vector3 prevPos = eng->projectiles.positions[i];

    // Gravity
    eng->projectiles.velocities[i].y -= eng->projectiles.dropRates[i] * dt;

    // Move
    eng->projectiles.positions[i] =
        Vector3Add(eng->projectiles.positions[i],
                   Vector3Scale(eng->projectiles.velocities[i], dt));

    Vector3 projPos = eng->projectiles.positions[i];

    // Update grid
    GridRemoveEntity(&gs->grid, MakeEntityID(ET_PROJECTILE, i), prevPos);
    GridAddEntity(&gs->grid, MakeEntityID(ET_PROJECTILE, i), projPos);

    // ===== TERRAIN COLLISION =====
    if (GetTerrainHeightAtXZ(&gs->terrain, projPos.x, projPos.z) >= projPos.y) {
      eng->projectiles.active[i] = false;
      spawnParticle(eng, prevPos, 5, 2);
      continue;
    }

    // ===== GRID CELL COORDS =====
    int cx = (int)((projPos.x - gs->grid.minX) / gs->grid.cellSize);
    int cz = (int)((projPos.z - gs->grid.minZ) / gs->grid.cellSize);

    // ===== STATIC COLLISION IN NEIGHBORING CELLS =====
    for (int dx = -1; dx <= 1; dx++) {
      for (int dz = -1; dz <= 1; dz++) {
        int nx = cx + dx;
        int nz = cz + dz;
        if (!IsCellValid(&gs->grid, nx, nz))
          continue;

        GridNode_t *node = &gs->grid.nodes[nx][nz];
        for (int n = 0; n < node->count; n++) {
          entity_t e = node->entities[n];
          if (GetEntityCategory(e) != ET_STATIC)
            continue;

          int s = GetEntityIndex(e);
          if (!eng->statics.modelCollections[s].countModels)
            continue;

          if (ProjectileIntersectsEntityOBB(eng, i, e)) {
            printf("PROJECTILE: hit Static ID %d\n", s);
            spawnParticle(eng, prevPos, 1, 1);
            eng->projectiles.active[i] = false;
            goto next_projectile;
          }
        }
      }
    }

    // ===== ACTOR COLLISION IN NEIGHBORING CELLS =====
    for (int dx = -1; dx <= 1; dx++) {
      for (int dz = -1; dz <= 1; dz++) {
        int nx = cx + dx;
        int nz = cz + dz;
        if (!IsCellValid(&gs->grid, nx, nz))
          continue;

        GridNode_t *node = &gs->grid.nodes[nx][nz];
        for (int n = 0; n < node->count; n++) {
          entity_t e = node->entities[n];
          if (GetEntityCategory(e) != ET_ACTOR)
            continue;

          int idx = GetEntityIndex(e);
          if (!(eng->em.masks[idx] & C_HITBOX))
            continue;
          if (!eng->em.alive[idx])
            continue;
          if (idx == eng->projectiles.owners[i])
            continue;

          if (ProjectileIntersectsEntityOBB(eng, i, e)) {
            spawnParticle(eng, prevPos, 2, 1);
            eng->projectiles.active[i] = false;

            printf("PROJECTILE: hit Actor ID %d\n", idx);

            if (eng->em.masks[idx] & C_HITPOINT_TAG) {
              eng->actors.hitPoints[idx] -= 50.0f;
              if (eng->actors.hitPoints[idx] <= 0)
                eng->em.alive[idx] = 0;
            }
            goto next_projectile;
          }
        }
      }
    }

  next_projectile:;
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
    break;
  }
}

void FireProjectile(Engine_t *eng, entity_t shooter, int rayIndex) {
  if (!eng->actors.raycasts[shooter][rayIndex].active)
    return;

  int gunId = 0;

  Ray *ray = &eng->actors.raycasts[shooter][rayIndex].ray;

  Vector3 origin = ray->position;
  Vector3 dir = Vector3Normalize(ray->direction);

  // Load weapon stats from components
  float muzzleVel = eng->actors.muzzleVelocities[shooter][gunId]
                        ? eng->actors.muzzleVelocities[shooter][gunId]
                        : 10.0f; // default fallback

  float drop = eng->actors.dropRates[shooter][gunId]
                   ? eng->actors.dropRates[shooter][gunId]
                   : 1.0f;

  Vector3 vel = Vector3Scale(dir, muzzleVel);
  printf("spawning projectile\n");
  spawnProjectile(eng, origin, vel,
                  10.0f,   // lifetime sec
                  0.5f,    // radius
                  drop,    // drop rate
                  shooter, // owner
                  1);      // type
}
