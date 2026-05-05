#include "sound.h"
#include "../../game/game.h"
#include <math.h>
#include <raymath.h>

SoundSystem_t InitSoundSystem(void) {
  SoundSystem_t sys = {0};
  InitAudioDevice();

  sys.assets[SOUND_FOOTSTEP].sound     = LoadSound("assets/audio/mech_step_1.wav");
  sys.assets[SOUND_WEAPON_FIRE].sound  = LoadSound("assets/audio/cannon_shot_1.wav");
  sys.assets[SOUND_EXPLOSION].sound    = LoadSound("assets/audio/explosion1.wav");
  sys.assets[SOUND_HITMARKER].sound    = LoadSound("assets/audio/hitmarker-sound-effect-sound.wav");
  sys.assets[SOUND_ROCKET_FIRE].sound  = LoadSound("assets/audio/rocket-launcher.wav");
  sys.assets[SOUND_CLANG].sound        = LoadSound("assets/audio/metal-clang.wav");
  sys.assets[SOUND_AMBIENT_DESERT].sound = LoadSound("assets/audio/desert-ambience-1.wav");

  for (int i = 0; i < SOUND_COUNT; i++) {
    for (int j = 0; j < SOUND_ALIASES; j++)
      sys.assets[i].alias[j] = LoadSoundAlias(sys.assets[i].sound);
    sys.assets[i].nextAlias = 0;
  }

  sys.ambient.type      = SOUND_AMBIENT_DESERT;
  sys.ambient.pauseTime = 5.0f;
  sys.ambient.volume    = 0.15f;
  sys.ambient.pitch     = 1.0f;
  sys.ambient.timer     = 0.0f;
  sys.ambient.enabled   = true;

  return sys;
}

void ShutdownSoundSystem(SoundSystem_t *sys) {
  for (int i = 0; i < SOUND_COUNT; i++) {
    for (int j = 0; j < SOUND_ALIASES; j++)
      UnloadSoundAlias(sys->assets[i].alias[j]);
    UnloadSound(sys->assets[i].sound);
  }
  CloseAudioDevice();
}

static float GetSoundLengthSec(Sound s) {
  if (s.stream.sampleRate == 0) return 0.0f;
  return (float)s.frameCount / (float)s.stream.sampleRate;
}

static void UpdateAmbient(SoundSystem_t *sys) {
  AmbientLoop_t *a = &sys->ambient;
  if (!a->enabled || a->timer > 0.0f) return;

  SoundAsset_t *asset = &sys->assets[a->type];
  int idx = asset->nextAlias;
  asset->nextAlias = (asset->nextAlias + 1) % SOUND_ALIASES;
  Sound *snd = &asset->alias[idx];

  SetSoundVolume(*snd, a->volume);
  SetSoundPitch(*snd, a->pitch);
  SetSoundPan(*snd, 0.5f);
  PlaySound(*snd);

  float len = GetSoundLengthSec(*snd);
  if (len <= 0.0f) len = 32.0f;
  a->timer = len + a->pauseTime;
}

void QueueSound(SoundSystem_t *sys, SoundType_t type, Vector3 pos,
                float vol, float pitch) {
  if (sys->eventCount < MAX_SOUND_EVENTS)
    sys->events[sys->eventCount++] = (SoundEvent_t){type, pos, vol, pitch};
}

static void ProcessSounds(SoundSystem_t *sys, world_t *world, GameWorld *game) {
  Position    *playerPos = ECS_GET(world, game->player, Position,    COMP_POSITION);
  Orientation *playerOri = ECS_GET(world, game->player, Orientation, COMP_ORIENTATION);
  if (!playerPos || !playerOri) { sys->eventCount = 0; return; }

  Vector3 listenerPos = playerPos->value;
  float   listenerYaw = playerOri->yaw;

  // Right vector in XZ derived from player yaw: forward=(sin,0,cos) → right=(cos,0,-sin)
  Vector3 right = {cosf(listenerYaw), 0.0f, -sinf(listenerYaw)};

  // Inverse-distance rolloff: full volume within REF_DIST, then 1/d decay.
  // No hard cutoff — sounds fade continuously rather than clamping to zero.
  const float REF_DIST = 5.0f;

  for (int i = 0; i < sys->eventCount; i++) {
    SoundEvent_t  ev    = sys->events[i];
    SoundAsset_t *asset = &sys->assets[ev.type];

    int idx = asset->nextAlias;
    asset->nextAlias = (asset->nextAlias + 1) % SOUND_ALIASES;
    Sound *snd = &asset->alias[idx];

    float dist = Vector3Distance(listenerPos, ev.position);

    // atten = 1.0 inside REF_DIST, then falls as REF_DIST/dist beyond it
    float atten = REF_DIST / fmaxf(dist, REF_DIST);

    SetSoundVolume(*snd, ev.volume * atten);

    SetSoundPitch(*snd, ev.pitch);

    // Stereo pan
    float pan = 0.5f;
    if (dist >= 0.001f) {
      Vector3 toSrc = Vector3Subtract(ev.position, listenerPos);
      toSrc.y = 0.0f;
      float len = sqrtf(toSrc.x * toSrc.x + toSrc.z * toSrc.z);
      if (len > 0.001f) { toSrc.x /= len; toSrc.z /= len; }

      float lr = toSrc.x * right.x + toSrc.z * right.z;
      float strength = 0.6f * (0.3f + 0.7f * (1.0f - Clamp(dist / 40.0f, 0.0f, 1.0f)));
      pan = Clamp(0.5f + lr * strength, 0.0f, 1.0f);

      // Near sounds: slight diffusion, very close sounds center out
      if (dist < 4.0f) {
        pan += GetRandomValue(-30, 30) / 1000.0f;
        pan = Clamp(pan, 0.0f, 1.0f);
      }
      if (dist < 2.0f) pan = 0.5f;
    }
    SetSoundPan(*snd, pan);

    PlaySound(*snd);
  }

  sys->eventCount = 0;
}

void UpdateSoundSystem(SoundSystem_t *sys, world_t *world, GameWorld *game,
                       float dt) {
  ProcessSounds(sys, world, game);

  if (sys->ambient.enabled) {
    sys->ambient.timer -= dt;
    if (sys->ambient.timer < 0.0f) sys->ambient.timer = 0.0f;
  }
  UpdateAmbient(sys);
}

void EnableDesertAmbience(SoundSystem_t *sys, bool enabled) {
  sys->ambient.enabled = enabled;
  if (enabled) sys->ambient.timer = 0.0f;
}
