#include "sound.h"
#include "../../game/game.h"
#include <math.h>
#include <raymath.h>

static const char *s_loopPaths[LOOP_SOUND_COUNT] = {
  "assets/audio/rocket-loop-99748.mp3",  // LOOP_SOUND_ROCKET
};

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
  sys.assets[SOUND_CHAIN_PULL].sound     = LoadSound("assets/audio/chain-pulled.wav");

  for (int i = 0; i < SOUND_COUNT; i++) {
    for (int j = 0; j < SOUND_ALIASES; j++)
      sys.assets[i].alias[j] = LoadSoundAlias(sys.assets[i].sound);
    sys.assets[i].nextAlias = 0;
  }

  // Pre-load one Music stream per loop slot (all rocket loops for now)
  for (int i = 0; i < MAX_LOOP_SOUNDS; i++) {
    sys.loopSlots[i].stream = LoadMusicStream(s_loopPaths[LOOP_SOUND_ROCKET]);
    sys.loopSlots[i].active = false;
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
  for (int i = 0; i < MAX_LOOP_SOUNDS; i++) {
    if (sys->loopSlots[i].active)
      StopMusicStream(sys->loopSlots[i].stream);
    UnloadMusicStream(sys->loopSlots[i].stream);
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

void TickLoopSound(SoundSystem_t *sys, LoopSoundType_t type,
                   entity_t owner, Vector3 pos, float vol, float pitch) {
  // Update existing slot if found
  for (int i = 0; i < MAX_LOOP_SOUNDS; i++) {
    if (sys->loopSlots[i].active && sys->loopSlots[i].owner.id == owner.id) {
      sys->loopSlots[i].position = pos;
      sys->loopSlots[i].baseVol  = vol;
      sys->loopSlots[i].pitch    = pitch;
      return;
    }
  }
  // Claim a free slot
  for (int i = 0; i < MAX_LOOP_SOUNDS; i++) {
    if (!sys->loopSlots[i].active) {
      sys->loopSlots[i].owner    = owner;
      sys->loopSlots[i].position = pos;
      sys->loopSlots[i].baseVol  = vol;
      sys->loopSlots[i].pitch    = pitch;
      sys->loopSlots[i].active   = true;
      PlayMusicStream(sys->loopSlots[i].stream);
      return;
    }
  }
  // Pool exhausted — silently drop
}

void StopLoopSound(SoundSystem_t *sys, entity_t owner) {
  for (int i = 0; i < MAX_LOOP_SOUNDS; i++) {
    if (sys->loopSlots[i].active && sys->loopSlots[i].owner.id == owner.id) {
      StopMusicStream(sys->loopSlots[i].stream);
      sys->loopSlots[i].active = false;
      return;
    }
  }
}

static void ApplySpatial(Vector3 listenerPos, Vector3 right,
                         Vector3 srcPos, float baseVol,
                         float *outVol, float *outPan) {
  const float REF_DIST = 5.0f;
  float dist  = Vector3Distance(listenerPos, srcPos);
  float atten = REF_DIST / fmaxf(dist, REF_DIST);
  *outVol = baseVol * atten;

  float pan = 0.5f;
  if (dist >= 0.001f) {
    Vector3 toSrc = Vector3Subtract(srcPos, listenerPos);
    toSrc.y = 0.0f;
    float len = sqrtf(toSrc.x * toSrc.x + toSrc.z * toSrc.z);
    if (len > 0.001f) { toSrc.x /= len; toSrc.z /= len; }
    float lr       = toSrc.x * right.x + toSrc.z * right.z;
    float strength = 0.6f * (0.3f + 0.7f * (1.0f - Clamp(dist / 40.0f, 0.0f, 1.0f)));
    pan = Clamp(0.5f + lr * strength, 0.0f, 1.0f);
    if (dist < 4.0f) {
      pan += GetRandomValue(-30, 30) / 1000.0f;
      pan  = Clamp(pan, 0.0f, 1.0f);
    }
    if (dist < 2.0f) pan = 0.5f;
  }
  *outPan = pan;
}

static void ProcessSounds(SoundSystem_t *sys, Vector3 listenerPos, Vector3 right) {
  for (int i = 0; i < sys->eventCount; i++) {
    SoundEvent_t  ev    = sys->events[i];
    SoundAsset_t *asset = &sys->assets[ev.type];

    int idx = asset->nextAlias;
    asset->nextAlias = (asset->nextAlias + 1) % SOUND_ALIASES;
    Sound *snd = &asset->alias[idx];

    float vol, pan;
    ApplySpatial(listenerPos, right, ev.position, ev.volume, &vol, &pan);

    SetSoundVolume(*snd, vol);
    SetSoundPitch(*snd, ev.pitch);
    SetSoundPan(*snd, pan);
    PlaySound(*snd);
  }
  sys->eventCount = 0;
}

static void UpdateLoopSounds(SoundSystem_t *sys, Vector3 listenerPos,
                              Vector3 right) {
  for (int i = 0; i < MAX_LOOP_SOUNDS; i++) {
    LoopSoundSlot_t *slot = &sys->loopSlots[i];
    if (!slot->active) continue;

    UpdateMusicStream(slot->stream);

    float vol, pan;
    ApplySpatial(listenerPos, right, slot->position, slot->baseVol, &vol, &pan);
    SetMusicVolume(slot->stream, vol);
    SetMusicPitch(slot->stream, slot->pitch);
    SetMusicPan(slot->stream, pan);
  }
}

void UpdateSoundSystem(SoundSystem_t *sys, world_t *world, GameWorld *game,
                       float dt) {
  Position    *playerPos = ECS_GET(world, game->player, Position,    COMP_POSITION);
  Orientation *playerOri = ECS_GET(world, game->player, Orientation, COMP_ORIENTATION);

  Vector3 listenerPos = playerPos ? playerPos->value : (Vector3){0, 0, 0};
  float   listenerYaw = playerOri ? playerOri->yaw   : 0.0f;
  Vector3 right       = {cosf(listenerYaw), 0.0f, -sinf(listenerYaw)};

  if (!playerPos || !playerOri) {
    sys->eventCount = 0;
  } else {
    ProcessSounds(sys, listenerPos, right);
    UpdateLoopSounds(sys, listenerPos, right);
  }

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
