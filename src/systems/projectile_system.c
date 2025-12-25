#include "systems.h"
#include <raylib.h>

void UpdateProjectiles(GameState_t *gs, Engine_t *eng, SoundSystem_t *soundSys,
                       float dt) {
  for (int i = 0; i < MAX_PROJECTILES; i++) {

    if (!eng->projectiles.active[i])
      continue;

    //------------------------------
    // LIFETIME
    //------------------------------
    eng->projectiles.lifetimes[i] -= dt;
    if (eng->projectiles.lifetimes[i] <= 0.0f) {
      eng->projectiles.active[i] = false;
      continue;
    }

    //------------------------------
    // MOVEMENT + GRAVITY
    //------------------------------
    Vector3 prevPos = eng->projectiles.positions[i];

    // Gravity drop acceleration
    eng->projectiles.dropRates[i] += 0.5f;
    eng->projectiles.velocities[i].y -= eng->projectiles.dropRates[i] * dt;

    // Compute nextPos *before* moving
    Vector3 vel = eng->projectiles.velocities[i];
    Vector3 nextPos = Vector3Add(prevPos, Vector3Scale(vel, dt));

    // Update actual projectile position
    eng->projectiles.positions[i] = nextPos;

    //------------------------------
    // UPDATE GRID POSITION
    //------------------------------
    GridRemoveEntity(&gs->grid, MakeEntityID(ET_PROJECTILE, i), prevPos);
    GridAddEntity(&gs->grid, MakeEntityID(ET_PROJECTILE, i), nextPos);

    //------------------------------
    // TERRAIN COLLISION (swept)
    //------------------------------
    float terrainY = GetTerrainHeightAtXZ(&gs->terrain, nextPos.x, nextPos.z);

    if (terrainY >= nextPos.y) {
      eng->projectiles.active[i] = false;
      SpawnDust(eng, prevPos);
      continue;
    }

    //------------------------------
    // GRID CELL COORDS
    //------------------------------
    int cx = (int)((nextPos.x - gs->grid.minX) / gs->grid.cellSize);
    int cz = (int)((nextPos.z - gs->grid.minZ) / gs->grid.cellSize);

    // ------------------------------
    // ===== STATIC COLLISION =====
    // ------------------------------
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

          ModelCollection_t *hb = &eng->statics.hitboxCollections[s];
          if (hb->countModels <= 0)
            continue;

          // test EACH hitbox
          for (int m = 0; m < hb->countModels; m++) {
            if (!hb->isActive[m])
              continue;

            if (SegmentIntersectsOBB(prevPos, nextPos, hb, m)) {
              printf("Projectile hit STATIC %d (hb %d)\n", s, m);

              SpawnMetalDust(eng, prevPos);
              eng->projectiles.active[i] = false;
              goto next_projectile;
            }
          }
        }
      }
    }

    // ------------------------------
    // ===== ACTOR COLLISION =====
    // ------------------------------
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

          ModelCollection_t *hb = &eng->actors.hitboxCollections[idx];
          if (hb->countModels <= 0)
            continue;

          // test EACH hitbox
          for (int m = 0; m < hb->countModels; m++) {
            if (!hb->isActive[m])
              continue;

            if (SegmentIntersectsOBB(prevPos, nextPos, hb, m)) {
              printf("Projectile hit ACTOR %d (hb %d)\n", idx, m);

              spawnParticle(eng, prevPos, 2, 1);
              eng->projectiles.active[i] = false;

              // existing HP logic
              if (eng->em.masks[idx] & C_HITPOINT_TAG) {
                eng->actors.hitPoints[idx] -= 10.0f;
                if (eng->actors.hitPoints[idx] <= 0) {
                  KillEntity(gs, eng, soundSys, MakeEntityID(ET_ACTOR, idx));
                }
              }

              goto next_projectile;
            }
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
