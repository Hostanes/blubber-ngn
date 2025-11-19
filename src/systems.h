
// systems.h
// Core gameplay systems: input, physics, rendering

#pragma once
#include "engine.h"
#include "game.h"
#include "sound.h"

void PlayerControlSystem(GameState_t *gs, Engine_t *eng,
                         SoundSystem_t *soundSys, float dt, Camera3D *camera);

void PhysicsSystem(GameState_t *gs, Engine_t *eng, float dt);
void RenderSystem(GameState_t *gs, Engine_t *eng, Camera3D camera);

void UpdateGame(GameState_t *gs, Engine_t *eng, SoundSystem_t *soundSys,
                Camera3D *camera, float dt);

void LoadAssets();

void UnloadAssets();
