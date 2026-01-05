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

#define WAVE_DEBUG 1

#if WAVE_DEBUG
#define WAVE_LOG(...) printf(__VA_ARGS__)
#else
#define WAVE_LOG(...)
#endif

static const char *WaveStateName(WaveState s) {
  switch (s) {
  case WAVE_WAITING:
    return "WAITING";
  case WAVE_SPAWNING:
    return "SPAWNING";
  case WAVE_ACTIVE:
    return "ACTIVE";
  case WAVE_COMPLETE:
    return "COMPLETE";
  case WAVE_FINISHED:
    return "FINISHED";
  default:
    return "UNKNOWN";
  }
}

void LoadAssets() {
  Texture2D sandTex = LoadTexture("assets/textures/xtSand.png");
}

typedef void (*WaveStartFn)(GameState_t *, Engine_t *);

static WaveStartFn gWaveStarts[MAX_WAVES] = {
    Wave1Start, Wave2Start, //
    Wave3Start, Wave4Start, //
    Wave5Start, Wave6Start, //
};

static void UpdateWaves(GameState_t *gs, Engine_t *eng, float dt) {
  WaveSystem_t *ws = &gs->waves;

  static WaveState lastState = -1;
  static int lastWaveIndex = -1;

  if (ws->state == WAVE_FINISHED) {
    if (lastState != WAVE_FINISHED) {
      WAVE_LOG("[WAVES] FINISHED all waves (%d total)\n", ws->totalWaves);
      lastState = WAVE_FINISHED;
    }
    return;
  }

  // Log state or wave changes once
  if (ws->state != lastState || ws->waveIndex != lastWaveIndex) {
    WAVE_LOG("[WAVES] State=%s | Wave=%d/%d | Alive=%d | Timer=%.2f\n",
             WaveStateName(ws->state), ws->waveIndex + 1, ws->totalWaves,
             ws->enemiesAliveThisWave, ws->betweenWaveTimer);
    lastState = ws->state;
    lastWaveIndex = ws->waveIndex;
  }

  switch (ws->state) {

  case WAVE_WAITING: {
    ws->betweenWaveTimer -= dt;

    // Optional: periodic countdown logging
    if ((int)ws->betweenWaveTimer != (int)(ws->betweenWaveTimer + dt)) {
      WAVE_LOG("[WAVES] Waiting... next wave in %.1f seconds\n",
               ws->betweenWaveTimer);
    }

    if (ws->betweenWaveTimer <= 0.0f) {
      WAVE_LOG("[WAVES] Transition WAITING → SPAWNING (Wave %d)\n",
               ws->waveIndex + 1);
      ws->state = WAVE_SPAWNING;
    }
  } break;

  case WAVE_SPAWNING: {
    if (ws->waveIndex >= ws->totalWaves || !gWaveStarts[ws->waveIndex]) {
      WAVE_LOG("[WAVES] No start function for wave %d — FINISHED\n",
               ws->waveIndex + 1);
      ws->state = WAVE_FINISHED;
      break;
    }

    WAVE_LOG("[WAVES] Spawning Wave %d\n", ws->waveIndex + 1);
    gWaveStarts[ws->waveIndex](gs, eng);

    WAVE_LOG("[WAVES] Wave %d ACTIVE (%d enemies)\n", ws->waveIndex + 1,
             ws->enemiesAliveThisWave);

    ws->state = WAVE_ACTIVE;
  } break;

  case WAVE_ACTIVE: {
    if (ws->enemiesAliveThisWave <= 0) {
      WAVE_LOG("[WAVES] Wave %d CLEARED\n", ws->waveIndex + 1);
      ws->state = WAVE_COMPLETE;
    }
  } break;

  case WAVE_COMPLETE: {
    ws->waveIndex++;

    float playerHP = eng->actors.hitPoints[gs->playerId];
    playerHP += 50;
    if (playerHP > 200) {
      playerHP = 200;
    }
    eng->actors.hitPoints[gs->playerId] = playerHP;

    TriggerMessage(gs, "Wave Complete!");
    if (ws->waveIndex >= ws->totalWaves) {
      WAVE_LOG("[WAVES] All waves complete — FINISHED\n");
      TriggerMessage(gs, "All waves complete, Well done!");
      ws->state = WAVE_FINISHED;
    } else {
      ws->betweenWaveTimer = ws->betweenWaveDelay;
      WAVE_LOG("[WAVES] Preparing Wave %d (starts in %.1f seconds)\n",
               ws->waveIndex + 1, ws->betweenWaveTimer);
      ws->state = WAVE_WAITING;
    }
  } break;

  default:
    break;
  }
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

    // Toggle pause
    if (IsKeyPressed(KEY_ESCAPE)) {
      gs->paused = !gs->paused;
      if (gs->paused)
        EnableCursor();
      else
        DisableCursor();
    }

    if (!gs->paused) {
      PlayerControlSystem(gs, eng, soundSys, dt, camera);

      int entity = 0; // player
      int rayIndex = 0;
      Raycast_t *rc = &eng->actors.raycasts[entity][rayIndex];
      UpdateRayCastToModel(gs, eng, rc, entity, 1);
      UpdateEntityRaycasts(eng, entity);

      DecrementCooldowns(eng, gs, dt);
      UpdateWaves(gs, eng, dt);

      UpdateTorsoRecoil(&eng->actors.modelCollections[gs->playerId], 1, dt);

      UpdateEnemyTargets(gs, eng, soundSys, dt);
      UpdateEnemyVelocities(gs, eng, soundSys, dt);

      UpdateTankAimingAndShooting(gs, eng, soundSys, dt);
      UpdateTankTurretAiming(gs, eng, soundSys, dt);
      UpdateHarasserAimingAndShooting(gs, eng, soundSys, dt);
      UpdateAlphaTankTurretAimingAndShooting(gs, eng, soundSys, dt);

      PhysicsSystem(gs, eng, soundSys, dt);
      UpdateParticles(eng, dt);

      UpdateMessageBanner(gs, dt);
    }

    // Always render (so pause shows the current scene)
    RenderSystem(gs, eng, *camera);

    // Optional: you can still update sound here or skip it when paused
    UpdateSoundSystem(soundSys, eng, gs, dt);

  } else if (gs->state == STATE_MAINMENU) {
    MainMenuSystem(gs, eng);
  }
}
