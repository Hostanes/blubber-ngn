
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

  sys.assets[SOUND_AMBIENT_DESERT].sound =
      LoadSound("assets/audio/desert-ambience-1.wav");

  for (int i = 0; i < SOUND_COUNT; i++) {
    for (int j = 0; j < SOUND_ALIASES; j++) {
      sys.assets[i].alias[j] = LoadSoundAlias(sys.assets[i].sound);
    }
    sys.assets[i].nextAlias = 0;
  }

  sys.ambient.type = SOUND_AMBIENT_DESERT;
  sys.ambient.pauseTime = 5.0f; // 5 second gap
  sys.ambient.volume = 0.35f;   // tweak
  sys.ambient.pitch = 1.0f;
  sys.ambient.timer = 5.0f; // play immediately (or set to 5.0f to delay)
  sys.ambient.enabled = true;

  return sys;
}

static float GetSoundLengthSec(Sound s) {
  unsigned int sr = s.stream.sampleRate;
  if (sr == 0)
    return 0.0f;
  return (float)s.frameCount / (float)sr;
}

static void UpdateAmbient(SoundSystem_t *sys) {
  AmbientLoop_t *a = &sys->ambient;
  if (!a->enabled)
    return;

  // only if timer elapsed
  if (a->timer > 0.0f)
    return;

  // play one instance using your alias system (so it doesn't cut other sounds)
  SoundAsset_t *asset = &sys->assets[a->type];

  int idx = asset->nextAlias;
  asset->nextAlias = (asset->nextAlias + 1) % SOUND_ALIASES;
  Sound *sound = &asset->alias[idx];

  SetSoundVolume(*sound, a->volume);
  SetSoundPitch(*sound, a->pitch);
  SetSoundPan(*sound, 0.5f); // centered ambience

  PlaySoundMultiCompat(*sound);

  float len = GetSoundLengthSec(*sound);
  if (len <= 0.0f)
    len = 32.0f; // fallback if metadata missing
  a->timer = len + a->pauseTime;
}

void UpdateSoundSystem(SoundSystem_t *sys, Engine_t *eng, GameState_t *gs,
                       float dt) {
  // 1) process queued positional events (your existing behavior)
  ProcessSoundSystem(sys, eng, gs);

  // 2) tick ambient timer
  if (sys->ambient.enabled) {
    sys->ambient.timer -= dt;
    if (sys->ambient.timer < 0.0f)
      sys->ambient.timer = 0.0f;
  }

  // 3) trigger ambient when ready
  UpdateAmbient(sys);
}

void EnableDesertAmbience(SoundSystem_t *sys, bool enabled) {
  sys->ambient.enabled = enabled;
  if (enabled && sys->ambient.timer > 1.0f) {
    // optional: play sooner when enabling
    sys->ambient.timer = 0.0f;
  }
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

  const float REF_DIST = 2.0f;

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
    float atten;
    if (dist <= REF_DIST) {
      atten = 1.0f;
    } else {
      float t = (dist - REF_DIST) / 1000.0f; // 40m falloff range (tweak)
      t = Clamp(t, 0.0f, 1.0f);
      atten = 1.0f - t; // linear fade
    }

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
