// systems.c
// Implements player input, physics, and rendering
#include "systems.h"
#include "../engine.h"
#include "../game.h"
#include "../sound.h"
#include "raylib.h"
#include "rlgl.h"
#include <float.h>
#include <stdbool.h>

void LoadAssets() {
  Texture2D sandTex = LoadTexture("assets/textures/xtSand.png");
}

// ------------- Weapon Cooldowns -------------

void DecrementCooldowns(Engine_t *eng, GameState_t *gs, float dt) {
  for (int i = 0; i < eng->em.count; i++) {
    // Skip dead entities
    if (!eng->em.alive[i])
      continue;

    // Only process entities that have the cooldown component
    if (eng->em.masks[i] & C_COOLDOWN_TAG) {

      for (int j = 0; j < *(int *)getComponent(&eng->actors, i,
                                               gs->compReg.cid_weaponCount);
           j++) {

        eng->actors.cooldowns[i][j] -= dt;

        // Clamp to zero
        if (eng->actors.cooldowns[i][j] < 0.0f)
          eng->actors.cooldowns[i][j] = 0.0f;
      }
    }
  }
}

void UpdateGame(GameState_t *gs, Engine_t *eng, SoundSystem_t *soundSys,
                Camera3D *camera, float dt) {

  if (GetScreenHeight() != eng->config.window_height) {
    eng->config.window_width = GetScreenWidth();
    eng->config.window_height = GetScreenHeight();
  }

  if (gs->state == STATE_INLEVEL) {

    PlayerControlSystem(gs, eng, soundSys, dt, camera);

    int entity = 0; // player
    int rayIndex = 0;
    Raycast_t *rc = &eng->actors.raycasts[entity][rayIndex];
    ModelCollection_t *mc = &eng->actors.modelCollections[entity];

    UpdateRayCastToModel(gs, eng, rc, entity, 1);
    UpdateEntityRaycasts(eng, entity);

    DecrementCooldowns(eng, gs, dt);

    UpdateTorsoRecoil(&eng->actors.modelCollections[gs->playerId], 1, dt);

    UpdateEnemyTargets(gs, eng, soundSys, dt);
    UpdateEnemyVelocities(gs, eng, soundSys, dt);

    UpdateTankAimingAndShooting(gs, eng, soundSys, dt);
    UpdateTankTurretAiming(gs, eng, soundSys, dt);
    UpdateHarasserAimingAndShooting(gs, eng, soundSys, dt);

    PhysicsSystem(gs, eng, soundSys, dt);
    UpdateParticles(eng, dt);

    UpdateMessageBanner(gs, dt);

    RenderSystem(gs, eng, *camera);

    UpdateSoundSystem(soundSys, eng, gs, dt);

  } else if (gs->state == STATE_MAINMENU) {
    MainMenuSystem(gs, eng);
  }
}
