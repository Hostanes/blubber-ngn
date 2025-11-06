// game_init.c
// Refactored initialization, multi-ray system, and entity factories.
// Requires game.h in the same include path.

#include "game.h"
#include "raylib.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#ifndef PI
#define PI 3.14159265358979323846f
#endif

// -----------------------------------------------
// Helper: initialize an empty ModelCollection
// -----------------------------------------------
static ModelCollection_t InitModelCollection(int countModels) {
  ModelCollection_t mc;
  memset(&mc, 0, sizeof(mc));
  mc.countModels = countModels;

  mc.models = (Model *)malloc(sizeof(Model) * countModels);
  mc.offsets = (Vector3 *)malloc(sizeof(Vector3) * countModels);
  mc.orientations = (Orientation *)malloc(sizeof(Orientation) * countModels);
  mc.parentIds = (int *)malloc(sizeof(int) * countModels);
  mc.rotLocks = (bool **)malloc(sizeof(bool *) * countModels);
  mc.rotInverts = (bool **)malloc(sizeof(bool *) * countModels);

  mc.localRotationOffset =
      (Orientation *)malloc(sizeof(Orientation) * countModels);

  mc.globalOrientations =
      (Orientation *)malloc(sizeof(Orientation) * countModels);
  mc.globalPositions = (Vector3 *)malloc(sizeof(Vector3) * countModels);

  // zero-init models (important for raylib Model default state)
  for (int i = 0; i < countModels; i++) {
    // Zero out Model struct so raylib won't have garbage
    memset(&mc.models[i], 0, sizeof(Model));
    mc.offsets[i] = (Vector3){0, 0, 0};
    mc.orientations[i] = (Orientation){0, 0, 0};
    mc.parentIds[i] = -1;
    mc.localRotationOffset[i] = (Orientation){0, 0, 0};
    mc.globalPositions[i] = (Vector3){0, 0, 0};
    mc.globalOrientations[i] = (Orientation){0, 0, 0};

    mc.rotLocks[i] = (bool *)malloc(sizeof(bool) * 3);
    mc.rotInverts[i] = (bool *)malloc(sizeof(bool) * 3);
    for (int j = 0; j < 3; j++) {
      mc.rotLocks[i][j] = true;
      mc.rotInverts[i][j] = false;
    }
  }

  return mc;
}

static void FreeModelCollection(ModelCollection_t *mc) {
  if (!mc)
    return;
  if (mc->models) {
    // If models contain loaded meshes you want to UnloadModel here, do it
    // externally if needed.
    free(mc->models);
  }
  if (mc->offsets)
    free(mc->offsets);
  if (mc->orientations)
    free(mc->orientations);
  if (mc->parentIds)
    free(mc->parentIds);
  if (mc->rotLocks) {
    for (int i = 0; i < mc->countModels; i++) {
      if (mc->rotLocks[i])
        free(mc->rotLocks[i]);
    }
    free(mc->rotLocks);
  }
  if (mc->rotInverts) {
    for (int i = 0; i < mc->countModels; i++) {
      if (mc->rotInverts[i])
        free(mc->rotInverts[i]);
    }
    free(mc->rotInverts);
  }
  if (mc->localRotationOffset)
    free(mc->localRotationOffset);
  if (mc->globalPositions)
    free(mc->globalPositions);
  if (mc->globalOrientations)
    free(mc->globalOrientations);

  // Finally clear struct to avoid accidental reuse
  memset(mc, 0, sizeof(ModelCollection_t));
}

// Convert orientation (yaw/pitch/roll) to forward vector
Vector3 ConvertOrientationToVector3(Orientation o) {
  Vector3 dir;
  // This uses yaw around Y, pitch around X. Matches earlier code patterns.
  dir.x = cosf(o.pitch) * sinf(o.yaw);
  dir.y = sinf(o.pitch);
  dir.z = cosf(o.pitch) * cosf(o.yaw);
  return dir;
}

// -----------------------------------------------
// Ray helpers (orientation-based)
// -----------------------------------------------
void AddRayToEntity(GameState_t *gs, entity_t e, int parentModelIndex,
                    Vector3 localOffset, Orientation oriOffset,
                    float distance) {
  if (e < 0 || e >= gs->em.count)
    return;

  int idx = gs->components.rayCounts[e];
  if (idx >= MAX_RAYS_PER_ENTITY)
    return; // too many rays

  Raycast_t *rc = &gs->components.raycasts[e][idx];
  rc->active = true;
  rc->parentModelIndex = parentModelIndex;
  rc->localOffset = localOffset;
  rc->oriOffset = oriOffset;
  rc->distance = distance;

  // Initialize world-space ray
  rc->ray.position = Vector3Zero();
  rc->ray.direction = Vector3Zero();

  gs->components.rayCounts[e] = idx + 1;
}

// -----------------------------------------------
// Terrain initialization
// -----------------------------------------------
void InitTerrain(GameState_t *gs, Texture2D sandTex) {
  Terrain_t *terrain = &gs->terrain;

  terrain->model = LoadModel("assets/models/terrain.glb");
  terrain->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = sandTex;
  terrain->mesh = terrain->model.meshes[0]; // assuming single mesh
}

// -----------------------------------------------
// Entity factory helpers
// -----------------------------------------------
static entity_t CreatePlayer(GameState_t *gs, Vector3 pos) {
  entity_t e = gs->em.count++;
  gs->em.alive[e] = 1;
  gs->em.masks[e] = C_POSITION | C_VELOCITY | C_MODEL | C_COLLISION | C_HITBOX |
                    C_RAYCAST | C_PLAYER_TAG | C_COOLDOWN_TAG | C_GRAVITY;

  gs->components.positions[e] = pos;
  gs->components.velocities[e] = (Vector3){0, 0, 0};
  gs->components.stepCycle[e] = 0;
  gs->components.prevStepCycle[e] = 0;
  gs->components.stepRate[e] = 2.0f;
  gs->components.types[e] = ENTITY_PLAYER;

  // Model collection: 3 parts (legs, torso/head, gun)
  ModelCollection_t *mc = &gs->components.modelCollections[e];
  *mc = InitModelCollection(5);

  mc->models[0] = LoadModel("assets/models/raptor1-legs.glb");
  Texture2D mechTex = LoadTexture("assets/textures/legs.png");
  mc->models[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = mechTex;
  mc->offsets[0] = (Vector3){0, 0, 0};
  mc->orientations[0] = (Orientation){0, 0, 0};

  // torso/head as a simple cube model for visualization
  Mesh torsoMesh = GenMeshCube(10.0f, 2.0f, 10.0f);
  mc->models[1] = LoadModelFromMesh(torsoMesh);
  mc->models[1].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = BLUE;
  mc->offsets[1] = (Vector3){0, 10.2f, 0};
  mc->parentIds[1] = -1;
  mc->localRotationOffset[1].yaw = 0;
  mc->rotLocks[1][0] = true;
  mc->rotLocks[1][1] = true;
  mc->rotLocks[1][2] = false;

  // gun as primitive model
  Mesh gunMesh = GenMeshCube(2.0f, 2.0f, 10.0f);
  mc->models[2] = LoadModelFromMesh(gunMesh);
  mc->models[2].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = PURPLE;
  mc->offsets[2] = (Vector3){8.0f, -2, 6};
  mc->parentIds[2] = 1;

  mc->rotLocks[2][0] = true;
  mc->rotLocks[2][1] = true;
  mc->rotLocks[2][2] = false;

  Mesh cockpitRoof = GenMeshCube(10.0f, 2.0f, 10.0f);
  mc->models[3] = LoadModelFromMesh(cockpitRoof);
  mc->models[3].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = BLACK;
  mc->offsets[3] = (Vector3){0, 4, 0};
  mc->parentIds[3] = 1;

  mc->rotLocks[3][0] = true;
  mc->rotLocks[3][1] = true;
  mc->rotLocks[3][2] = false;

  Mesh cockpitFloor = GenMeshCube(10.0f, 2.0f, 10.0f);
  mc->models[4] = LoadModelFromMesh(cockpitFloor);
  mc->models[4].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = BLACK;
  mc->offsets[4] = (Vector3){0, -3.5, 0};
  mc->parentIds[4] = 1;

  mc->rotLocks[4][0] = true;
  mc->rotLocks[4][1] = true;
  mc->rotLocks[4][2] = false;

  // initialize rayCount and add a muzzle ray for the gun (model index 2)
  gs->components.rayCounts[e] = 0;
  // Main aim ray - parent to torso (model index 1)
  AddRayToEntity(gs, e, 1,
                 (Vector3){0, 0, 0},     // offset near head/center of torso
                 (Orientation){0, 0, 0}, // forward
                 500.0f);                // long aim distance

  // Gun muzzle ray - still parent to gun (model index 2)
  AddRayToEntity(gs, e, 2, (Vector3){0, 0, 0}, (Orientation){0, 0, 0}, 500.0f);

  // 2 guns
  gs->components.muzzleVelocities[0] = MemAlloc(sizeof(float) * 2);
  gs->components.muzzleVelocities[0][0] = 1000.0f;
  gs->components.dropRates[0] = MemAlloc(sizeof(float) * 2);
  gs->components.dropRates[0][0] = 70.0f;

  // cooldown & firerate allocations
  gs->components.cooldowns[e] = (float *)malloc(sizeof(float) * 1);
  gs->components.cooldowns[e][0] = 0.8;
  gs->components.firerate[e] = (float *)malloc(sizeof(float) * 1);
  gs->components.firerate[e][0] = 0.2f;

  // Collision
  ModelCollection_t *col = &gs->components.collisionCollections[e];
  *col = InitModelCollection(1);
  Mesh moveBox = GenMeshCube(4, 8, 4);
  col->models[0] = LoadModelFromMesh(moveBox);
  col->offsets[0] = (Vector3){0, 5, 0};

  // Hitbox
  ModelCollection_t *hit = &gs->components.hitboxCollections[e];
  *hit = InitModelCollection(1);
  Mesh hitbox1 = GenMeshCube(4, 10, 4);
  hit->models[0] = LoadModelFromMesh(hitbox1);
  hit->offsets[0] = (Vector3){0, 2, 0};
  return e;
}

static int CreateStatic(GameState_t *gs, Vector3 pos, Vector3 size, Color c) {
  // find open slot
  int i = 0;
  while (i < MAX_ENTITIES && gs->statics.modelCollections[i].countModels != 0)
    i++;

  if (i >= MAX_ENTITIES)
    return -1; // no room

  gs->statics.positions[i] = pos;

  ModelCollection_t *mc = &gs->statics.modelCollections[i];
  *mc = InitModelCollection(1);

  Mesh cube = GenMeshCube(size.x, size.y, size.z);
  mc->models[0] = LoadModelFromMesh(cube);
  mc->models[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = c;

  mc->offsets[0] = Vector3Zero();
  mc->parentIds[0] = -1;

  // collision too
  ModelCollection_t *col = &gs->statics.collisionCollections[i];
  *col = InitModelCollection(1);
  col->models[0] = LoadModelFromMesh(GenMeshCube(size.x, size.y, size.z));
  col->offsets[0] = Vector3Zero();
  col->parentIds[0] = -1;

  // hitbox too
  ModelCollection_t *hb = &gs->statics.hitboxCollections[i];
  *hb = InitModelCollection(1);
  hb->models[0] = LoadModelFromMesh(GenMeshCube(size.x, size.y, size.z));
  hb->offsets[0] = Vector3Zero();
  hb->parentIds[0] = -1;

  return i;
}

static entity_t CreateTurret(GameState_t *gs, Vector3 pos) {
  entity_t e = gs->em.count++;
  gs->em.alive[e] = 1;
  gs->em.masks[e] = C_POSITION | C_MODEL | C_HITBOX | C_HITPOINT_TAG |
                    C_TURRET_BEHAVIOUR_1 | C_COOLDOWN_TAG | C_RAYCAST |
                    C_GRAVITY;
  gs->components.positions[e] = pos;
  gs->components.types[e] = ENTITY_TURRET;
  gs->components.hitPoints[e] = 100.0f;

  // visual models (base + barrel)
  ModelCollection_t *mc = &gs->components.modelCollections[e];
  *mc = InitModelCollection(2);

  mc->models[0] = LoadModelFromMesh(GenMeshCylinder(2.0f, 5.0f, 5));
  mc->offsets[0] = (Vector3){0, 0, 0};
  mc->parentIds[0] = -1;

  mc->models[1] = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 6.0f));
  mc->offsets[1] = (Vector3){0, 5.0f, 3.0f};
  mc->parentIds[1] = 0;

  // hitbox
  ModelCollection_t *hb = &gs->components.hitboxCollections[e];
  *hb = InitModelCollection(1);
  hb->models[0] = LoadModelFromMesh(GenMeshCube(20, 20, 20));
  hb->offsets[0] = Vector3Zero();
  hb->parentIds[0] = -1;

  // ray: attach to barrel (model 1) muzzle
  gs->components.rayCounts[e] = 0;
  AddRayToEntity(gs, e, 1, (Vector3){0, 0, 0}, (Orientation){0, 0, 0}, 500.0f);

  // cooldown & firerate
  gs->components.cooldowns[e] = (float *)malloc(sizeof(float) * 1);
  gs->components.cooldowns[e][0] = 0.0f;
  gs->components.firerate[e] = (float *)malloc(sizeof(float) * 1);
  gs->components.firerate[e][0] = 0.4f;

  return e;
}

// -----------------------------------------------
// InitGame: orchestrates initialization
// -----------------------------------------------
GameState_t InitGame(void) {
  GameState_t gs;
  memset(&gs, 0, sizeof(gs));

  gs.em.count = 0;
  memset(gs.em.alive, 0, sizeof(gs.em.alive));
  memset(gs.em.masks, 0, sizeof(gs.em.masks));

  gs.state = STATE_INLEVEL;
  gs.pHeadbobTimer = 0.0f;

  // terrain
  Texture2D sandTex = LoadTexture("assets/textures/xtSand.png");
  InitTerrain(&gs, sandTex);

  // create player at origin-ish
  gs.playerId = CreatePlayer(&gs, (Vector3){0, 10.0f, 0});

  // create a bunch of simple houses/walls
  int numStatics = 100;
  for (int i = 0; i < numStatics; i++) {
    float width = GetRandomValue(10, 40);
    float height = GetRandomValue(15, 55);
    float depth = GetRandomValue(10, 40);

    float x =
        GetRandomValue(-TERRAIN_SIZE / 2, TERRAIN_SIZE / 2) * TERRAIN_SCALE;
    float z =
        GetRandomValue(-TERRAIN_SIZE / 2, TERRAIN_SIZE / 2) * TERRAIN_SCALE;
    float y = height / 2.0f;

    Color c = (Color){(unsigned char)GetRandomValue(100, 255),
                      (unsigned char)GetRandomValue(100, 255),
                      (unsigned char)GetRandomValue(100, 255), 255};

    CreateStatic(&gs, (Vector3){x, y, z}, (Vector3){width, height, depth}, c);
  }
  // create a sample turret
  CreateTurret(&gs, (Vector3){500.0f, 8.0f, 30.0f});

  // ensure rayCounts initialized for any entities that weren't touched
  for (int i = 0; i < gs.em.count; i++) {
    if (gs.components.rayCounts[i] == 0)
      gs.components.rayCounts[i] = 0;
  }

  // Initialize projectile pool
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    gs.projectiles.active[i] = false;
    gs.projectiles.positions[i] = Vector3Zero();
    gs.projectiles.velocities[i] = Vector3Zero();
    gs.projectiles.lifetimes[i] = 0.0f;
    gs.projectiles.radii[i] = 1.0f; // default bullet size
    gs.projectiles.owners[i] = -1;
    gs.projectiles.types[i] = -1;
  }

  return gs;
}
