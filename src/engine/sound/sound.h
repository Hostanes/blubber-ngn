#pragma once
#include "raylib.h"
#include <stdbool.h>
#include "../../engine/ecs/ecs_types.h"

// Forward declarations — full types in game.h / world.h
struct world_t;
typedef struct world_t world_t;
typedef struct GameWorld GameWorld;

#define MAX_SOUND_EVENTS      256
#define MAX_SIMULTANEOUS_SOUNDS 32
#define SOUND_ALIASES         32
#define MAX_LOOP_SOUNDS       8

typedef enum {
  SOUND_FOOTSTEP,
  SOUND_WEAPON_FIRE,
  SOUND_ROCKET_FIRE,
  SOUND_EXPLOSION,
  SOUND_HITMARKER,
  SOUND_CLANG,
  SOUND_AMBIENT_DESERT,
  SOUND_CHAIN_PULL,
  SOUND_COUNT
} SoundType_t;

typedef enum {
  LOOP_SOUND_ROCKET = 0,
  LOOP_SOUND_COUNT
} LoopSoundType_t;

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

// A looping sound attached to a specific entity (e.g. in-flight rocket).
// Stopped abruptly with StopLoopSound when the entity dies.
typedef struct {
  Music    stream;
  entity_t owner;
  Vector3  position;
  float    baseVol;
  float    pitch;
  bool     active;
} LoopSoundSlot_t;

typedef struct {
  SoundAsset_t    assets[SOUND_COUNT];
  SoundEvent_t    events[MAX_SOUND_EVENTS];
  int             eventCount;
  AmbientLoop_t   ambient;
  LoopSoundSlot_t loopSlots[MAX_LOOP_SOUNDS];
} SoundSystem_t;

SoundSystem_t InitSoundSystem(void);
void          ShutdownSoundSystem(SoundSystem_t *sys);
void          QueueSound(SoundSystem_t *sys, SoundType_t type, Vector3 pos,
                         float vol, float pitch);
// Attach-or-update a looping sound for an entity. Call every frame while alive.
void          TickLoopSound(SoundSystem_t *sys, LoopSoundType_t type,
                            entity_t owner, Vector3 pos, float vol, float pitch);
// Immediately cut the looping sound for an entity (call on death/explosion).
void          StopLoopSound(SoundSystem_t *sys, entity_t owner);
void          UpdateSoundSystem(SoundSystem_t *sys, world_t *world,
                                GameWorld *game, float dt);
void          EnableDesertAmbience(SoundSystem_t *sys, bool enabled);
