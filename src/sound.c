
// sound.c
// Implements sound system initialization, queuing, and processing

#include "sound.h"
#include "raymath.h"

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
  sys.assets[SOUND_FOOTSTEP].sound = LoadSound("assets/audio/mech_step_1.mp3");
  sys.assets[SOUND_WEAPON_FIRE].sound =
      LoadSound("assets/audio/cannon_shot_1.mp3");
  // sys.assets[SOUND_EXPLOSION].sound = LoadSound("assets/sfx/explosion.wav");
  // sys.assets[SOUND_UI_CLICK].sound = LoadSound("assets/sfx/ui_click.wav");

  return sys;
}

void QueueSound(SoundSystem_t *sys, SoundType_t type, Vector3 pos, float vol,
                float pitch) {
  if (sys->eventCount < MAX_SOUND_EVENTS) {
    sys->events[sys->eventCount++] = (SoundEvent_t){type, pos, vol, pitch};
  }
}

void ProcessSoundSystem(SoundSystem_t *sys, Vector3 listenerPos) {
  for (int i = 0; i < sys->eventCount; i++) {
    SoundEvent_t event = sys->events[i];
    Sound sound = sys->assets[event.type].sound;

    // distance-based volume fade
    float dist = Vector3Distance(listenerPos, event.position);
    float atten = 1.0f / (1.0f + 0.1f * dist); // simple falloff

    SetSoundVolume(sound, event.volume * atten * 0.5f);
    SetSoundPitch(sound, event.pitch);
    PlaySoundMultiCompat(sound);
  }

  sys->eventCount = 0; // clear for next frame
}
