
#include "raylib.h"
#include "raymath.h"
#include <stdint.h>
#include <stdlib.h>

typedef struct AudioSystem AudioSystem;
typedef uint32_t SoundHandle;
typedef uint32_t SoundInstanceID;

typedef struct {
  Sound sound;
  Sound *aliases;
  uint32_t aliasCount;
  uint32_t nextAlias;
} AudioAsset;

typedef struct {
  SoundHandle handle;
  Vector3 position;
  float volume;
  float pitch;
} AudioEvent;

typedef struct {
  bool active;
  SoundHandle handle;
  Vector3 position;
  float volume;
  float pitch;
  bool looping;
} AudioInstance;

struct AudioSystem {
  AudioAsset *assets;
  uint32_t assetCount;
  uint32_t assetCapacity;

  AudioEvent *events;
  uint32_t eventCount;
  uint32_t eventCapacity;

  AudioInstance *instances;
  uint32_t instanceCount;
  uint32_t instanceCapacity;
};

static void *GrowArray(void *ptr, uint32_t *capacity, uint32_t elemSize) {
  uint32_t newCap = (*capacity == 0) ? 8 : (*capacity * 2);
  void *newMem = realloc(ptr, newCap * elemSize);

  if (!newMem)
    return ptr;

  *capacity = newCap;
  return newMem;
}

// Init / Shutdown
AudioSystem *Audio_Create(void);
void Audio_Destroy(AudioSystem *audio);

// Asset management
SoundHandle Audio_Load(AudioSystem *audio, const char *path);

// One-shot
void Audio_Play3D(AudioSystem *audio, SoundHandle handle, Vector3 position,
                  float volume, float pitch);

// Persistent looping
SoundInstanceID Audio_PlayLoop3D(AudioSystem *audio, SoundHandle handle,
                                 Vector3 position, float volume, float pitch);

void Audio_UpdateLoopPosition(AudioSystem *audio, SoundInstanceID id,
                              Vector3 newPosition);

void Audio_Stop(AudioSystem *audio, SoundInstanceID id);

// Frame update
void Audio_Update(AudioSystem *audio, Vector3 listenerPos, float listenerYaw,
                  float dt);
