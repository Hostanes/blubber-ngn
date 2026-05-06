#include "../game.h"
#include "../level_creater_helper.h"
#include "systems.h"
#include <math.h>
#include <raylib.h>
#include <raymath.h>

#define HOOK_SPEED       50.0f
#define HOOK_MAX_RANGE   55.0f
#define HOOK_HIT_RADIUS   2.0f
#define HOOK_ARRIVE_DIST  3.0f
#define HOOK_PULL_FORCE  38.0f

void BlunderbussSystem(world_t *world, GameWorld *game,
                       entity_t player, Camera3D *camera, float dt) {
  (void)camera;

  MuzzleCollection_t *muzzles = ECS_GET(world, player, MuzzleCollection_t, COMP_MUZZLES);
  if (!muzzles || muzzles->count < 4) return;

  Muzzle_t *m = &muzzles->Muzzles[3];

  // Clear hook state when weapon is not active
  if (game->playerActiveWeapon != 3) {
    game->hookState = HOOKSTATE_IDLE;
    return;
  }

  Orientation *ori = ECS_GET(world, player, Orientation, COMP_ORIENTATION);
  Position    *pos = ECS_GET(world, player, Position,    COMP_POSITION);
  Velocity    *vel = ECS_GET(world, player, Velocity,    COMP_VELOCITY);
  if (!ori || !pos || !vel) return;

  Vector3 forward3D = {
    cosf(ori->pitch) * sinf(ori->yaw),
    sinf(ori->pitch),
    cosf(ori->pitch) * cosf(ori->yaw),
  };

  // ------------------------------------------------------------------
  // LMB — spread shot (single press, semi-auto)
  // ------------------------------------------------------------------
  if (!m->isOverheated && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
    FireMuzzle(world, game, player, game->playerArchId, m);

    m->heat += m->heatPerShot;
    if (m->heat >= 1.0f) { m->heat = 1.0f; m->isOverheated = true; }
    m->coolDelayTimer = m->coolDelay;
    m->recoil = 0.3f;

    // Muzzle flash particles
    for (int p = 0; p < 4; p++) {
      float jx = (float)GetRandomValue(-100, 100) / 100.0f;
      float jz = (float)GetRandomValue(-100, 100) / 100.0f;
      Vector3 pvel = {jx * 0.6f, 0.5f + (float)GetRandomValue(10, 50) / 100.0f, jz * 0.6f};
      SpawnParticle(world, game, m->worldPosition, pvel, 0.09f, 0.4f,
                    (Color){255, 180, 60, 200});
    }

    QueueSound(&game->soundSystem, SOUND_WEAPON_FIRE, m->worldPosition, 0.7f, 0.45f);
  }

  // ------------------------------------------------------------------
  // RMB — fire hook or cancel
  // ------------------------------------------------------------------
  if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
    if (game->hookState == HOOKSTATE_IDLE) {
      game->hookState  = HOOKSTATE_FLYING;
      game->hookOrigin = m->worldPosition;
      game->hookPos    = m->worldPosition;
      game->hookVel    = Vector3Scale(forward3D, HOOK_SPEED);
    } else {
      game->hookState = HOOKSTATE_IDLE;
    }
  }

  // ------------------------------------------------------------------
  // Hook — fly and stick
  // ------------------------------------------------------------------
  if (game->hookState == HOOKSTATE_FLYING) {
    game->hookPos = Vector3Add(game->hookPos, Vector3Scale(game->hookVel, dt));

    float range = Vector3Distance(game->hookPos, game->hookOrigin);
    if (range > HOOK_MAX_RANGE) {
      game->hookState = HOOKSTATE_IDLE;
    } else {
      uint32_t archIds[] = {
        game->enemyGruntArchId,
        game->enemyRangerArchId,
        game->enemyMeleeArchId,
        game->enemyDroneArchId,
      };
      for (int ai = 0; ai < 4 && game->hookState == HOOKSTATE_FLYING; ai++) {
        archetype_t *arch = WorldGetArchetype(world, archIds[ai]);
        if (!arch) continue;
        for (uint32_t i = 0; i < arch->count; i++) {
          entity_t e = arch->entities[i];
          Active *act = ECS_GET(world, e, Active, COMP_ACTIVE);
          if (!act || !act->value) continue;
          Position *ep = ECS_GET(world, e, Position, COMP_POSITION);
          if (!ep) continue;
          if (Vector3Distance(game->hookPos, ep->value) < HOOK_HIT_RADIUS) {
            game->hookState  = HOOKSTATE_PULLING;
            game->hookTarget = e;
            game->hookPos    = ep->value;
            break;
          }
        }
      }
    }
  }

  // ------------------------------------------------------------------
  // Hook — pull player toward stuck target
  // ------------------------------------------------------------------
  if (game->hookState == HOOKSTATE_PULLING) {
    Active *ta = ECS_GET(world, game->hookTarget, Active, COMP_ACTIVE);
    if (!ta || !ta->value) {
      game->hookState = HOOKSTATE_IDLE;
      return;
    }

    Position *tp = ECS_GET(world, game->hookTarget, Position, COMP_POSITION);
    if (!tp) { game->hookState = HOOKSTATE_IDLE; return; }

    game->hookPos = tp->value;

    float dx = tp->value.x - pos->value.x;
    float dy = tp->value.y - pos->value.y;
    float dz = tp->value.z - pos->value.z;
    float dist = sqrtf(dx*dx + dy*dy + dz*dz);

    if (dist < HOOK_ARRIVE_DIST) {
      game->hookState = HOOKSTATE_IDLE;
    } else {
      float inv = 1.0f / dist;
      vel->value.x += dx * inv * HOOK_PULL_FORCE;
      vel->value.y += dy * inv * HOOK_PULL_FORCE;
      vel->value.z += dz * inv * HOOK_PULL_FORCE;
    }
  }
}
