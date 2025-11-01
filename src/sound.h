
// sound.h
// Audio system: assets, events, and processing

#pragma once
#include "raylib.h"

#define MAX_SOUNDS 128
#define MAX_SOUND_EVENTS 256
#define MAX_SIMULTANEOUS_SOUNDS 32
#define SOUND_ALIASES 16

typedef enum {
  SOUND_FOOTSTEP,
  SOUND_WEAPON_FIRE,
  SOUND_EXPLOSION,
  SOUND_UI_CLICK,
  SOUND_COUNT
} SoundType_t;

typedef struct {
  Sound sound;
  Sound alias[SOUND_ALIASES];
  int nextAlias;
} SoundAsset_t;

typedef struct {
  SoundType_t type;
  Vector3 position;
  float volume;
  float pitch;
} SoundEvent_t;

typedef struct {
  SoundAsset_t assets[MAX_SOUNDS];
  SoundEvent_t events[MAX_SOUND_EVENTS];
  int eventCount;
} SoundSystem_t;

SoundSystem_t InitSoundSystem(void);
void QueueSound(SoundSystem_t *sys, SoundType_t type, Vector3 pos, float vol,
                float pitch);
void ProcessSoundSystem(SoundSystem_t *sys, Vector3 listenerPos);
