#include "../game.h"
#include "systems.h"
#include <math.h>

#define COOLANT_PICKUP_RADIUS  2.0f   // XZ-only — player eye is ~1.8m above orbs
#define COOLANT_GRAVITY        18.0f
#define COOLANT_BOUNCE_DAMP    0.45f
#define COOLANT_TRAIL_INTERVAL 0.05f

void CoolantSystem(world_t *world, GameWorld *game, float dt) {
  archetype_t *arch = WorldGetArchetype(world, game->coolantArchId);
  if (!arch) return;

  Position         *ppos = ECS_GET(world, game->player, Position,          COMP_POSITION);
  MuzzleCollection_t *mc = ECS_GET(world, game->player, MuzzleCollection_t, COMP_MUZZLES);
  if (!ppos) return;

  for (uint32_t i = 0; i < arch->count; i++) {
    entity_t e = arch->entities[i];
    Active   *active = ECS_GET(world, e, Active,    COMP_ACTIVE);
    if (!active || !active->value) continue;

    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
    Velocity *vel = ECS_GET(world, e, Velocity, COMP_VELOCITY);
    Coolant  *co  = ECS_GET(world, e, Coolant,  COMP_COOLANT);
    if (!pos || !vel || !co) continue;

    co->lifetime -= dt;
    if (co->lifetime <= 0.0f) { active->value = false; continue; }

    // Gravity
    vel->value.y -= COOLANT_GRAVITY * dt;

    // Terrain bounce
    float ty = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap,
                                              pos->value.x, pos->value.z);
    if (pos->value.y <= ty) {
      pos->value.y = ty;
      vel->value.y = fabsf(vel->value.y) * COOLANT_BOUNCE_DAMP;
    }

    pos->value.x += vel->value.x * dt;
    pos->value.y += vel->value.y * dt;
    pos->value.z += vel->value.z * dt;

    // Particle trail — spray behind the direction of travel
    co->particleTimer -= dt;
    if (co->particleTimer <= 0.0f) {
      co->particleTimer = COOLANT_TRAIL_INTERVAL;

      float spd = sqrtf(vel->value.x*vel->value.x +
                        vel->value.y*vel->value.y +
                        vel->value.z*vel->value.z);
      if (spd > 0.5f) {
        // Unit vector opposite to travel direction
        float ix = -vel->value.x / spd;
        float iy = -vel->value.y / spd;
        float iz = -vel->value.z / spd;

        // Spawn 2 particles per tick for a denser trail
        for (int p = 0; p < 2; p++) {
          float spread = 1.2f;
          float pvx = ix * (spd * 0.3f) + ((float)GetRandomValue(-100,100)/100.0f) * spread;
          float pvz = iz * (spd * 0.3f) + ((float)GetRandomValue(-100,100)/100.0f) * spread;
          // Always fall — random downward speed, never upward
          float pvy = -((float)GetRandomValue(150, 400) / 100.0f);

          Vector3 spawnPos = {
            pos->value.x + ix * 0.25f,
            pos->value.y + iy * 0.25f,
            pos->value.z + iz * 0.25f,
          };
          SpawnParticle(world, game, spawnPos, (Vector3){pvx, pvy, pvz},
                        0.28f, 0.7f, (Color){40, 160, 255, 200});
        }
      }
    }

    // Pickup — XZ only: player eye is high above orb, 3D distance always fails
    float dx = ppos->value.x - pos->value.x;
    float dz = ppos->value.z - pos->value.z;
    if (sqrtf(dx*dx + dz*dz) < COOLANT_PICKUP_RADIUS) {
      if (mc) {
        for (int k = 0; k < mc->count; k++) {
          Muzzle_t *m = &mc->Muzzles[k];
          m->heat -= 0.20f;
          if (m->heat < 0.0f) m->heat = 0.0f;
          if (m->isOverheated && m->heat < m->overheatThreshold)
            m->isOverheated = false;
        }
      }
      active->value = false;
    }
  }
}
