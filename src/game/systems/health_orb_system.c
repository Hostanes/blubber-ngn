#include "../game.h"
#include "systems.h"
#include <math.h>

#define HEALTH_ORB_PICKUP_RADIUS  2.0f
#define HEALTH_ORB_GRAVITY        18.0f
#define HEALTH_ORB_BOUNCE_DAMP    0.45f
#define HEALTH_ORB_TRAIL_INTERVAL 0.05f
#define HEALTH_ORB_HEAL           25.0f

void HealthOrbSystem(world_t *world, GameWorld *game, float dt) {
  archetype_t *arch = WorldGetArchetype(world, game->healthOrbArchId);
  if (!arch) return;

  Position *ppos  = ECS_GET(world, game->player, Position, COMP_POSITION);
  Health   *phealth = ECS_GET(world, game->player, Health,   COMP_HEALTH);
  if (!ppos) return;

  for (uint32_t i = 0; i < arch->count; i++) {
    entity_t e = arch->entities[i];
    Active   *active = ECS_GET(world, e, Active,     COMP_ACTIVE);
    if (!active || !active->value) continue;

    Position  *pos = ECS_GET(world, e, Position,  COMP_POSITION);
    Velocity  *vel = ECS_GET(world, e, Velocity,  COMP_VELOCITY);
    HealthOrb *ho  = ECS_GET(world, e, HealthOrb, COMP_HEALTH_ORB);
    if (!pos || !vel || !ho) continue;

    ho->lifetime -= dt;
    if (ho->lifetime <= 0.0f) { active->value = false; continue; }

    // Gravity
    vel->value.y -= HEALTH_ORB_GRAVITY * dt;

    // Terrain bounce
    float ty = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap,
                                              pos->value.x, pos->value.z);
    if (pos->value.y <= ty) {
      pos->value.y  = ty;
      vel->value.y  = fabsf(vel->value.y) * HEALTH_ORB_BOUNCE_DAMP;
    }

    pos->value.x += vel->value.x * dt;
    pos->value.y += vel->value.y * dt;
    pos->value.z += vel->value.z * dt;

    // Green particle trail
    ho->particleTimer -= dt;
    if (ho->particleTimer <= 0.0f) {
      ho->particleTimer = HEALTH_ORB_TRAIL_INTERVAL;

      float spd = sqrtf(vel->value.x*vel->value.x +
                        vel->value.y*vel->value.y +
                        vel->value.z*vel->value.z);
      if (spd > 0.5f) {
        float ix = -vel->value.x / spd;
        float iy = -vel->value.y / spd;
        float iz = -vel->value.z / spd;
        for (int p = 0; p < 2; p++) {
          float spread = 1.0f;
          float pvx = ix * (spd * 0.3f) + ((float)GetRandomValue(-100,100)/100.0f) * spread;
          float pvz = iz * (spd * 0.3f) + ((float)GetRandomValue(-100,100)/100.0f) * spread;
          float pvy = -((float)GetRandomValue(100, 300) / 100.0f);
          Vector3 spawnPos = {
            pos->value.x + ix * 0.2f,
            pos->value.y + iy * 0.2f,
            pos->value.z + iz * 0.2f,
          };
          SpawnParticle(world, game, spawnPos, (Vector3){pvx, pvy, pvz},
                        0.22f, 0.6f, (Color){60, 230, 80, 200});
        }
      }
    }

    // Pickup — XZ only (same reason as coolant: player eye height)
    float dx = ppos->value.x - pos->value.x;
    float dz = ppos->value.z - pos->value.z;
    if (sqrtf(dx*dx + dz*dz) < HEALTH_ORB_PICKUP_RADIUS) {
      if (phealth) {
        phealth->current += HEALTH_ORB_HEAL;
        if (phealth->current > phealth->max)
          phealth->current = phealth->max;
      }
      active->value = false;
    }
  }
}
