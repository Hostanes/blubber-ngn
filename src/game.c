// game_init.c
// Refactored initialization, multi-ray system, and entity factories.
// Requires game.h in the same include path.
#include "game.h"
#include "engine.h"
#include "engine_components.h"
#include "raylib.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define MAX_COMPONENTS 32

#ifndef PI
#define PI 3.14159265358979323846f
#endif

#define TERRAIN_SIZE 200
#define TERRAIN_SCALE 10.0f

void TriggerMessage(GameState_t *gs, const char *msg);

// Called when the player is inside the cube
static void Cube_OnCollision(Engine_t *eng, GameState_t *gs, entity_t self,
                             entity_t other, char *text) {
  int idx = GetEntityIndex(self);
  ModelCollection_t *mc = &eng->actors.modelCollections[idx];

  // Set color to BLUE
  // mc->models[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = BLUE;

  TriggerMessage(gs, text);
}

// Called when the player leaves the cube
static void Cube_OnCollisionExit(Engine_t *eng, GameState_t *gs, entity_t self,
                                 entity_t other) {
  int idx = GetEntityIndex(self);
  ModelCollection_t *mc = &eng->actors.modelCollections[idx];

  // Set color to RED
  // mc->models[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = RED;
}

// -----------------------------------------------
// Helper: initialize an empty ModelCollection
// -----------------------------------------------
static ModelCollection_t InitModelCollection(int countModels) {
  ModelCollection_t mc;
  memset(&mc, 0, sizeof(mc));
  mc.countModels = countModels;

  mc.models = (Model *)malloc(sizeof(Model) * countModels);
  mc.isActive = (bool *)malloc(sizeof(bool) * countModels);
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
    mc.isActive[i] = true;
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
void AddRayToEntity(Engine_t *eng, entity_t e, int parentModelIndex,
                    Vector3 localOffset, Orientation oriOffset,
                    float distance) {
  if (e < 0 || e >= eng->em.count)
    return;

  int idx = eng->actors.rayCounts[e];
  if (idx >= MAX_RAYS_PER_ENTITY)
    return; // too many rays

  Raycast_t *rc = &eng->actors.raycasts[e][idx];
  rc->active = true;
  rc->parentModelIndex = parentModelIndex;
  rc->localOffset = localOffset;
  rc->oriOffset = oriOffset;
  rc->distance = distance;

  // Initialize world-space ray
  rc->ray.position = Vector3Zero();
  rc->ray.direction = Vector3Zero();

  eng->actors.rayCounts[e] = idx + 1;
}

// -----------------------------------------------
// Terrain initialization
// -----------------------------------------------
void InitTerrain(GameState_t *gs, Engine_t *eng, Texture2D sandTex,
                 char *terrainModelPath) {
  Terrain_t *terrain = &gs->terrain;

  terrain->model = LoadModel(terrainModelPath);
  terrain->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = sandTex;
  terrain->mesh = terrain->model.meshes[0];

  BoundingBox bb = GetMeshBoundingBox(terrain->mesh);

  // min are the origin of the bb, in this case origin is a corner, not the
  // volumetric center
  terrain->minX = bb.min.x;
  terrain->minZ = bb.min.z;

  terrain->worldWidth = bb.max.x - bb.min.x;
  terrain->worldLength = bb.max.z - bb.min.z;

  terrain->hmWidth = HEIGHTMAP_RES_X;
  terrain->hmHeight = HEIGHTMAP_RES_Z;

  terrain->cellSizeX = terrain->worldWidth / (float)(terrain->hmWidth - 1);
  terrain->cellSizeZ = terrain->worldLength / (float)(terrain->hmHeight - 1);

  terrain->height =
      malloc(sizeof(float) * terrain->hmWidth * terrain->hmHeight);
  memset(terrain->height, 0,
         sizeof(float) * terrain->hmWidth * terrain->hmHeight);
}

static void BuildHeightmap(Terrain_t *terrain) {
  Mesh *m = &terrain->mesh;

  const float minX = terrain->minX;
  const float minZ = terrain->minZ;

  const int w = terrain->hmWidth;
  const int h = terrain->hmHeight;

  const float dx = terrain->cellSizeX;
  const float dz = terrain->cellSizeZ;

  // Clear heightmap to a very low number
  for (int i = 0; i < w * h; i++)
    terrain->height[i] = -99999.0f;

  Vector3 *verts = (Vector3 *)m->vertices;
  unsigned short *indices = (unsigned short *)m->indices;

  // Loop triangles
  for (int t = 0; t < m->triangleCount; t++) {
    int i0 = indices[t * 3 + 0];
    int i1 = indices[t * 3 + 1];
    int i2 = indices[t * 3 + 2];

    Vector3 v0 = verts[i0];
    Vector3 v1 = verts[i1];
    Vector3 v2 = verts[i2];

    // Compute triangle AABB in grid space
    float minTx = fminf(v0.x, fminf(v1.x, v2.x));
    float maxTx = fmaxf(v0.x, fmaxf(v1.x, v2.x));
    float minTz = fminf(v0.z, fminf(v1.z, v2.z));
    float maxTz = fmaxf(v0.z, fmaxf(v1.z, v2.z));

    int ix0 = (int)((minTx - minX) / dx);
    int ix1 = (int)((maxTx - minX) / dx);
    int iz0 = (int)((minTz - minZ) / dz);
    int iz1 = (int)((maxTz - minZ) / dz);

    // Clamp range to heightmap
    if (ix0 < 0)
      ix0 = 0;
    if (iz0 < 0)
      iz0 = 0;
    if (ix1 >= w)
      ix1 = w - 1;
    if (iz1 >= h)
      iz1 = h - 1;

// Barycentric helper fn
#define BARY(u, v, w, a, b, c) (u * a + v * b + w * c)

    // Iterate over heightmap cells overlapped by the triangle
    for (int iz = iz0; iz <= iz1; iz++) {
      for (int ix = ix0; ix <= ix1; ix++) {
        float wx = minX + ix * dx;
        float wz = minZ + iz * dz;

        // Barycentric coordinates
        Vector2 p = {wx, wz};
        Vector2 a = {v0.x, v0.z};
        Vector2 b = {v1.x, v1.z};
        Vector2 c = {v2.x, v2.z};

        float denom = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
        if (fabsf(denom) < 1e-6f)
          continue; // Degenerate triangle

        float u =
            ((b.y - c.y) * (p.x - c.x) + (c.x - b.x) * (p.y - c.y)) / denom;
        float v =
            ((c.y - a.y) * (p.x - c.x) + (a.x - c.x) * (p.y - c.y)) / denom;
        float wB = 1.0f - u - v;

        // Outside triangle?
        if (u < 0 || v < 0 || wB < 0)
          continue;

        // Interpolate height
        float hy = BARY(u, v, wB, v0.y, v1.y, v2.y);

        float *cell = &terrain->height[iz * w + ix];
        if (hy > *cell)
          *cell = hy;
      }
    }
  }
}

// -----------------------------------------------
// Entity factory helpers
// -----------------------------------------------
static entity_t CreatePlayer(Engine_t *eng, ActorComponentRegistry_t compReg,
                             Vector3 pos) {
  entity_t e = eng->em.count++;
  eng->em.alive[e] = 1;
  eng->em.masks[e] = C_POSITION | C_VELOCITY | C_MODEL | C_COLLISION |
                     C_HITBOX | C_RAYCAST | C_PLAYER_TAG | C_COOLDOWN_TAG |
                     C_GRAVITY | C_HITPOINT_TAG;

  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_Positions, &pos);
  Vector3 vel = {0, 0, 0};
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_velocities,
                        &vel);
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_prevPositions,
                        &pos);
  int moveBehaviour = PSTATE_NORMAL;
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_moveBehaviour,
                        &moveBehaviour);
  float timer = 0;
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_moveTimer,
                        &timer);
  eng->actors.stepCycle[e] = 0;
  eng->actors.prevStepCycle[e] = 0;
  eng->actors.stepRate[e] = 2.0f;
  eng->actors.types[e] = ENTITY_PLAYER;
  eng->actors.hitPoints[e] = 200.0f;

  // Model collection: 3 parts (legs, torso/head, gun)
  ModelCollection_t *mc = &eng->actors.modelCollections[e];
  *mc = InitModelCollection(6);

  mc->models[0] = LoadModel("assets/models/raptor1-legs.glb");
  Texture2D mechTex = LoadTexture("assets/textures/legs.png");
  mc->models[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = mechTex;
  mc->offsets[0] = (Vector3){0, 0, 0};
  mc->orientations[0] = (Orientation){-PI / 2, 0, 0};

  // torso/head as a simple cube model for visualization
  Mesh torsoMesh = GenMeshCube(10.0f, 2.0f, 10.0f);
  mc->models[1] = LoadModelFromMesh(torsoMesh);
  mc->models[1].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = BLUE;
  mc->offsets[1] = (Vector3){0, 10.2f, 0};
  mc->parentIds[1] = -1;
  mc->orientations[1].yaw = PI;
  mc->localRotationOffset[1].yaw = PI / 2;
  mc->rotLocks[1][0] = true;
  mc->rotLocks[1][1] = true;
  mc->rotLocks[1][2] = false;

  // gun as primitive model
  Mesh gunMesh = GenMeshCube(2.0f, 2.0f, 10.0f);
  // mc->models[2] = LoadModelFromMesh(gunMesh);
  mc->models[2] = LoadModel("assets/models/gun-autocannon.glb");

  mc->offsets[2] = (Vector3){8.0f, -4, 8};
  mc->orientations[2] = (Orientation){0, PI / 2, 0};
  mc->parentIds[2] = 1;

  mc->rotLocks[2][0] = true;
  mc->rotLocks[2][1] = true;
  mc->rotLocks[2][2] = false;

  // mc->models[2] = LoadModelFromMesh(gunMesh);
  mc->models[3] = LoadModel("assets/models/gun2.glb");

  mc->offsets[3] = (Vector3){-8.0f, -4, 8};
  mc->orientations[3] = (Orientation){0, PI / 2, 0};
  mc->parentIds[3] = 1;

  mc->rotLocks[3][0] = true;
  mc->rotLocks[3][1] = true;
  mc->rotLocks[3][2] = false;

  // mc->models[2] = LoadModelFromMesh(gunMesh);
  mc->models[4] = LoadModel("assets/models/gun3-rocketlauncher.glb");
  mc->offsets[4] = (Vector3){8.0f, 6, 8};
  mc->orientations[4] = (Orientation){0, PI / 2, 0};
  mc->parentIds[4] = 1;

  mc->rotLocks[4][0] = true;
  mc->rotLocks[4][1] = true;
  mc->rotLocks[4][2] = false;

  // mc->models[2] = LoadModelFromMesh(gunMesh);
  mc->models[5] = LoadModel("assets/models/gun4-blunderbus.glb");
  mc->offsets[5] = (Vector3){-8.0f, 6, 8};
  mc->orientations[5] = (Orientation){0, PI / 2, 0};
  mc->parentIds[5] = 1;

  mc->rotLocks[5][0] = true;
  mc->rotLocks[5][1] = true;
  mc->rotLocks[5][2] = false;

  int weaponCount = 4;
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_weaponCount,
                        &weaponCount);
  int weaponDamage[] = {10, 20, 20, 3};
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_weaponDamage,
                        &weaponDamage);

  // initialize rayCount and add a muzzle ray for the gun (model index 2)
  eng->actors.rayCounts[e] = 0;
  // Main aim ray - parent to torso (model index 1)
  AddRayToEntity(eng, e, 1,
                 (Vector3){0, 0, 0},     // offset near head/center of torso
                 (Orientation){0, 0, 0}, // forward
                 5000.0f);               // long aim distance

  // Gun muzzle ray - still parent to gun (model index 2)
  AddRayToEntity(eng, e, 2, (Vector3){0, 0, 0}, (Orientation){0, 0, 0},
                 5000.0f);

  // [ray 2] Right-hand cannon muzzle ray - parent to gun (model index 2)
  AddRayToEntity(eng, e, 3, (Vector3){0, 0, 0}, // mirrored X offset (tweak)
                 (Orientation){0, 0, 0}, 5000.0f);

  // [ray 3] Shoulder rocket muzzle ray - parent to torso (model index 1)
  AddRayToEntity(eng, e, 4, (Vector3){0.0f, 0.0f, 0.0f}, // up + forward (tweak)
                 (Orientation){0, 0, 0}, 5000.0f);

  // [ray 4] Blunder bus
  AddRayToEntity(eng, e, 5, (Vector3){0.0f, 0.0f, 0.0f}, // up + forward (tweak)
                 (Orientation){0, 0, 0}, 5000.0f);

  // Weapons (3)
  eng->actors.muzzleVelocities[e] = MemAlloc(sizeof(float) * weaponCount);
  eng->actors.dropRates[e] = MemAlloc(sizeof(float) * weaponCount);

  // Weapon 0: left-hand gun (fast bullet)
  eng->actors.muzzleVelocities[e][0] = 2500.0f;
  eng->actors.dropRates[e][0] = 20.0f;

  // Weapon 1: right-hand cannon (slow heavy shell, long reload)
  eng->actors.muzzleVelocities[e][1] = 700.0f; // slow
  eng->actors.dropRates[e][1] = 35.0f;         // heavier drop (tweak)

  // Weapon 2: shoulder rocket (medium speed, no drop)
  eng->actors.muzzleVelocities[e][2] = 1200.0f; // medium
  eng->actors.dropRates[e][2] = 0;              // no drop

  // Weapon 4: Blunder bus
  eng->actors.muzzleVelocities[e][3] = 3500.0f; // medium
  eng->actors.dropRates[e][3] = 15.0f;          // no drop

  eng->actors.cooldowns[e] = (float *)malloc(sizeof(float) * weaponCount);
  eng->actors.firerate[e] = (float *)malloc(sizeof(float) * weaponCount);

  // Cooldowns start at 0 (ready)
  for (int w = 0; w < weaponCount; w++)
    eng->actors.cooldowns[e][w] = 0.2f;

  // Weapon 0: left-hand gun (shots/sec)
  eng->actors.firerate[e][0] = 0.2f;

  // Weapon 1: right-hand cannon (long reload)
  eng->actors.firerate[e][1] = 2.5f;

  // Weapon 2: shoulder rocket
  eng->actors.firerate[e][2] = 1.5f;

  // Weapon 3: BLUNDERBUSS!!
  eng->actors.firerate[e][3] = 1.5f;

  // Collision
  ModelCollection_t *col = &eng->actors.collisionCollections[e];
  *col = InitModelCollection(1);
  Mesh moveBox = GenMeshCube(4, 15, 4);
  col->models[0] = LoadModelFromMesh(moveBox);
  col->offsets[0] = (Vector3){0, 5, 0};

  // Hitbox
  ModelCollection_t *hit = &eng->actors.hitboxCollections[e];
  *hit = InitModelCollection(1);
  Mesh hitbox1 = GenMeshCube(10, 15, 10);
  hit->models[0] = LoadModelFromMesh(hitbox1);
  hit->offsets[0] = (Vector3){0, 5, 0};

  entity_t id = MakeEntityID(ET_STATIC, e);
  return id;
}

static int CreateSkybox(Engine_t *eng, Vector3 pos) {

  int i = 0;
  while (i < MAX_ENTITIES && eng->statics.modelCollections[i].countModels != 0)
    i++;

  if (i >= MAX_ENTITIES)
    return -1; // no room

  eng->statics.positions[i] = pos;

  ModelCollection_t *mc = &eng->statics.modelCollections[i];
  *mc = InitModelCollection(1);

  mc->models[0] = LoadModel("assets/models/skybox.glb");

  mc->offsets[0] = Vector3Zero();
  mc->parentIds[0] = -1;

  entity_t id = MakeEntityID(ET_STATIC, i);
  return id;
}

static int CreateStatic(Engine_t *eng, Vector3 pos, Vector3 size, Color c) {
  int i = 0;
  while (i < MAX_STATICS && eng->statics.modelCollections[i].countModels != 0)
    i++;

  if (i >= MAX_STATICS)
    return -1; // no room

  eng->statics.positions[i] = pos;

  ModelCollection_t *mc = &eng->statics.modelCollections[i];
  *mc = InitModelCollection(1);
  mc->isActive[0] = true;

  Mesh cube = GenMeshCube(size.x, size.y, size.z);
  mc->models[0] = LoadModelFromMesh(cube);
  mc->models[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = c;

  mc->offsets[0] = Vector3Zero();
  mc->parentIds[0] = -1;

  // collision too
  ModelCollection_t *col = &eng->statics.collisionCollections[i];
  *col = InitModelCollection(1);
  col->isActive[0] = true;

  col->models[0] = LoadModelFromMesh(GenMeshCube(size.x, size.y, size.z));
  col->offsets[0] = Vector3Zero();
  col->parentIds[0] = -1;

  // hitbox too
  ModelCollection_t *hb = &eng->statics.hitboxCollections[i];
  *hb = InitModelCollection(1);
  hb->isActive[0] = true;

  hb->models[0] = LoadModelFromMesh(GenMeshCube(size.x, size.y, size.z));
  hb->offsets[0] = Vector3Zero();
  hb->parentIds[0] = -1;

  entity_t id = MakeEntityID(ET_STATIC, i);
  return id;
}

static int CreateStaticModel(Engine_t *eng, Vector3 pos, const char *modelPath,
                             Color tint) {
  // Find an empty slot
  int i = 0;
  while (i < MAX_STATICS && eng->statics.modelCollections[i].countModels != 0)
    i++;

  if (i >= MAX_STATICS)
    return -1;

  eng->statics.positions[i] = pos;

  // -------------------------------
  // Load 3D model (visual)
  // -------------------------------
  ModelCollection_t *mc = &eng->statics.modelCollections[i];
  *mc = InitModelCollection(1);

  mc->models[0] = LoadModel(modelPath);
  mc->models[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = tint;

  mc->offsets[0] = Vector3Zero();
  mc->parentIds[0] = -1;

  // -------------------------------
  // Build collision from bounding box
  // -------------------------------
  BoundingBox bb = GetMeshBoundingBox(mc->models[0].meshes[0]);
  Vector3 size = Vector3Subtract(bb.max, bb.min);

  size.y -= 30;

  // COLLISION MODEL
  ModelCollection_t *col = &eng->statics.collisionCollections[i];
  *col = InitModelCollection(1);
  col->models[0] = LoadModelFromMesh(GenMeshCube(size.x, size.y, size.z));
  col->offsets[0] = Vector3Zero();
  col->parentIds[0] = -1;

  // HITBOX MODEL (same as collision)
  ModelCollection_t *hb = &eng->statics.hitboxCollections[i];
  *hb = InitModelCollection(1);
  hb->models[0] = LoadModelFromMesh(GenMeshCube(size.x, size.y, size.z));
  hb->offsets[0] = Vector3Zero();
  hb->parentIds[0] = -1;

  return MakeEntityID(ET_STATIC, i);
}

// Creates an actor target
entity_t CreateTargetActor(Engine_t *eng, ActorComponentRegistry_t compReg,
                           Vector3 pos,
                           const char *modelPath, // visual model
                           float hp, Color tint) {
  int e = eng->em.count++;

  eng->em.alive[e] = 1;

  //--------------------------------------------------------
  // COMPONENT MASK
  //--------------------------------------------------------
  // NO C_COLLISION → does not block movement
  // YES C_HITBOX → projectiles should hit it
  // YES C_HITPOINT_TAG → it has HP and uses death system
  eng->em.masks[e] = C_POSITION | C_MODEL | C_HITBOX | C_HITPOINT_TAG;

  eng->actors.types[e] = ENTITY_TURRET;
  eng->actors.hitPoints[e] = hp;

  //--------------------------------------------------------
  // POSITION
  //--------------------------------------------------------
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_Positions, &pos);

  //--------------------------------------------------------
  // VISUAL MODEL
  //--------------------------------------------------------
  ModelCollection_t *mc = &eng->actors.modelCollections[e];
  *mc = InitModelCollection(2);

  // --------------------------------------------------------
  // MODEL 0: Main model from file
  // --------------------------------------------------------
  mc->models[0] = LoadModel(modelPath);
  Orientation standOrientation = {-PI / 2, 0, 0};

  mc->offsets[0] = Vector3Zero();
  mc->orientations[0] = standOrientation;
  mc->parentIds[0] = -1;
  mc->isActive[0] = true;

  //--------------------------------------------------------
  // HITBOX COLLECTION
  //--------------------------------------------------------
  ModelCollection_t *hb = &eng->actors.hitboxCollections[e];
  *hb = InitModelCollection(1);

  // Stand
  Mesh cube = GenMeshCube(20, 30, 20);
  hb->models[0] = LoadModelFromMesh(cube);
  hb->offsets[0] = Vector3Zero();
  hb->parentIds[0] = -1;
  hb->isActive[0] = true;

  ModelCollection_t *col = &eng->actors.collisionCollections[e];
  *col = InitModelCollection(0); // no solids

  return MakeEntityID(ET_ACTOR, e);
}

entity_t CreateEnvironmentObject(Engine_t *eng,
                                 ActorComponentRegistry_t compReg, Vector3 pos,
                                 Vector3 ori,
                                 const char *modelPath, // visual model
                                 float hp, Color tint) {
  int e = eng->em.count++;

  eng->em.alive[e] = 1;

  eng->em.masks[e] = C_POSITION | C_MODEL | C_HITBOX | C_HITPOINT_TAG;

  eng->actors.types[e] = ENTITY_ENVIRONMENT;
  eng->actors.hitPoints[e] = hp;

  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_Positions, &pos);

  ModelCollection_t *mc = &eng->actors.modelCollections[e];
  *mc = InitModelCollection(2);

  mc->models[0] = LoadModel(modelPath);
  Orientation standOrientation = {-PI / 2, 0, 0};
  standOrientation.yaw += ori.x;
  standOrientation.pitch += ori.x;
  standOrientation.roll += ori.x;

  mc->offsets[0] = Vector3Zero();
  mc->orientations[0] = standOrientation;
  mc->parentIds[0] = -1;
  mc->isActive[0] = true;

  ModelCollection_t *hb = &eng->actors.hitboxCollections[e];
  *hb = InitModelCollection(0);
  ModelCollection_t *col = &eng->actors.collisionCollections[e];
  *col = InitModelCollection(0); // no solids

  return MakeEntityID(ET_ACTOR, e);
}

entity_t CreateRockRandomOri(Engine_t *eng, ActorComponentRegistry_t compReg,
                             Vector3 pos) {
  int e = eng->em.count++;
  int hp = 5000;
  char *modelPath = "assets/models/rocks.glb";

  eng->em.alive[e] = 1;

  eng->em.masks[e] = C_POSITION | C_MODEL | C_HITBOX | C_HITPOINT_TAG;

  eng->actors.types[e] = ENTITY_ROCK;
  eng->actors.hitPoints[e] = hp;

  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_Positions, &pos);

  ModelCollection_t *mc = &eng->actors.modelCollections[e];
  *mc = InitModelCollection(2);

  mc->models[0] = LoadModel(modelPath);
  Orientation standOrientation = {-PI / 2, 0, 0};
  float randomYaw = ((float)GetRandomValue(0, 360)) * DEG2RAD;
  standOrientation.yaw += randomYaw;

  mc->offsets[0] = Vector3Zero();
  mc->orientations[0] = standOrientation;
  mc->parentIds[0] = -1;
  mc->isActive[0] = true;

  ModelCollection_t *hb = &eng->actors.hitboxCollections[e];
  *hb = InitModelCollection(0);
  ModelCollection_t *col = &eng->actors.collisionCollections[e];
  *col = InitModelCollection(0); // no solids

  return MakeEntityID(ET_ACTOR, e);
}

static entity_t CreateTurret(Engine_t *eng, ActorComponentRegistry_t compReg,
                             Vector3 pos) {
  entity_t e = eng->em.count++;
  eng->em.alive[e] = 1;
  eng->em.masks[e] = C_POSITION | C_MODEL | C_HITBOX | C_HITPOINT_TAG |
                     C_TURRET_BEHAVIOUR_1 | C_COOLDOWN_TAG | C_RAYCAST |
                     C_GRAVITY;

  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_Positions, &pos);
  eng->actors.types[e] = ENTITY_TURRET;
  eng->actors.hitPoints[e] = 200.0f;

  // visual models (base + barrel)
  ModelCollection_t *mc = &eng->actors.modelCollections[e];
  *mc = InitModelCollection(2);

  mc->models[0] = LoadModelFromMesh(GenMeshCylinder(2.0f, 5.0f, 5));
  mc->offsets[0] = (Vector3){0, 0, 0};
  mc->parentIds[0] = -1;

  mc->models[1] = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 6.0f));
  mc->offsets[1] = (Vector3){0, 5.0f, 3.0f};
  mc->parentIds[1] = 0;

  // hitbox
  ModelCollection_t *hb = &eng->actors.hitboxCollections[e];
  *hb = InitModelCollection(1);
  hb->models[0] = LoadModelFromMesh(GenMeshCube(10, 10, 10));
  hb->offsets[0] = Vector3Zero();
  hb->parentIds[0] = -1;

  // ray: attach to barrel (model 1) muzzle
  eng->actors.rayCounts[e] = 0;
  AddRayToEntity(eng, e, 1, (Vector3){0, 0, 0}, (Orientation){0, 0, 0}, 500.0f);

  // cooldown & firerate
  eng->actors.cooldowns[e] = (float *)malloc(sizeof(float) * 1);
  eng->actors.cooldowns[e][0] = 0.0f;
  eng->actors.firerate[e] = (float *)malloc(sizeof(float) * 1);
  eng->actors.firerate[e][0] = 0.4f;

  entity_t id = MakeEntityID(ET_STATIC, e);
  return id;
}

static entity_t CreateDestructible(Engine_t *eng,
                                   ActorComponentRegistry_t compReg,
                                   Vector3 pos, float hitPoints,
                                   const char *modelPath, Color tint) {
  //-----------------------------------------------------
  // Allocate new ACTOR entity (because it has HP + hitbox)
  //-----------------------------------------------------
  int e = eng->em.count++;
  eng->em.alive[e] = 1;

  eng->em.masks[e] =
      C_POSITION | C_MODEL | C_COLLISION | C_HITBOX | C_HITPOINT_TAG | C_SOLID;

  eng->actors.types[e] = ENTITY_DESTRUCT;
  eng->actors.hitPoints[e] = hitPoints;

  //-----------------------------------------------------
  // Position component
  //-----------------------------------------------------
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_Positions, &pos);

  //-----------------------------------------------------
  // VISUAL MODEL
  //-----------------------------------------------------
  ModelCollection_t *mc = &eng->actors.modelCollections[e];
  *mc = InitModelCollection(2);
  mc->isActive[0] = true;
  mc->isActive[1] = false;

  mc->models[0] = LoadModel(modelPath);
  mc->models[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = tint;
  mc->offsets[0] = (Vector3){0, 0, 0};
  mc->parentIds[0] = -1;

  mc->models[1] = LoadModel("assets/models/fuel-tank2.glb");
  mc->models[1].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = tint;
  mc->offsets[1] = (Vector3){0, 0, 0};
  mc->parentIds[1] = -1;

  //-----------------------------------------------------
  // Extract true mesh bounds to build hitbox + collision box
  //-----------------------------------------------------
  BoundingBox bb = GetMeshBoundingBox(mc->models[0].meshes[0]);

  Vector3 half = Vector3Scale(Vector3Subtract(bb.max, bb.min), 0.5f);
  Vector3 size = Vector3Scale(half, 2.0f); // full extents

  //-----------------------------------------------------
  // COLLISION MODEL (same size as bounding box)
  //-----------------------------------------------------
  ModelCollection_t *col = &eng->actors.collisionCollections[e];
  *col = InitModelCollection(1);

  Mesh colMesh = GenMeshCube(size.x, size.y, size.z);
  col->models[0] = LoadModelFromMesh(colMesh);
  col->offsets[0] = (Vector3){0, 20, 0};
  col->parentIds[0] = -1;

  //-----------------------------------------------------
  // HITBOX MODEL (same size, normally identical)
  //-----------------------------------------------------
  ModelCollection_t *hb = &eng->actors.hitboxCollections[e];
  *hb = InitModelCollection(1);

  Mesh hbMesh = GenMeshCube(size.x, size.y, size.z);
  hb->models[0] = LoadModelFromMesh(hbMesh);
  hb->offsets[0] = (Vector3){0, 20, 0};
  hb->parentIds[0] = -1;

  //-----------------------------------------------------
  // Return final ID
  //-----------------------------------------------------
  return MakeEntityID(ET_ACTOR, e);
}

entity_t CreateTextTriggerCube(Engine_t *eng, GameState_t *gs, Vector3 pos,
                               Vector3 size, char *text) {
  int e = eng->em.count++;
  eng->em.alive[e] = 1;

  // This entity receives collision events but is NOT solid
  eng->em.masks[e] = C_POSITION | C_TRIGGER;

  eng->actors.types[e] = ENTITY_TRIGGER;
  eng->actors.OnCollideTexts[e] = text;

  //-----------------------------------------------------
  // POSITION COMPONENT
  //-----------------------------------------------------
  addComponentToElement(&eng->em, &eng->actors, e, gs->compReg.cid_Positions,
                        &pos);

  //-----------------------------------------------------
  // MODEL (visual)
  //-----------------------------------------------------
  ModelCollection_t *mc = &eng->actors.modelCollections[e];
  *mc = InitModelCollection(1);

  Mesh mesh = GenMeshCube(size.x, size.y, size.z);
  mc->models[0] = LoadModelFromMesh(mesh);
  mc->offsets[0] = Vector3Zero();
  mc->parentIds[0] = -1;
  mc->isActive[0] = false;

  // initial color = RED
  mc->models[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = LIGHTGRAY;

  //-----------------------------------------------------
  // COLLISION COLLECTION (same size)
  //-----------------------------------------------------
  ModelCollection_t *col = &eng->actors.collisionCollections[e];
  *col = InitModelCollection(1);

  col->models[0] = LoadModelFromMesh(GenMeshCube(size.x, size.y, size.z));
  col->offsets[0] = Vector3Zero();
  col->parentIds[0] = -1;
  col->isActive[0] = true;

  //-----------------------------------------------------
  // BEHAVIOR CALLBACK
  //-----------------------------------------------------
  BehaviorCallBacks_t cb = {0};
  cb.onCollision = Cube_OnCollision;
  cb.onCollisionExit = Cube_OnCollisionExit;
  cb.onDeath = NULL;
  cb.isColliding = false;

  addComponentToElement(&eng->em, &eng->actors, e, gs->compReg.cid_behavior,
                        &cb);

  //-----------------------------------------------------
  // Return entity ID
  //-----------------------------------------------------
  return MakeEntityID(ET_ACTOR, e);
}

static entity_t CreateTankAlpha(Engine_t *eng, ActorComponentRegistry_t compReg,
                                Vector3 pos) {
  entity_t e = eng->em.count++;
  eng->em.alive[e] = 1;
  eng->em.masks[e] = C_POSITION | C_MODEL | C_HITBOX | C_HITPOINT_TAG |
                     C_TURRET_BEHAVIOUR_1 | C_TANK_MOVEMENT | C_COOLDOWN_TAG |
                     C_RAYCAST | C_GRAVITY;

  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_Positions, &pos);
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_prevPositions,
                        &pos);

  Vector3 vel = {0, 0, 0};
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_velocities,
                        &vel);

  Vector3 moveTarget = {0, 0, 0};
  Vector3 aimTarget = {0, 0, 0};
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_aimTarget,
                        &aimTarget);
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_moveTarget,
                        &moveTarget);

  float timer = 0.0f;
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_moveTimer,
                        &timer);

  int moveBehaviour = 1;
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_moveBehaviour,
                        &moveBehaviour);

  float maxAimError = 1.5f;
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_aimError,
                        &maxAimError);

  int weaponCount = 2;
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_weaponCount,
                        &weaponCount);

  eng->actors.types[e] = ENTITY_TANK_ALPHA;
  eng->actors.hitPoints[e] = 500.0f;

  // ----- models (same as CreateTank) -----
  ModelCollection_t *mc = &eng->actors.modelCollections[e];
  *mc = InitModelCollection(3);

  mc->models[0] = LoadModel("assets/models/enemy-alpha-hull.glb");
  mc->models[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = BLACK;
  mc->offsets[0] = (Vector3){0, 0, 0};
  mc->parentIds[0] = -1;

  mc->models[1] = LoadModel("assets/models/enemy-alpha-turret.glb");
  mc->models[1].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = GRAY;
  mc->offsets[1] = (Vector3){0, 15, 5};
  mc->parentIds[1] = 0;

  mc->rotLocks[1][0] = true;
  mc->rotLocks[1][1] = true;
  mc->rotLocks[1][2] = true;

  mc->models[2] = LoadModel("assets/models/enemy-alpha-gun.glb");
  mc->orientations[2] = (Orientation){0, 0, 0};
  mc->offsets[2] = (Vector3){0, 1, 5};
  mc->parentIds[2] = 1;

  mc->rotLocks[2][0] = true;
  mc->rotLocks[2][1] = true;
  mc->rotLocks[2][2] = false;

  mc->orientations[0] = (Orientation){0, 0, 0};
  mc->orientations[1] = (Orientation){PI, 0, 0};

  // ----- hitbox -----
  ModelCollection_t *hb = &eng->actors.hitboxCollections[e];
  *hb = InitModelCollection(1);
  hb->models[0] = LoadModelFromMesh(GenMeshCube(50, 40, 50));
  hb->offsets[0] = (Vector3){0, 0, 0};
  hb->parentIds[0] = -1;

  // ----- ray (same barrel) -----
  eng->actors.rayCounts[e] = 0;
  AddRayToEntity(eng, e, 2, (Vector3){0, 0, 0}, (Orientation){0, 0, 0}, 500.0f);

  // ----- weapon arrays (size 2) -----
  eng->actors.cooldowns[e] = (float *)malloc(sizeof(float) * 2);
  eng->actors.firerate[e] = (float *)malloc(sizeof(float) * 2);

  eng->actors.muzzleVelocities[e] = (float *)MemAlloc(sizeof(float) * 2);
  eng->actors.dropRates[e] = (float *)MemAlloc(sizeof(float) * 2);

  // weapon 0: current gun (bullets)
  float r = 0.1f + ((float)GetRandomValue(0, 1000) / 1000.0f) * 5.4f;
  eng->actors.cooldowns[e][0] = 1.4f + r;
  eng->actors.firerate[e][0] = 0.5f;
  eng->actors.muzzleVelocities[e][0] = 2800.0f;
  eng->actors.dropRates[e][0] = 20.0f;

  // weapon 1: missile launcher (P_MISSILE)
  eng->actors.cooldowns[e][1] = 3.0f;          // initial delay
  eng->actors.firerate[e][1] = 2.0f;           // one missile every 8s (tune)
  eng->actors.muzzleVelocities[e][1] = 600.0f; // missile speed (used at spawn)
  eng->actors.dropRates[e][1] = 0.0f;          // no gravity for missile

  return MakeEntityID(ET_ACTOR, e);
}

static entity_t CreateTank(Engine_t *eng, ActorComponentRegistry_t compReg,
                           Vector3 pos) {
  entity_t e = eng->em.count++;
  eng->em.alive[e] = 1;
  eng->em.masks[e] = C_POSITION | C_MODEL | C_HITBOX | C_HITPOINT_TAG |
                     C_TURRET_BEHAVIOUR_1 | C_TANK_MOVEMENT | C_COOLDOWN_TAG |
                     C_RAYCAST | C_GRAVITY;
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_Positions, &pos);
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_prevPositions,
                        &pos);
  Vector3 vel = {0, 0, 0};
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_velocities,
                        &vel);

  Vector3 moveTarget = {0, 0, 0};
  Vector3 aimTarget = {0, 0, 0};
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_aimTarget,
                        &aimTarget);
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_moveTarget,
                        &moveTarget);
  float timer = 0;
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_moveTimer,
                        &timer);
  int moveBehaviour = 1;
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_moveBehaviour,
                        &moveBehaviour);

  float maxAimError = 0.5f; // Aim error radius in radians
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_aimError,
                        &maxAimError);

  int weaponCount = 1;
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_weaponCount,
                        &weaponCount);

  eng->actors.types[e] = ENTITY_TANK;
  eng->actors.hitPoints[e] = 20.0f;

  // visual models (base + turret + barrel)
  ModelCollection_t *mc = &eng->actors.modelCollections[e];
  *mc = InitModelCollection(3); // Now 3 models!

  // Model 0: Tank base (body) - rotates with movement
  // mc->models[0] = LoadModelFromMesh(GenMeshCylinder(3.0f, 6.0f, 8));
  mc->models[0] = LoadModel("assets/models/enemy1-tank-hull.glb");
  mc->models[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = BLACK;
  mc->offsets[0] = (Vector3){0, -4, 0}; // Center at y=3 (half height)
  mc->parentIds[0] = -1;                // No parent

  // Model 1: Turret (rotates horizontally/yaw only)
  // mc->models[1] = LoadModelFromMesh(GenMeshCylinder(2.5f, 3.0f, 8));
  mc->models[1] = LoadModel("assets/models/enemy1-tank-turret.glb");
  mc->models[1].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = GRAY;
  mc->offsets[1] = (Vector3){0, 8, 0}; // On top of base
  mc->parentIds[1] = 0;                // Parented to base

  mc->rotLocks[1][0] = true;
  mc->rotLocks[1][1] = true;
  mc->rotLocks[1][2] = true;

  // Model 2: Barrel (rotates vertically/pitch only, parented to turret)
  mc->models[2] = LoadModel("assets/models/enemy1-gun.glb");
  // mc->models[2] = LoadModel("assets/models/enemy-1-cyclops-gun.glb");
  mc->orientations[2] = (Orientation){0, 0, 0};
  mc->offsets[2] = (Vector3){0, 1, 3}; // Forward from turret center
  mc->parentIds[2] = 1;                // Parented to turret

  mc->rotLocks[2][0] = true;
  mc->rotLocks[2][1] = true;
  mc->rotLocks[2][2] = false;

  mc->orientations[0].yaw = 0;
  mc->orientations[0].pitch = 0;
  mc->orientations[0].roll = 0;

  // Turret: starts aligned with base
  mc->orientations[1].yaw = PI;
  mc->orientations[1].pitch = 0;
  mc->orientations[1].roll = 0;

  // hitbox
  ModelCollection_t *hb = &eng->actors.hitboxCollections[e];
  *hb = InitModelCollection(1);
  hb->models[0] = LoadModelFromMesh(GenMeshCube(25, 20, 25));
  hb->offsets[0] = (Vector3){0, 0, 0};
  hb->parentIds[0] = -1;

  // ray: attach to barrel (model 1) muzzle
  eng->actors.rayCounts[e] = 0;
  AddRayToEntity(eng, e, 2, (Vector3){0, 0, 0}, (Orientation){0, 0, 0}, 500.0f);

  // cooldown & firerate
  eng->actors.cooldowns[e] = (float *)malloc(sizeof(float) * 1);

  float r = 0.1f + ((float)GetRandomValue(0, 1000) / 1000.0f) * 5.4f;
  eng->actors.cooldowns[e][0] = 1.4f + r;
  eng->actors.firerate[e] = (float *)malloc(sizeof(float) * 1);
  eng->actors.firerate[e][0] = 5.5f;
  eng->actors.muzzleVelocities[e] = MemAlloc(sizeof(float) * 2);
  eng->actors.muzzleVelocities[e][0] = 2500.0f;
  eng->actors.dropRates[e] = MemAlloc(sizeof(float) * 2);
  eng->actors.dropRates[e][0] = 20.0f;

  entity_t id = MakeEntityID(ET_ACTOR, e);
  return id;
}

static entity_t CreateHarasser(Engine_t *eng, ActorComponentRegistry_t compReg,
                               Vector3 pos) {
  entity_t e = eng->em.count++;
  eng->em.alive[e] = 1;
  eng->em.masks[e] = C_POSITION | C_MODEL | C_HITBOX | C_HITPOINT_TAG |
                     C_AIRHARASSER_MOVEMENT | C_COOLDOWN_TAG | C_RAYCAST;

  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_Positions, &pos);
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_prevPositions,
                        &pos);
  Vector3 vel = {0, 0, 0};
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_velocities,
                        &vel);

  Vector3 moveTarget = {0, 0, 0};
  Vector3 aimTarget = {0, 0, 0};
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_aimTarget,
                        &aimTarget);
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_moveTarget,
                        &moveTarget);
  float timer = 0;
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_moveTimer,
                        &timer);
  int moveBehaviour = 1;
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_moveBehaviour,
                        &moveBehaviour);

  float maxAimError = 0.5f; // Aim error radius in radians
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_aimError,
                        &maxAimError);

  int weaponCount = 1;
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_weaponCount,
                        &weaponCount);

  eng->actors.types[e] = ENTITY_HARASSER;
  eng->actors.hitPoints[e] = 6.0f;

  // visual models (base + turret + barrel)
  ModelCollection_t *mc = &eng->actors.modelCollections[e];
  *mc = InitModelCollection(2); // Now 3 models!

  // Model 0: Tank base (body) - rotates with movement
  // mc->models[0] = LoadModelFromMesh(GenMeshCylinder(3.0f, 6.0f, 8));
  mc->models[0] = LoadModel("assets/models/enemy1-barrel-fuselage.glb");
  mc->models[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = BLACK;
  mc->offsets[0] = (Vector3){0, -4, 0}; // Center at y=3 (half height)
  mc->parentIds[0] = -1;                // No parent

  // Model 1: Turret (rotates horizontally/yaw only)
  // mc->models[1] = LoadModelFromMesh(GenMeshCylinder(2.5f, 3.0f, 8));
  mc->models[1] = LoadModel("assets/models/enemy1-barrel-gun.glb");
  mc->models[1].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = GRAY;
  mc->offsets[1] = (Vector3){0, -1.5, 15};
  mc->parentIds[1] = 0; // Parented to base

  mc->rotLocks[1][0] = true;
  mc->rotLocks[1][1] = true;
  mc->rotLocks[1][2] = true;

  mc->orientations[0].yaw = 0;
  mc->orientations[0].pitch = 0;
  mc->orientations[0].roll = 0;

  // Turret: starts aligned with base
  mc->orientations[1].yaw = PI;
  mc->orientations[1].pitch = 0;
  mc->orientations[1].roll = 0;

  // hitbox
  ModelCollection_t *hb = &eng->actors.hitboxCollections[e];
  *hb = InitModelCollection(1);
  hb->models[0] = LoadModelFromMesh(GenMeshCube(25, 20, 25));
  hb->offsets[0] = (Vector3){0, 0, 0};
  hb->parentIds[0] = -1;

  // ray: attach to barrel (model 1) muzzle
  eng->actors.rayCounts[e] = 0;
  AddRayToEntity(eng, e, 1, (Vector3){0, 0, 0}, (Orientation){0, 0, 0}, 500.0f);

  // cooldown & firerate
  eng->actors.cooldowns[e] = (float *)malloc(sizeof(float) * 1);

  float r = 0.1f + ((float)GetRandomValue(0, 1000) / 1000.0f) * 5.4f;
  eng->actors.cooldowns[e][0] = 1.4f + r;
  eng->actors.firerate[e] = (float *)malloc(sizeof(float) * 1);
  eng->actors.firerate[e][0] = 5.5f;
  eng->actors.muzzleVelocities[e] = MemAlloc(sizeof(float) * 2);
  eng->actors.muzzleVelocities[e][0] = 2500.0f;
  eng->actors.dropRates[e] = MemAlloc(sizeof(float) * 2);
  eng->actors.dropRates[e][0] = 20.0f;

  entity_t id = MakeEntityID(ET_ACTOR, e);
  return id;
}

float GetTerrainHeightAtPosition(Terrain_t *terrain, float wx, float wz) {

  float minX = terrain->minX;
  float minZ = terrain->minZ;

  int ix = (int)((wx - minX) / terrain->cellSizeX);
  int iz = (int)((wz - minZ) / terrain->cellSizeZ);

  if (ix < 0)
    ix = 0;
  if (iz < 0)
    iz = 0;
  if (ix >= terrain->hmWidth)
    ix = terrain->hmWidth - 1;
  if (iz >= terrain->hmHeight)
    iz = terrain->hmHeight - 1;

  printf("STATIC INIT height %f\n",
         terrain->height[iz * terrain->hmWidth + ix]);
  return terrain->height[iz * terrain->hmWidth + ix];
}

// GRID funcs

//----------------------------------------
// Add all entities to the grid
//----------------------------------------
void PopulateGridWithEntities(EntityGrid_t *grid,
                              ActorComponentRegistry_t compReg, Engine_t *eng) {
  // --- Actors ---
  for (int i = 0; i < eng->em.count; i++) {
    if (!eng->em.alive[i])
      continue;
    Vector3 *pos =
        (Vector3 *)getComponent(&eng->actors, i, compReg.cid_Positions);
    GridAddEntity(grid, MakeEntityID(ET_ACTOR, i), *pos);
  }

  // --- Statics ---
  for (int i = 0; i < MAX_STATICS; i++) {
    if (eng->statics.modelCollections[i].countModels == 0)
      continue;
    Vector3 pos = eng->statics.positions[i];
    GridAddEntity(grid, MakeEntityID(ET_STATIC, i), pos);
  }

  // --- Projectiles ---
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    if (!eng->projectiles.active[i])
      continue;
    Vector3 pos = eng->projectiles.positions[i];
    GridAddEntity(grid, MakeEntityID(ET_PROJECTILE, i), pos);
  }
}

//----------------------------------------
// Print the grid as 2D array of entity IDs
//----------------------------------------
void PrintGrid(EntityGrid_t *grid) {
  printf("Grid (%d x %d):\n", grid->width, grid->length);

  for (int z = 0; z < grid->length; z++) {
    for (int x = 0; x < grid->width; x++) {
      GridNode_t *node = &grid->nodes[x][z];
      printf("%d ", node->count);
    }
    printf("\n"); // next row
  }
}

static void FreeActorDynamicData(Engine_t *eng) {
  // free per-entity weapon arrays
  for (int i = 0; i < MAX_ENTITIES; i++) {
    if (eng->actors.cooldowns[i]) {
      free(eng->actors.cooldowns[i]);
      eng->actors.cooldowns[i] = NULL;
    }
    if (eng->actors.firerate[i]) {
      free(eng->actors.firerate[i]);
      eng->actors.firerate[i] = NULL;
    }

    if (eng->actors.muzzleVelocities[i]) {
      MemFree(eng->actors.muzzleVelocities[i]);
      eng->actors.muzzleVelocities[i] = NULL;
    }
    if (eng->actors.dropRates[i]) {
      MemFree(eng->actors.dropRates[i]);
      eng->actors.dropRates[i] = NULL;
    }

    // If you allocate other per-entity pointers later, free them here too.
  }

  // free component store blocks (whatever registerComponent allocated)
  if (eng->actors.componentStore) {
    for (int c = 0; c < eng->actors.componentCount; c++) {
      if (eng->actors.componentStore[c].data) {
        free(eng->actors.componentStore[c].data);
        eng->actors.componentStore[c].data = NULL;
      }
      if (eng->actors.componentStore[c].occupied) {
        free(eng->actors.componentStore[c].occupied);
        eng->actors.componentStore[c].occupied = NULL;
      }
      eng->actors.componentStore[c].count = 0;
    }

    free(eng->actors.componentStore);
    eng->actors.componentStore = NULL;
  }

  eng->actors.componentCount = 0;
}

void ResetGameDuel(GameState_t *gs, Engine_t *eng) {
  // -------------------------
  // Reset gameplay state
  // -------------------------
  gs->heatMeter = 30;

  gs->banner.active = false;
  gs->banner.state = BANNER_HIDDEN;
  gs->banner.y = -80.0f;
  gs->banner.hiddenY = -80.0f;
  gs->banner.targetY = 0.0f;
  gs->banner.speed = 200.0f;
  gs->banner.visibleTime = 10.0f;

  gs->pHeadbobTimer = 0.0f;

  // -------------------------
  // Clear pools (projectiles/particles)
  // -------------------------
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    eng->projectiles.active[i] = false;
    eng->projectiles.positions[i] = Vector3Zero();
    eng->projectiles.velocities[i] = Vector3Zero();
    eng->projectiles.lifetimes[i] = 0.0f;
    eng->projectiles.radii[i] = 1.0f;
    eng->projectiles.owners[i] = -1;
    eng->projectiles.types[i] = -1;

    eng->projectiles.thrusterTimers[i] = 0.0f;
    // if you added missile homing arrays, reset them too:
    // eng->projectiles.homingDelay[i] = 0.0f;
    // eng->projectiles.homingTurnRate[i] = 0.0f;
  }

  for (int i = 0; i < MAX_PARTICLES; i++) {
    eng->particles.active[i] = false;
    eng->particles.lifetimes[i] = 0.0f;
    eng->particles.positions[i] = Vector3Zero();
    eng->particles.types[i] = -1;
  }

  // -------------------------
  // Reset statics pool (mark as unused)
  // -------------------------
  for (int i = 0; i < MAX_STATICS; i++) {
    eng->statics.positions[i] = Vector3Zero();

    // mark as empty; if you have an UnloadModelCollection, call it
    eng->statics.modelCollections[i].countModels = 0;
    eng->statics.collisionCollections[i].countModels = 0;
    eng->statics.hitboxCollections[i].countModels = 0;
  }

  // -------------------------
  // Reset ECS entities
  // -------------------------
  eng->em.count = 0;
  memset(eng->em.alive, 0, sizeof(eng->em.alive));
  memset(eng->em.masks, 0, sizeof(eng->em.masks));

  // free + recreate component store (since you register components in
  // InitGameDuel)
  FreeActorDynamicData(eng);

  // -------------------------
  // Clear grid contents (keep allocation)
  // -------------------------
  ClearGrid(&gs->grid);
}

static entity_t AcquireTank(GameState_t *gs) {
  for (int i = 0; i < MAX_POOL_TANKS; i++) {
    if (!gs->waves.tankUsed[i]) {
      gs->waves.tankUsed[i] = true;
      return gs->waves.tankPool[i];
    }
  }
  return (entity_t)0; // or some invalid
}

static entity_t AcquireHarasser(GameState_t *gs) {
  for (int i = 0; i < MAX_POOL_HARASSERS; i++) {
    if (!gs->waves.harasserUsed[i]) {
      gs->waves.harasserUsed[i] = true;
      return gs->waves.harasserPool[i];
    }
  }

  return (entity_t)0;
}

static entity_t AcquireAlphaTank(GameState_t *gs) {
  for (int i = 0; i < MAX_POOL_ALPHA; i++) {
    if (!gs->waves.alphaUsed[i]) {
      gs->waves.alphaUsed[i] = true;
      return gs->waves.alphaPool[i];
    }
  }

  return (entity_t)0;
}

static Vector3 PickSpawnAroundPlayer(GameState_t *gs, Engine_t *eng,
                                     float radiusMin, float radiusMax) {
  Vector3 *pPos = (Vector3 *)getComponent(&eng->actors, (entity_t)gs->playerId,
                                          gs->compReg.cid_Positions);
  pPos->x = 0;
  pPos->y = 0;
  Vector3 center = pPos ? *pPos : (Vector3){0, 0, 0};

  float a = (float)GetRandomValue(0, 359) * (PI / 180.0f);
  float r = (float)GetRandomValue((int)radiusMin, (int)radiusMax);

  float x = center.x + cosf(a) * r;
  float z = center.z + sinf(a) * r;
  float y = GetTerrainHeightAtPosition(&gs->terrain, x, z);

  return (Vector3){x, y, z};
}

void Wave1Start(GameState_t *gs, Engine_t *eng) {
  gs->waves.enemiesAliveThisWave = 0;
  TriggerMessage(gs,
                 "WAVE 1/6\n Watch out! You got a couple scout cars coming in");

  // 4 tanks
  for (int i = 0; i < 4; i++) {
    entity_t e = AcquireTank(gs);
    if (!e)
      break;

    Vector3 pos = PickSpawnAroundPlayer(gs, eng, 2500.0f, 3500.0f);
    ActivateEntityAt(gs, eng, e, pos);
    gs->waves.enemiesAliveThisWave++;
  }
}

void Wave2Start(GameState_t *gs, Engine_t *eng) {
  gs->waves.enemiesAliveThisWave = 0;
  TriggerMessage(gs,
                 "WAVE 2/6\n Enemy birds incoming! Use your blunderbus (E)");

  // 3 tanks
  for (int i = 0; i < 3; i++) {
    entity_t e = AcquireTank(gs);
    if (!e)
      break;

    Vector3 pos = PickSpawnAroundPlayer(gs, eng, 2500.0f, 3500.0f);
    ActivateEntityAt(gs, eng, e, pos);
    gs->waves.enemiesAliveThisWave++;
  }

  // 2 harassers
  for (int i = 0; i < 2; i++) {
    entity_t e = AcquireHarasser(gs);
    if (!e)
      break;

    Vector3 pos = PickSpawnAroundPlayer(gs, eng, 2500.0f, 3500.0f);
    ActivateEntityAt(gs, eng, e, pos);
    gs->waves.enemiesAliveThisWave++;
  }
}

void Wave3Start(GameState_t *gs, Engine_t *eng) {
  gs->waves.enemiesAliveThisWave = 0;
  TriggerMessage(gs,
                 "WAVE 3/6\n More enemies! Keep moving so you dont get hit");

  // 5 tanks
  for (int i = 0; i < 5; i++) {
    entity_t e = AcquireTank(gs);
    if (!e)
      break;

    Vector3 pos = PickSpawnAroundPlayer(gs, eng, 2500.0f, 3500.0f);
    ActivateEntityAt(gs, eng, e, pos);
    gs->waves.enemiesAliveThisWave++;
  }

  // 3 harassers
  for (int i = 0; i < 3; i++) {
    entity_t e = AcquireHarasser(gs);
    if (!e)
      break;

    Vector3 pos = PickSpawnAroundPlayer(gs, eng, 2500.0f, 3500.0f);
    ActivateEntityAt(gs, eng, e, pos);
    gs->waves.enemiesAliveThisWave++;
  }
}

void Wave4Start(GameState_t *gs, Engine_t *eng) {
  gs->waves.enemiesAliveThisWave = 0;
  TriggerMessage(
      gs,
      "WAVE 4/6\n They called in a larger tank, watch out for those missiles");

  // 1 alpha
  entity_t a = AcquireAlphaTank(gs);
  if (a) {
    Vector3 pos = PickSpawnAroundPlayer(gs, eng, 2500.0f, 3500.0f);
    ActivateEntityAt(gs, eng, a, pos);
    gs->waves.enemiesAliveThisWave++;
  }
}

void Wave5Start(GameState_t *gs, Engine_t *eng) {
  gs->waves.enemiesAliveThisWave = 0;
  TriggerMessage(
      gs, "WAVE 5/6 \n Nearly at the end, just hold out a little while longer");

  // 1 alpha
  entity_t a = AcquireAlphaTank(gs);
  if (a) {
    Vector3 pos = PickSpawnAroundPlayer(gs, eng, 2500.0f, 3500.0f);
    ActivateEntityAt(gs, eng, a, pos);
    gs->waves.enemiesAliveThisWave++;
  }

  // 2 tanks
  for (int i = 0; i < 5; i++) {
    entity_t e = AcquireTank(gs);
    if (!e)
      break;

    Vector3 pos = PickSpawnAroundPlayer(gs, eng, 2500.0f, 3500.0f);
    ActivateEntityAt(gs, eng, e, pos);
    gs->waves.enemiesAliveThisWave++;
  }

  // 1 harasser
  entity_t h = AcquireHarasser(gs);
  if (h) {
    Vector3 pos = PickSpawnAroundPlayer(gs, eng, 2500.0f, 3500.0f);
    ActivateEntityAt(gs, eng, h, pos);
    gs->waves.enemiesAliveThisWave++;
  }
}

void Wave6Start(GameState_t *gs, Engine_t *eng) {
  gs->waves.enemiesAliveThisWave = 0;
  TriggerMessage(gs, "WAVE 6/6 \n This should be the last of them");

  // 1 alpha
  entity_t a = AcquireAlphaTank(gs);
  if (a) {
    Vector3 pos = PickSpawnAroundPlayer(gs, eng, 2500.0f, 3500.0f);
    ActivateEntityAt(gs, eng, a, pos);
    gs->waves.enemiesAliveThisWave++;
  }

  // 4 tanks
  for (int i = 0; i < 10; i++) {
    entity_t e = AcquireTank(gs);
    if (!e)
      break;

    Vector3 pos = PickSpawnAroundPlayer(gs, eng, 2500.0f, 3500.0f);
    ActivateEntityAt(gs, eng, e, pos);
    gs->waves.enemiesAliveThisWave++;
  }

  // 2 harassers
  for (int i = 0; i < 4; i++) {
    entity_t e = AcquireHarasser(gs);
    if (!e)
      break;
    Vector3 pos = PickSpawnAroundPlayer(gs, eng, 2500.0f, 3500.0f);
    ActivateEntityAt(gs, eng, e, pos);
    gs->waves.enemiesAliveThisWave++;
  }
}

static void InitWavePools(GameState_t *gs, Engine_t *eng) {
  // reset bookkeeping
  memset(&gs->waves, 0, sizeof(gs->waves));
  gs->waves.state = WAVE_WAITING;
  gs->waves.waveIndex = 0;
  gs->waves.totalWaves = 6;           // whatever
  gs->waves.betweenWaveDelay = 15.0f; // seconds between waves
  gs->waves.betweenWaveTimer = 7.0f;  // first wave delay (optional)

  // spawn tanks
  for (int i = 0; i < MAX_POOL_TANKS; i++) {
    entity_t e = CreateTank(eng, gs->compReg, PARK_POS);
    gs->waves.tankPool[i] = e;
    gs->waves.tankUsed[i] = false;
    DeactivateEntity(gs, eng, e);
  }

  // spawn harassers
  for (int i = 0; i < MAX_POOL_HARASSERS; i++) {
    entity_t e = CreateHarasser(eng, gs->compReg, PARK_POS);
    gs->waves.harasserPool[i] = e;
    gs->waves.harasserUsed[i] = false;
    DeactivateEntity(gs, eng, e);
  }

  // spawn alphas
  for (int i = 0; i < MAX_POOL_ALPHA; i++) {
    entity_t e = CreateTankAlpha(eng, gs->compReg, PARK_POS);
    gs->waves.alphaPool[i] = e;
    gs->waves.alphaUsed[i] = false;
    DeactivateEntity(gs, eng, e);
  }
}

static void WaveSystemDefaults(WaveSystem_t *ws) {
  memset(ws, 0, sizeof(*ws));

  ws->state = WAVE_WAITING;
  ws->waveIndex = 0;

  ws->totalWaves = 5;          // change as needed
  ws->betweenWaveDelay = 5.0f; // seconds between waves
  ws->betweenWaveTimer = 2.0f; // delay before wave 1 starts (optional)

  ws->enemiesAliveThisWave = 0;

  // pools arrays are zeroed by memset:
  // tankPool/harasserPool/alphaPool = 0
  // tankUsed/harasserUsed/alphaUsed = false
}

void StartGameDuel(GameState_t *gs, Engine_t *eng) {
  // re-register components (same as your InitGameDuel)
  eng->actors.componentStore =
      malloc(sizeof(ComponentStorage_t) * MAX_COMPONENTS);
  memset(eng->actors.componentStore, 0,
         sizeof(ComponentStorage_t) * MAX_COMPONENTS);

  gs->compReg.cid_Positions = registerComponent(&eng->actors, sizeof(Vector3));
  gs->compReg.cid_velocities = registerComponent(&eng->actors, sizeof(Vector3));
  gs->compReg.cid_prevPositions =
      registerComponent(&eng->actors, sizeof(Vector3));
  gs->compReg.cid_weaponCount = registerComponent(&eng->actors, sizeof(int));
  gs->compReg.cid_weaponDamage = registerComponent(&eng->actors, sizeof(int *));
  gs->compReg.cid_behavior =
      registerComponent(&eng->actors, sizeof(BehaviorCallBacks_t));
  gs->compReg.cid_aimTarget = registerComponent(&eng->actors, sizeof(Vector3));
  gs->compReg.cid_aimError = registerComponent(&eng->actors, sizeof(float));
  gs->compReg.cid_moveTarget = registerComponent(&eng->actors, sizeof(Vector3));
  gs->compReg.cid_moveTimer = registerComponent(&eng->actors, sizeof(float));
  gs->compReg.cid_moveBehaviour = registerComponent(&eng->actors, sizeof(int));
  gs->compReg.cid_aiTimer = registerComponent(&eng->actors, sizeof(float));

  // // TIPS
  // gs->tips.index = 0;
  // gs->tips.count = 6;
  gs->tips.visible = false;

  float cellSize = GRID_CELL_SIZE;
  ClearGrid(&gs->grid);

  CreateSkybox(eng, (Vector3){0, 0, 0});

  Vector3 playerStartPos = (Vector3){0.0f, 20.0f, 0.0f};
  playerStartPos.y = GetTerrainHeightAtPosition(&gs->terrain, playerStartPos.x,
                                                playerStartPos.z);
  gs->playerId = GetEntityIndex(CreatePlayer(eng, gs->compReg, playerStartPos));

  // terrain
  Texture2D sandTex = LoadTexture("assets/textures/xtSand.png");
  InitTerrain(gs, eng, sandTex, "assets/models/terrain.glb");

  BuildHeightmap(&gs->terrain);

  Texture2D tankAimerTex = LoadTexture("assets/textures/tank-aimer.png");
  gs->tankAimerTex = tankAimerTex;

  CreateEnvironmentObject(eng, gs->compReg, (Vector3){7000, 1800, 0},
                          (Vector3){0, 0, 0},
                          "assets/models/megabuilding-1.glb", 1000, WHITE);
  CreateEnvironmentObject(
      eng, gs->compReg, (Vector3){-11000, 1600, 0}, (Vector3){0, 0, 0},
      "assets/models/megabuilding-2-radar.glb", 1000, WHITE);
  CreateEnvironmentObject(
      eng, gs->compReg, (Vector3){-13000, 1600, 5000}, (Vector3){0, 0, 0},
      "assets/models/megabuilding-2-radar.glb", 1000, WHITE);
  CreateEnvironmentObject(
      eng, gs->compReg, (Vector3){-13000, 1600, -5000}, (Vector3){0, 0, 0},
      "assets/models/megabuilding-2-radar.glb", 1000, WHITE);

  int rocksCount = 150;
  // scatter radius (tweak)
  float minRadius = 500.0f;
  float maxRadius = 3000.0f;

  for (int i = 0; i < rocksCount; i++) {
    float angle = ((float)GetRandomValue(0, 360)) * DEG2RAD;
    float radius = ((float)GetRandomValue((int)minRadius, (int)maxRadius));

    Vector3 pos;
    pos.x = cosf(angle) * radius;
    pos.z = sinf(angle) * radius;

    // snap to terrain
    pos.y = GetTerrainHeightAtPosition(&gs->terrain, pos.x, pos.z);

    CreateRockRandomOri(eng, gs->compReg, pos);
  }

  // ensure rayCounts initialized for any entities that weren't touched
  for (int i = 0; i < eng->em.count; i++) {
    if (eng->actors.rayCounts[i] == 0)
      eng->actors.rayCounts[i] = 0;
  }

  // Initialize projectile pool
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    eng->projectiles.active[i] = false;
    eng->projectiles.positions[i] = Vector3Zero();
    eng->projectiles.velocities[i] = Vector3Zero();
    eng->projectiles.lifetimes[i] = 0.0f;
    eng->projectiles.radii[i] = 1.0f; // default bullet size
    eng->projectiles.owners[i] = -1;
    eng->projectiles.types[i] = -1;
  }

  for (int i = 0; i < MAX_PARTICLES; i++) {
    eng->particles.active[i] = false;
    eng->particles.lifetimes[i] = 0;
    eng->particles.positions[i] = Vector3Zero();
    eng->particles.types[i] = -1;
  }

  WaveSystemDefaults(&gs->waves);

  InitWavePools(gs, eng);

  PopulateGridWithEntities(&gs->grid, gs->compReg, eng);
}

// -----------------------------------------------
// InitGame: orchestrates initialization
// -----------------------------------------------
GameState_t InitGameDuel(Engine_t *eng) {

  GameState_t *gs = (GameState_t *)malloc(sizeof(GameState_t));
  memset(gs, 0, sizeof(GameState_t));

  gs->outlineShader = LoadShader("src/outline.vs", "src/outline.fs");

  gs->heatMeter = 30;

  gs->banner.active = false;
  gs->banner.state = BANNER_HIDDEN;

  gs->banner.y = -80.0f; // initial off-screen
  gs->banner.hiddenY = -80.0f;
  gs->banner.targetY = 0.0f; // slide down to top of screen
  gs->banner.speed = 200.0f; // pixels/sec
  gs->banner.visibleTime = 5.0f;

  eng->em.count = 0;
  memset(eng->em.alive, 0, sizeof(eng->em.alive));
  memset(eng->em.masks, 0, sizeof(eng->em.masks));

  eng->actors.componentCount = 0;
  eng->actors.componentStore =
      malloc(sizeof(ComponentStorage_t) * MAX_COMPONENTS);
  memset(eng->actors.componentStore, 0,
         sizeof(ComponentStorage_t) * MAX_COMPONENTS);

  // REGISTER COMPONENTS
  gs->compReg.cid_Positions = registerComponent(&eng->actors, sizeof(Vector3));
  gs->compReg.cid_velocities = registerComponent(&eng->actors, sizeof(Vector3));
  gs->compReg.cid_prevPositions =
      registerComponent(&eng->actors, sizeof(Vector3));

  gs->compReg.cid_weaponCount = registerComponent(&eng->actors, sizeof(int));
  gs->compReg.cid_weaponDamage = registerComponent(&eng->actors, sizeof(int *));

  gs->compReg.cid_behavior =
      registerComponent(&eng->actors, sizeof(BehaviorCallBacks_t));

  gs->compReg.cid_aimTarget = registerComponent(&eng->actors, sizeof(Vector3));
  gs->compReg.cid_aimError = registerComponent(&eng->actors, sizeof(float));
  gs->compReg.cid_moveTarget = registerComponent(&eng->actors, sizeof(Vector3));
  gs->compReg.cid_moveTimer = registerComponent(&eng->actors, sizeof(float));
  gs->compReg.cid_moveBehaviour = registerComponent(&eng->actors, sizeof(int));

  gs->compReg.cid_aiTimer = registerComponent(&eng->actors, sizeof(float));
  // END REGISTER COMPONENTS

  // // TIPS
  // gs->tips.index = 0;
  // gs->tips.count = 6;
  gs->tips.visible = false;

  gs->state = STATE_INLEVEL;
  gs->pHeadbobTimer = 0.0f;

  // terrain
  Texture2D sandTex = LoadTexture("assets/textures/xtSand.png");
  InitTerrain(gs, eng, sandTex, "assets/models/terrain.glb");

  BuildHeightmap(&gs->terrain);

  Texture2D tankAimerTex = LoadTexture("assets/textures/tank-aimer.png");
  gs->tankAimerTex = tankAimerTex;

  CreateEnvironmentObject(eng, gs->compReg, (Vector3){7000, 1800, 0},
                          (Vector3){0, 0, 0},
                          "assets/models/megabuilding-1.glb", 1000, WHITE);
  CreateEnvironmentObject(
      eng, gs->compReg, (Vector3){-11000, 1600, 0}, (Vector3){0, 0, 0},
      "assets/models/megabuilding-2-radar.glb", 1000, WHITE);
  CreateEnvironmentObject(
      eng, gs->compReg, (Vector3){-13000, 1600, 5000}, (Vector3){0, 0, 0},
      "assets/models/megabuilding-2-radar.glb", 1000, WHITE);
  CreateEnvironmentObject(
      eng, gs->compReg, (Vector3){-13000, 1600, -5000}, (Vector3){0, 0, 0},
      "assets/models/megabuilding-2-radar.glb", 1000, WHITE);

  CreateSkybox(eng, (Vector3){0, 0, 0});

  float cellSize = GRID_CELL_SIZE;
  AllocGrid(&gs->grid, &gs->terrain, cellSize);

  Vector3 playerStartPos = (Vector3){0.0f, 20.0f, 0.0f};
  playerStartPos.y = GetTerrainHeightAtPosition(&gs->terrain, playerStartPos.x,
                                                playerStartPos.z);

  int rocksCount = 150;
  // scatter radius (tweak)
  float minRadius = 500.0f;
  float maxRadius = 3000.0f;

  for (int i = 0; i < rocksCount; i++) {
    float angle = ((float)GetRandomValue(0, 360)) * DEG2RAD;
    float radius = ((float)GetRandomValue((int)minRadius, (int)maxRadius));

    Vector3 pos;
    pos.x = cosf(angle) * radius;
    pos.z = sinf(angle) * radius;

    // snap to terrain
    pos.y = GetTerrainHeightAtPosition(&gs->terrain, pos.x, pos.z);

    CreateRockRandomOri(eng, gs->compReg, pos);
  }

  // create player at origin-ish
  gs->playerId = GetEntityIndex(CreatePlayer(eng, gs->compReg, playerStartPos));

  // ensure rayCounts initialized for any entities that weren't touched
  for (int i = 0; i < eng->em.count; i++) {
    if (eng->actors.rayCounts[i] == 0)
      eng->actors.rayCounts[i] = 0;
  }

  // Initialize projectile pool
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    eng->projectiles.active[i] = false;
    eng->projectiles.positions[i] = Vector3Zero();
    eng->projectiles.velocities[i] = Vector3Zero();
    eng->projectiles.lifetimes[i] = 0.0f;
    eng->projectiles.radii[i] = 1.0f; // default bullet size
    eng->projectiles.owners[i] = -1;
    eng->projectiles.types[i] = -1;
  }

  for (int i = 0; i < MAX_PARTICLES; i++) {
    eng->particles.active[i] = false;
    eng->particles.lifetimes[i] = 0;
    eng->particles.positions[i] = Vector3Zero();
    eng->particles.types[i] = -1;
  }

  WaveSystemDefaults(&gs->waves);

  InitWavePools(gs, eng);

  PopulateGridWithEntities(&gs->grid, gs->compReg, eng);

  // PrintGrid(&gs->grid);

  return *gs;
}

void StartGameTutorial(GameState_t *gs, Engine_t *eng) {
  ResetGameDuel(gs, eng);

  // re-register components (same as your InitGameDuel)
  eng->actors.componentStore =
      malloc(sizeof(ComponentStorage_t) * MAX_COMPONENTS);
  memset(eng->actors.componentStore, 0,
         sizeof(ComponentStorage_t) * MAX_COMPONENTS);

  gs->compReg.cid_Positions = registerComponent(&eng->actors, sizeof(Vector3));
  gs->compReg.cid_velocities = registerComponent(&eng->actors, sizeof(Vector3));
  gs->compReg.cid_prevPositions =
      registerComponent(&eng->actors, sizeof(Vector3));
  gs->compReg.cid_weaponCount = registerComponent(&eng->actors, sizeof(int));
  gs->compReg.cid_weaponDamage = registerComponent(&eng->actors, sizeof(int *));
  gs->compReg.cid_behavior =
      registerComponent(&eng->actors, sizeof(BehaviorCallBacks_t));
  gs->compReg.cid_aimTarget = registerComponent(&eng->actors, sizeof(Vector3));
  gs->compReg.cid_aimError = registerComponent(&eng->actors, sizeof(float));
  gs->compReg.cid_moveTarget = registerComponent(&eng->actors, sizeof(Vector3));
  gs->compReg.cid_moveTimer = registerComponent(&eng->actors, sizeof(float));
  gs->compReg.cid_moveBehaviour = registerComponent(&eng->actors, sizeof(int));
  gs->compReg.cid_aiTimer = registerComponent(&eng->actors, sizeof(float));

  // TIPS
  gs->tips.index = 0;
  gs->tips.count = 6;
  gs->tips.visible = true;

  float cellSize = GRID_CELL_SIZE;
  ClearGrid(&gs->grid);

  CreateSkybox(eng, (Vector3){0, 0, 0});

  Vector3 playerStartPos = (Vector3){0.0f, 20.0f, 0.0f};
  playerStartPos.y = GetTerrainHeightAtPosition(&gs->terrain, playerStartPos.x,
                                                playerStartPos.z);
  gs->playerId = GetEntityIndex(CreatePlayer(eng, gs->compReg, playerStartPos));

  Texture2D tankAimerTex = LoadTexture("assets/textures/tank-aimer.png");
  gs->tankAimerTex = tankAimerTex;

  CreateEnvironmentObject(eng, gs->compReg, (Vector3){7000, 1800, 0},
                          (Vector3){0, 0, 0},
                          "assets/models/megabuilding-1.glb", 1000, WHITE);
  CreateEnvironmentObject(
      eng, gs->compReg, (Vector3){-11000, 1600, 0}, (Vector3){0, 0, 0},
      "assets/models/megabuilding-2-radar.glb", 1000, WHITE);
  CreateEnvironmentObject(
      eng, gs->compReg, (Vector3){-13000, 1600, 5000}, (Vector3){0, 0, 0},
      "assets/models/megabuilding-2-radar.glb", 1000, WHITE);
  CreateEnvironmentObject(
      eng, gs->compReg, (Vector3){-13000, 1600, -5000}, (Vector3){0, 0, 0},
      "assets/models/megabuilding-2-radar.glb", 1000, WHITE);

  Vector3 rangeStart = (Vector3){-100, 0, 100}; // starting 50 units ahead
  rangeStart.y =
      GetTerrainHeightAtPosition(&gs->terrain, rangeStart.x, rangeStart.z) + 5;
  rangeStart.x += 100;

  Vector3 t0 = rangeStart;
  t0.x += 500;
  t0.z -= 200;

  CreateStaticModel(eng, rangeStart, "assets/models/sandbags.glb", WHITE);
  // Distance markers every 500 units up to 5000 units
  int maxRange = 3000;
  for (int dist = 500; dist <= maxRange; dist += 500) {

    float x = rangeStart.x - dist / 10.0f + 200;
    float z = rangeStart.z + dist;
    float y = GetTerrainHeightAtPosition(&gs->terrain, x, z);

    bool isBig = (dist % 1000 == 0);

    Vector3 size;
    Color color;

    if (isBig) {

      CreateTargetActor(eng, gs->compReg, (Vector3){x, y, z},
                        "assets/models/enemy1-target.glb", 1500, WHITE);
      continue;
    } else {
      CreateTargetActor(eng, gs->compReg, (Vector3){x, y, z},
                        "assets/models/target-marker.glb", 1500, WHITE);
    }
  }

  int rocksCount = 150;
  float minRadius = 500.0f;
  float maxRadius = 3000.0f;

  for (int i = 0; i < rocksCount; i++) {
    float angle = ((float)GetRandomValue(0, 360)) * DEG2RAD;
    float radius = ((float)GetRandomValue((int)minRadius, (int)maxRadius));

    Vector3 pos;
    pos.x = cosf(angle) * radius;
    pos.z = sinf(angle) * radius;

    // snap to terrain
    pos.y = GetTerrainHeightAtPosition(&gs->terrain, pos.x, pos.z);

    CreateRockRandomOri(eng, gs->compReg, pos);
  }

  // ensure rayCounts initialized for any entities that weren't touched
  for (int i = 0; i < eng->em.count; i++) {
    if (eng->actors.rayCounts[i] == 0)
      eng->actors.rayCounts[i] = 0;
  }

  // Initialize projectile pool
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    eng->projectiles.active[i] = false;
    eng->projectiles.positions[i] = Vector3Zero();
    eng->projectiles.velocities[i] = Vector3Zero();
    eng->projectiles.lifetimes[i] = 0.0f;
    eng->projectiles.radii[i] = 1.0f; // default bullet size
    eng->projectiles.owners[i] = -1;
    eng->projectiles.types[i] = -1;
  }

  for (int i = 0; i < MAX_PARTICLES; i++) {
    eng->particles.active[i] = false;
    eng->particles.lifetimes[i] = 0;
    eng->particles.positions[i] = Vector3Zero();
    eng->particles.types[i] = -1;
  }

  WaveSystemDefaults(&gs->waves);

  InitWavePools(gs, eng);

  PopulateGridWithEntities(&gs->grid, gs->compReg, eng);

  WaveSystemDefaults(&gs->waves);
  gs->waves.totalWaves = 0;
  gs->waves.state = WAVE_FINISHED;

  TriggerMessage(gs, "Tutorial: learn movement + shooting");
}
