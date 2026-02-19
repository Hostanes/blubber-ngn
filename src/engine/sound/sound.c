#include "sound.h"

SoundHandle Audio_Load(AudioSystem *audio, const char *path) {
  if (audio->assetCount >= audio->assetCapacity) {
    audio->assets =
        GrowArray(audio->assets, &audio->assetCapacity, sizeof(AudioAsset));
  }

  uint32_t id = audio->assetCount++;

  AudioAsset *asset = &audio->assets[id];
  asset->sound = LoadSound(path);

  asset->aliasCount = 16; // configurable
  asset->aliases = malloc(sizeof(Sound) * asset->aliasCount);

  for (uint32_t i = 0; i < asset->aliasCount; i++)
    asset->aliases[i] = LoadSoundAlias(asset->sound);

  asset->nextAlias = 0;

  return id;
}

void Audio_Play3D(AudioSystem *audio, SoundHandle handle, Vector3 pos,
                  float volume, float pitch) {
  if (audio->eventCount >= audio->eventCapacity) {
    audio->events =
        GrowArray(audio->events, &audio->eventCapacity, sizeof(AudioEvent));
  }

  audio->events[audio->eventCount++] = (AudioEvent){handle, pos, volume, pitch};
}

SoundInstanceID Audio_PlayLoop3D(AudioSystem *audio, SoundHandle handle,
                                 Vector3 pos, float volume, float pitch) {
  if (audio->instanceCount >= audio->instanceCapacity) {
    audio->instances = GrowArray(audio->instances, &audio->instanceCapacity,
                                 sizeof(AudioInstance));
  }

  uint32_t id = audio->instanceCount++;

  audio->instances[id] = (AudioInstance){.active = true,
                                         .handle = handle,
                                         .position = pos,
                                         .volume = volume,
                                         .pitch = pitch,
                                         .looping = true};

  return id;
}

void Audio_UpdateLoopPosition(AudioSystem *audio, SoundInstanceID id,
                              Vector3 pos) {
  if (id >= audio->instanceCount)
    return;

  audio->instances[id].position = pos;
}

void Audio_Stop(AudioSystem *audio, SoundInstanceID id) {
  if (id >= audio->instanceCount)
    return;

  audio->instances[id].active = false;
}
