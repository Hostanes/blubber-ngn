#ifndef SYSTEMS_H
#define SYSTEMS_H

#include "../engine.h"
#include "../game.h"
#include "../sound.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------- Player ----------
void PlayerControlSystem(GameState_t *gs, Engine_t *eng,
                         SoundSystem_t *soundSys, float dt, Camera3D *camera);

// ---------- Raycast ----------
void UpdateRayCast(Raycast_t *raycast, Vector3 position,
                   Orientation orientation);
void UpdateRayCastToModel(GameState_t *gs, Engine_t *eng, Raycast_t *raycast,
                          int entityId, int modelId);
void UpdateEntityRaycasts(Engine_t *eng, entity_t e);
bool CheckRaycastCollision(GameState_t *gs, Engine_t *eng, Raycast_t *raycast,
                           entity_t self);

// ---------- Weapon ----------
void FireProjectile(Engine_t *eng, entity_t shooter, int rayIndex);
void DecrementCooldowns(Engine_t *eng, float dt);

// ---------- Projectiles ----------
void UpdateProjectiles(GameState_t *gs, Engine_t *eng, SoundSystem_t *soundSys,
                       float dt);

// ---------- Particles ----------
void UpdateParticles(Engine_t *eng, float dt);
void spawnParticle(Engine_t *eng, Vector3 pos, float lifetime, int type);
static inline void SpawnParticleTyped(Engine_t *eng, Vector3 pos, float life,
                                      int type);

// --- Dust puff (type 0) ---
void SpawnDust(Engine_t *eng, Vector3 pos);

// --- Metal dust / debris (type 1) ---
void SpawnMetalDust(Engine_t *eng, Vector3 pos);

// --- Sparks (type 2) ---
void SpawnSpark(Engine_t *eng, Vector3 pos);

// --- Sand / dirt burst (type 3) ---
void SpawnSandBurst(Engine_t *eng, Vector3 pos);

// --- Smoke plume (type 4) ---
void SpawnSmoke(Engine_t *eng, Vector3 pos);

// ---------- Physics ----------
void PhysicsSystem(GameState_t *gs, Engine_t *eng, SoundSystem_t *soundSys,
                   float dt);
float GetTerrainHeightAtXZ(Terrain_t *terrain, float wx, float wz);
bool ProjectileIntersectsEntityOBB(Engine_t *eng, int projIndex, entity_t eid);
bool CheckAndResolveOBBCollision(Vector3 *aPos, ModelCollection_t *aCC,
                                 Vector3 *bPos, ModelCollection_t *bCC);
bool SegmentIntersectsOBB(Vector3 p0, Vector3 p1, ModelCollection_t *coll,
                          int modelIndex);
void ApplyTorsoRecoil(ModelCollection_t *mc, int torsoIndex, float intensity,
                      Vector3 direction);
void UpdateTorsoRecoil(ModelCollection_t *mc, int torsoIndex, float dt);
bool CheckOBBOverlap(Vector3 aPos, ModelCollection_t *aCC, Vector3 bPos,
                     ModelCollection_t *bCC);

// ---------- AI ----------
void TurretAISystem(GameState_t *gs, Engine_t *eng, SoundSystem_t *soundSys,
                    float dt);
void MechAISystem(GameState_t *gs, Engine_t *eng, SoundSystem_t *soundSys,
                  float dt);
void UpdateEnemyVelocities(GameState_t *gs, Engine_t *eng,
                           SoundSystem_t *soundSys, float dt);
void UpdateEnemyTargets(GameState_t *gs, Engine_t *eng, SoundSystem_t *soundSys,
                        float dt);

// ---------- Rendering ----------
void RenderSystem(GameState_t *gs, Engine_t *eng, Camera3D camera);

// ---------- Main Menu ----------
void MainMenuSystem(GameState_t *gs, Engine_t *eng);

// ---------- Game Update ----------
void UpdateGame(GameState_t *gs, Engine_t *eng, SoundSystem_t *soundSys,
                Camera3D *camera, float dt);

// ---------- Banner ----------
void TriggerMessage(GameState_t *gs, const char *msg);
void UpdateMessageBanner(GameState_t *gs, float dt);
void DrawMessageBanner(GameState_t *gs);

// death system
void KillEntity(GameState_t *gs, Engine_t *eng, SoundSystem_t *soundSys,
                entity_t id);

// other

void LoadAssets();

#endif
