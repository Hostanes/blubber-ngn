// game_init.c
// Refactored initialization, multi-ray system, and entity factories.
// Requires game.h in the same include path.
#include "game.h"
#include "engine.h"
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
void InitTerrain(GameState_t *gs, Engine_t *eng, Texture2D sandTex) {
  Terrain_t *terrain = &gs->terrain;

  terrain->model = LoadModel("assets/models/terrain.glb");
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
                     C_GRAVITY;

  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_Positions, &pos);
  Vector3 vel = {0, 0, 0};
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_velocities,
                        &vel);
  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_prevPositions,
                        &pos);
  eng->actors.stepCycle[e] = 0;
  eng->actors.prevStepCycle[e] = 0;
  eng->actors.stepRate[e] = 2.0f;
  eng->actors.types[e] = ENTITY_PLAYER;

  // Model collection: 3 parts (legs, torso/head, gun)
  ModelCollection_t *mc = &eng->actors.modelCollections[e];
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
  // mc->models[2] = LoadModelFromMesh(gunMesh);
  mc->models[2] = LoadModel("assets/models/gun1.glb");
  mc->models[2].materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = mechTex;
  mc->offsets[2] = (Vector3){8.0f, -2, 10};
  mc->orientations[2] = (Orientation){0, PI/2, 0};
  mc->parentIds[2] = 1;

  mc->rotLocks[2][0] = true;
  mc->rotLocks[2][1] = true;
  mc->rotLocks[2][2] = false;

  // Mesh cockpitRoof = GenMeshCube(10.0f, 2.0f, 10.0f);
  // mc->models[3] = LoadModelFromMesh(cockpitRoof);
  // mc->models[3].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = BLACK;
  // mc->offsets[3] = (Vector3){0, 4, 0};
  // mc->parentIds[3] = 1;

  // mc->rotLocks[3][0] = true;
  // mc->rotLocks[3][1] = true;
  // mc->rotLocks[3][2] = false;

  // Mesh cockpitFloor = GenMeshCube(10.0f, 2.0f, 10.0f);
  // mc->models[4] = LoadModelFromMesh(cockpitFloor);
  // mc->models[4].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = BLACK;
  // mc->offsets[4] = (Vector3){0, -3.5, 0};
  // mc->parentIds[4] = 1;

  // mc->rotLocks[4][0] = true;
  // mc->rotLocks[4][1] = true;
  // mc->rotLocks[4][2] = false;

  // initialize rayCount and add a muzzle ray for the gun (model index 2)
  eng->actors.rayCounts[e] = 0;
  // Main aim ray - parent to torso (model index 1)
  AddRayToEntity(eng, e, 1,
                 (Vector3){0, 0, 0},     // offset near head/center of torso
                 (Orientation){0, 0, 0}, // forward
                 500.0f);                // long aim distance

  // Gun muzzle ray - still parent to gun (model index 2)
  AddRayToEntity(eng, e, 2, (Vector3){0, 0, 0}, (Orientation){0, 0, 0}, 500.0f);

  // 2 guns
  eng->actors.muzzleVelocities[0] = MemAlloc(sizeof(float) * 2);
  eng->actors.muzzleVelocities[0][0] = 1000.0f;
  eng->actors.dropRates[0] = MemAlloc(sizeof(float) * 2);
  eng->actors.dropRates[0][0] = 70.0f;

  // cooldown & firerate allocations
  eng->actors.cooldowns[e] = (float *)malloc(sizeof(float) * 1);
  eng->actors.cooldowns[e][0] = 0.8;
  eng->actors.firerate[e] = (float *)malloc(sizeof(float) * 1);
  eng->actors.firerate[e][0] = 0.8f;

  // Collision
  ModelCollection_t *col = &eng->actors.collisionCollections[e];
  *col = InitModelCollection(1);
  Mesh moveBox = GenMeshCube(4, 8, 4);
  col->models[0] = LoadModelFromMesh(moveBox);
  col->offsets[0] = (Vector3){0, 5, 0};

  // Hitbox
  ModelCollection_t *hit = &eng->actors.hitboxCollections[e];
  *hit = InitModelCollection(1);
  Mesh hitbox1 = GenMeshCube(4, 10, 4);
  hit->models[0] = LoadModelFromMesh(hitbox1);
  hit->offsets[0] = (Vector3){0, 2, 0};

  entity_t id = MakeEntityID(ET_STATIC, e);
  return id;
}

static int CreateStatic(Engine_t *eng, Vector3 pos, Vector3 size, Color c) {
  // find open slot
  int i = 0;
  while (i < MAX_ENTITIES && eng->statics.modelCollections[i].countModels != 0)
    i++;

  if (i >= MAX_ENTITIES)
    return -1; // no room

  eng->statics.positions[i] = pos;

  ModelCollection_t *mc = &eng->statics.modelCollections[i];
  *mc = InitModelCollection(1);

  Mesh cube = GenMeshCube(size.x, size.y, size.z);
  mc->models[0] = LoadModelFromMesh(cube);
  mc->models[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = c;

  mc->offsets[0] = Vector3Zero();
  mc->parentIds[0] = -1;

  // collision too
  ModelCollection_t *col = &eng->statics.collisionCollections[i];
  *col = InitModelCollection(1);
  col->models[0] = LoadModelFromMesh(GenMeshCube(size.x, size.y, size.z));
  col->offsets[0] = Vector3Zero();
  col->parentIds[0] = -1;

  // hitbox too
  ModelCollection_t *hb = &eng->statics.hitboxCollections[i];
  *hb = InitModelCollection(1);
  hb->models[0] = LoadModelFromMesh(GenMeshCube(size.x, size.y, size.z));
  hb->offsets[0] = Vector3Zero();
  hb->parentIds[0] = -1;

  entity_t id = MakeEntityID(ET_STATIC, i);
  return id;
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

static entity_t CreateMech(Engine_t *eng, ActorComponentRegistry_t compReg,
                           Vector3 pos) {
  entity_t e = eng->em.count++;
  eng->em.alive[e] = 1;
  eng->em.masks[e] = C_POSITION | C_MODEL | C_HITBOX | C_HITPOINT_TAG |
                     C_TURRET_BEHAVIOUR_1 | C_COOLDOWN_TAG | C_RAYCAST |
                     C_GRAVITY;

  addComponentToElement(&eng->em, &eng->actors, e, compReg.cid_Positions, &pos);
  eng->actors.types[e] = ENTITY_MECH;
  eng->actors.hitPoints[e] = 100.0f;

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
  hb->models[0] = LoadModelFromMesh(GenMeshCube(20, 20, 20));
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

      if (node->count == 0) {
        printf(" -1 ");
      } else {
        // Print first entity in cell for brevity (or all, comma-separated)
        for (int i = 0; i < node->count; i++) {
          printf("%d", GetEntityIndex(node->entities[i]));
          if (i < node->count - 1)
            printf(",");
        }
        printf(" ");
      }
    }
    printf("\n"); // next row
  }
}

// -----------------------------------------------
// InitGame: orchestrates initialization
// -----------------------------------------------
GameState_t InitGame(Engine_t *eng) {

  GameState_t *gs = (GameState_t *)malloc(sizeof(GameState_t));
  memset(gs, 0, sizeof(GameState_t));

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
  // END REGISTER COMPONENTS

  gs->state = STATE_INLEVEL;
  gs->pHeadbobTimer = 0.0f;

  // terrain
  Texture2D sandTex = LoadTexture("assets/textures/xtSand.png");
  InitTerrain(gs, eng, sandTex);

  BuildHeightmap(&gs->terrain);

  float cellSize = GRID_CELL_SIZE;
  AllocGrid(&gs->grid, &gs->terrain, cellSize);

  // create player at origin-ish
  gs->playerId =
      GetEntityIndex(CreatePlayer(eng, gs->compReg, (Vector3){0, 10.0f, 0}));

  // create a bunch of simple houses/walls
  int numStatics = 5;
  for (int i = 0; i < numStatics; i++) {
    float width = GetRandomValue(10, 40);
    float height = GetRandomValue(15, 55);
    float depth = GetRandomValue(10, 40);

    float x =
        GetRandomValue(-TERRAIN_SIZE / 2, TERRAIN_SIZE / 2) * TERRAIN_SCALE;
    float z =
        GetRandomValue(-TERRAIN_SIZE / 2, TERRAIN_SIZE / 2) * TERRAIN_SCALE;
    float y = GetTerrainHeightAtPosition(&gs->terrain, x, z);

    Color c = (Color){(unsigned char)GetRandomValue(100, 255),
                      (unsigned char)GetRandomValue(100, 255),
                      (unsigned char)GetRandomValue(100, 255), 255};

    CreateStatic(eng, (Vector3){x, y, z}, (Vector3){width, height, depth}, c);
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

  PopulateGridWithEntities(&gs->grid, gs->compReg, eng);

  PrintGrid(&gs->grid);

  return *gs;
}
