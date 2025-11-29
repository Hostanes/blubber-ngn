
// sound.c
// Implements sound system initialization, queuing, and processing

#include "sound.h"
#include "engine.h"
#include "game.h"
#include "raymath.h"
#include <raylib.h>

static Sound soundPool[MAX_SIMULTANEOUS_SOUNDS];
static int soundIndex = 0;

static void PlaySoundMultiCompat(Sound s) {
  soundIndex = (soundIndex + 1) % MAX_SIMULTANEOUS_SOUNDS;
  StopSound(soundPool[soundIndex]);
  soundPool[soundIndex] = s;
  PlaySound(soundPool[soundIndex]);
}

SoundSystem_t InitSoundSystem(void) {
  SoundSystem_t sys = {0};
  InitAudioDevice();

  // Load assets (expand as needed)
  sys.assets[SOUND_FOOTSTEP].sound = LoadSound("assets/audio/mech_step_1.wav");
  sys.assets[SOUND_WEAPON_FIRE].sound =
      LoadSound("assets/audio/cannon_shot_1.wav");
  sys.assets[SOUND_EXPLOSION].sound = LoadSound("assets/audio/explosion1.wav");

  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < SOUND_ALIASES; j++) {
      sys.assets[i].alias[j] = LoadSoundAlias(sys.assets[i].sound);
    }
    sys.assets[i].nextAlias = 0;
  }

  return sys;
}

void QueueSound(SoundSystem_t *sys, SoundType_t type, Vector3 pos, float vol,
                float pitch) {
  if (sys->eventCount < MAX_SOUND_EVENTS) {
    sys->events[sys->eventCount++] = (SoundEvent_t){type, pos, vol, pitch};
  }
}

float NearDampen(float dist) {
  if (dist >= 4.0f)
    return 1.0f;          // normal
  float t = dist / 4.0f;  // 1 → 0
  return 0.5f + 0.5f * t; // 0.5 → 1 (soft near)
}

void ProcessSoundSystem(SoundSystem_t *sys, Engine_t *eng, GameState_t *gs) {

  Vector3 listenerPos = *(Vector3 *)getComponent(&eng->actors, gs->playerId,
                                                 gs->compReg.cid_Positions);

  const float REF_DIST = 3.0f; // No extra loudness inside this range

  for (int i = 0; i < sys->eventCount; i++) {

    SoundEvent_t event = sys->events[i];
    SoundAsset_t *asset = &sys->assets[event.type];

    int idx = asset->nextAlias;
    asset->nextAlias = (asset->nextAlias + 1) % SOUND_ALIASES;

    Sound *sound = &asset->alias[idx];

    float dist = Vector3Distance(listenerPos, event.position);

    //------------------------------------
    // Distance attenuation with ref dist
    //------------------------------------
    float atten = (dist <= REF_DIST) ? 1.0f : (REF_DIST / dist);

    //------------------------------------
    // Near-field soften (under 4m)
    //------------------------------------
    float nearSoft = (dist >= 4.0f) ? 1.0f : (0.5f + 0.5f * (dist / 4.0f));

    //------------------------------------
    // Final volume
    //------------------------------------
    float volume = event.volume * atten * nearSoft;
    SetSoundVolume(*sound, volume);

    //------------------------------------
    // Soft pitch shift when extremely close
    //------------------------------------
    float pitchFactor = 1.0f - 0.1f * fmaxf(0.0f, (4.0f - dist) / 4.0f);
    SetSoundPitch(*sound, event.pitch * pitchFactor);

    //------------------------------------
    // Tiny stereo diffusion for near sounds
    //------------------------------------
    float pan = 0.5f;
    if (dist < 4.0f)
      pan += GetRandomValue(-30, 30) / 1000.0f; // ±0.03

    SetSoundPan(*sound, pan);

    //------------------------------------
    PlaySoundMultiCompat(*sound);
  }

  sys->eventCount = 0;
}
