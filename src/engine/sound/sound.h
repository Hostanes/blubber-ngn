#pragma once
#include "raylib.h"
#include <stdbool.h>

// Forward declarations — full types in game.h / world.h
struct world_t;
typedef struct world_t world_t;
typedef struct GameWorld GameWorld;

#define MAX_SOUND_EVENTS      256
#define MAX_SIMULTANEOUS_SOUNDS 32
#define SOUND_ALIASES         32

typedef enum {
  SOUND_FOOTSTEP,
  SOUND_WEAPON_FIRE,
  SOUND_ROCKET_FIRE,
  SOUND_EXPLOSION,
  SOUND_HITMARKER,
  SOUND_CLANG,
  SOUND_AMBIENT_DESERT,
  SOUND_COUNT
} SoundType_t;

typedef struct {
  SoundType_t type;
  float pauseTime;
  float volume;
  float pitch;
  float timer;
  bool  enabled;
} AmbientLoop_t;

typedef struct {
  Sound sound;
  Sound alias[SOUND_ALIASES];
  int   nextAlias;
} SoundAsset_t;

typedef struct {
  SoundType_t type;
  Vector3     position;
  float       volume;
  float       pitch;
} SoundEvent_t;

typedef struct {
  SoundAsset_t assets[SOUND_COUNT];
  SoundEvent_t events[MAX_SOUND_EVENTS];
  int          eventCount;
  AmbientLoop_t ambient;
} SoundSystem_t;

SoundSystem_t InitSoundSystem(void);
void          ShutdownSoundSystem(SoundSystem_t *sys);
void          QueueSound(SoundSystem_t *sys, SoundType_t type, Vector3 pos,
                         float vol, float pitch);
void          UpdateSoundSystem(SoundSystem_t *sys, world_t *world,
                                GameWorld *game, float dt);
void          EnableDesertAmbience(SoundSystem_t *sys, bool enabled);
