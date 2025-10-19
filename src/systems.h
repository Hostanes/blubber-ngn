
// systems.h
// Core gameplay systems: input, physics, rendering

#pragma once
#include "game.h"
#include "sound.h"

void PlayerControlSystem(GameState_t *gs, SoundSystem_t *soundSys, float dt,
                         Camera3D *camera);
void PhysicsSystem(GameState_t *gs, float dt);
void RenderSystem(GameState_t *gs, Camera3D camera);
void LoadAssets();

void UnloadAssets();
