// systems.c
// Implements player input, physics, and rendering

#include "systems.h"
#include "game.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define WORLD_SIZE_X 10
#define WORLD_SIZE_Z 10

typedef enum ChunkType {
  CHUNK_FLAT,
  CHUNK_RAMP_UP,
  CHUNK_RAMP_DOWN,
  // ... maybe CHUNK_RAMP_LEFT, CHUNK_RAMP_RIGHT later
} ChunkType_t;

typedef struct {
  Vector2 gridPos;  // position in chunk grid
  ChunkType_t type; // which mesh to use
  Model model;      // model reference
  Vector3 worldPos; // cached world position for drawing
} Chunk;

typedef struct {
  Model levelChunks[3]; // stores the types of chunks
} Level_t;

Chunk world[WORLD_SIZE_X][WORLD_SIZE_Z];

Level_t *level;

void LoadAssets() {

  Texture2D sandTex = LoadTexture("assets/textures/xtSand.png");

  // Generate flat base meshes
  Mesh flatMesh = GenMeshPlane(50.0f, 50.0f, 1, 1);
  // Mesh rampUpMesh = GenMeshPlane(50.0f, 50.0f, 1, 1);
  // Mesh rampDownMesh = GenMeshPlane(50.0f, 50.0f, 1, 1);

  // Create models
  Model flatModel = LoadModelFromMesh(flatMesh);

  // Apply textures
  flatModel.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = sandTex;

  // Store them
  level = malloc(sizeof(Level_t));
  level->levelChunks[CHUNK_FLAT] = flatModel;

  for (int z = 0; z < WORLD_SIZE_Z; z++) {
    for (int x = 0; x < WORLD_SIZE_X; x++) {

      world[x][z].gridPos = (Vector2){x, z};
      world[x][z].worldPos = (Vector3){x * 50.0f - 250, 0, z * 50.0f - 250};

      world[x][z].type = CHUNK_FLAT;
    }
  }
}

// ---------------- Player Control ----------------

void PlayerControlSystem(GameState_t *gs, SoundSystem_t *soundSys, float dt) {
  int pid = gs->playerId;
  Vector3 *pos = gs->entities.positions;
  Vector3 *vel = gs->entities.velocities;
  Orientation *leg = &gs->entities.modelCollections[pid].orientations[0];
  Orientation *torso = &gs->entities.modelCollections[pid].orientations[1];

  // Rotate legs with A/D
  if (IsKeyDown(KEY_A))
    leg[pid].yaw -= 1.5f * dt;
  if (IsKeyDown(KEY_D))
    leg[pid].yaw += 1.5f * dt;

  // Torso yaw/pitch from mouse
  Vector2 mouse = GetMouseDelta();
  float sensitivity = 0.0005f;
  torso[pid].yaw += mouse.x * sensitivity;
  torso[pid].pitch += -mouse.y * sensitivity;
  // gs->entities.collisionCollections[pid].orientations[pid].yaw =
  // torso[pid].yaw;

  // Clamp torso pitch between -89° and +89°
  if (torso[pid].pitch > 1.2f)
    torso[pid].pitch = 1.2f;
  if (torso[pid].pitch < -1.0f)
    torso[pid].pitch = -1.0f;

  // Movement is based on leg orientation
  float c = cosf(leg[pid].yaw);
  float s = sinf(leg[pid].yaw);
  Vector3 forward = {c, 0, s};
  Vector3 right = {-s, 0, c};

  float totalSpeedMult = 1.0f;
  float forwardSpeedMult = 5.0f * totalSpeedMult;
  float backwardSpeedMult = 5.0f * totalSpeedMult;
  float strafeSpeedMult = 2.0f * totalSpeedMult;

  // Movement keys
  if (IsKeyDown(KEY_W)) {
    vel[pid].x += forward.x * 100.0f * dt * forwardSpeedMult;
    vel[pid].z += forward.z * 100.0f * dt * forwardSpeedMult;
  }
  if (IsKeyDown(KEY_S)) {
    vel[pid].x -= forward.x * 100.0f * dt * backwardSpeedMult;
    vel[pid].z -= forward.z * 100.0f * dt * backwardSpeedMult;
  }
  if (IsKeyDown(KEY_Q)) {
    vel[pid].x += right.x * -100.0f * dt * strafeSpeedMult;
    vel[pid].z += right.z * -100.0f * dt * strafeSpeedMult;
  }
  if (IsKeyDown(KEY_E)) {
    vel[pid].x += right.x * 100.0f * dt * strafeSpeedMult;
    vel[pid].z += right.z * 100.0f * dt * strafeSpeedMult;
  }

  // Step cycle update
  Vector3 velocity = vel[pid];
  float speed = sqrtf(velocity.x * velocity.x + velocity.z * velocity.z);

  if (speed > 1.0f) {
    gs->pHeadbobTimer += dt * 8.0f;
    gs->entities.stepRate[pid] = speed * 0.07;

    float prev = gs->entities.prevStepCycle[pid];
    float curr = gs->entities.stepCycle[pid] + gs->entities.stepRate[pid] * dt;

    if (curr >= 1.0f)
      curr -= 1.0f; // wrap cycle

    if (prev > curr) { // wrapped around -> stomp
      QueueSound(soundSys, SOUND_FOOTSTEP, pos[pid], 0.1f, 1.0f);
    }

    gs->entities.stepCycle[pid] = curr;
    gs->entities.prevStepCycle[pid] = curr;
  } else {
    gs->pHeadbobTimer = 0;
    gs->entities.stepCycle[pid] = 0.0f;
    gs->entities.prevStepCycle[pid] = 0.0f;
  }
}

// ---------------- Physics ----------------

//----------------------------------------
// Terrain Collision: Keep entity on ground
//----------------------------------------

static float GetTerrainHeightAtXZ(Terrain_t *terrain, float xWorld,
                                  float zWorld) {
  float halfWidth = (TERRAIN_SIZE - 1) * TERRAIN_SCALE * 0.5f;

  // Convert world X,Z to grid coordinates
  float localX = (xWorld + halfWidth) / TERRAIN_SCALE;
  float localZ = (zWorld + halfWidth) / TERRAIN_SCALE;

  // Clamp to range
  if (localX < 0)
    localX = 0;
  if (localX > TERRAIN_SIZE - 1)
    localX = TERRAIN_SIZE - 1;
  if (localZ < 0)
    localZ = 0;
  if (localZ > TERRAIN_SIZE - 1)
    localZ = TERRAIN_SIZE - 1;

  int x0 = (int)localX;
  int z0 = (int)localZ;
  int x1 = (x0 + 1 < TERRAIN_SIZE) ? x0 + 1 : x0;
  int z1 = (z0 + 1 < TERRAIN_SIZE) ? z0 + 1 : z0;

  float tx = localX - x0;
  float tz = localZ - z0;

// Access 2D heights array (row-major)
#define H(x, z) terrain->heights[(z) * TERRAIN_SIZE + (x)]

  // Bilinear interpolation between the four corners
  float h00 = H(x0, z0);
  float h10 = H(x1, z0);
  float h01 = H(x0, z1);
  float h11 = H(x1, z1);

  float h0 = h00 * (1.0f - tx) + h10 * tx;
  float h1 = h01 * (1.0f - tx) + h11 * tx;
  float h = h0 * (1.0f - tz) + h1 * tz;

  return h;
}

void ApplyTerrainCollision(GameState_t *gs, int entityId) {
  Vector3 *pos = &gs->entities.positions[entityId];
  Vector3 *vel = &gs->entities.velocities[entityId];

  float terrainY = GetTerrainHeightAtXZ(&gs->terrain, pos->x, pos->z);

  pos->y = terrainY;
  vel->y = 0;
}

//----------------------------------------
// Helpers
//----------------------------------------

// Project OBB corners onto an axis
typedef struct {
  float min, max;
} Projection;

static Projection ProjectOBB(const Vector3 *corners, int numCorners,
                             Vector3 axis, Vector3 center) {
  Projection p = {FLT_MAX, -FLT_MAX};
  for (int i = 0; i < numCorners; i++) {
    Vector3 world = Vector3Add(corners[i], center);
    float dot = Vector3DotProduct(world, axis);
    if (dot < p.min)
      p.min = dot;
    if (dot > p.max)
      p.max = dot;
  }
  return p;
}

// Compute overlap along axis
static float GetOverlap(Projection a, Projection b) {
  return fminf(a.max, b.max) - fmaxf(a.min, b.min);
}

// Get local axis from rotation matrix
static Vector3 OBBGetAxis(const Matrix *rot, int index) {
  switch (index) {
  case 0:
    return (Vector3){rot->m0, rot->m1, rot->m2};
  case 1:
    return (Vector3){rot->m4, rot->m5, rot->m6};
  case 2:
    return (Vector3){rot->m8, rot->m9, rot->m10};
  default:
    return (Vector3){0, 0, 0};
  }
}

//----------------------------------------
// OBB Collision + Resolution
//----------------------------------------

bool CheckAndResolveOBBCollision(Vector3 *aPos, ModelCollection_t *aCC,
                                 Vector3 *bPos, ModelCollection_t *bCC) {
  if (aCC->countModels == 0 || bCC->countModels == 0)
    return false;

  Model aCube = aCC->models[0];
  Model bCube = bCC->models[0];
  Vector3 aOffset = aCC->offsets[0];
  Vector3 bOffset = bCC->offsets[0];

  BoundingBox aBBox = GetMeshBoundingBox(aCube.meshes[0]);
  BoundingBox bBBox = GetMeshBoundingBox(bCube.meshes[0]);

  Vector3 aHalf = Vector3Scale(Vector3Subtract(aBBox.max, aBBox.min), 0.5f);
  Vector3 bHalf = Vector3Scale(Vector3Subtract(bBBox.max, bBBox.min), 0.5f);

  Vector3 aCenter = Vector3Add(*aPos, aOffset);
  Vector3 bCenter = Vector3Add(*bPos, bOffset);

  Matrix aRot = MatrixRotateXYZ((Vector3){aCC->orientations[0].pitch,
                                          aCC->orientations[0].yaw * -1,
                                          aCC->orientations[0].roll});
  Matrix bRot = MatrixRotateXYZ((Vector3){bCC->orientations[0].pitch,
                                          bCC->orientations[0].yaw * -1,
                                          bCC->orientations[0].roll});

  // Generate corners
  Vector3 aCorners[8], bCorners[8];
  for (int i = 0; i < 8; i++) {
    aCorners[i] =
        (Vector3){(i & 1 ? aHalf.x : -aHalf.x), (i & 2 ? aHalf.y : -aHalf.y),
                  (i & 4 ? aHalf.z : -aHalf.z)};
    bCorners[i] =
        (Vector3){(i & 1 ? bHalf.x : -bHalf.x), (i & 2 ? bHalf.y : -bHalf.y),
                  (i & 4 ? bHalf.z : -bHalf.z)};

    aCorners[i] = Vector3Transform(aCorners[i], aRot);
    bCorners[i] = Vector3Transform(bCorners[i], bRot);
  }

  // 15 SAT axes
  Vector3 axes[15];
  int idx = 0;
  for (int i = 0; i < 3; i++)
    axes[idx++] = OBBGetAxis(&aRot, i);
  for (int i = 0; i < 3; i++)
    axes[idx++] = OBBGetAxis(&bRot, i);
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      axes[idx++] =
          Vector3CrossProduct(OBBGetAxis(&aRot, i), OBBGetAxis(&bRot, j));

  float minOverlap = FLT_MAX;
  Vector3 mtvAxis = {0, 0, 0};

  for (int i = 0; i < 15; i++) {
    Vector3 axis = axes[i];
    if (Vector3Length(axis) < 1e-6f)
      continue; // skip degenerate
    axis = Vector3Normalize(axis);

    Projection aProj = ProjectOBB(aCorners, 8, axis, aCenter);
    Projection bProj = ProjectOBB(bCorners, 8, axis, bCenter);

    float overlap = GetOverlap(aProj, bProj);
    if (overlap <= 0.0f)
      return false; // separating axis found

    if (overlap < minOverlap) {
      minOverlap = overlap;
      mtvAxis = axis;
      Vector3 d = Vector3Subtract(bCenter, aCenter);
      if (Vector3DotProduct(d, mtvAxis) < 0)
        mtvAxis = Vector3Scale(mtvAxis, -1.0f);
    }
  }

  // Move A out along MTV
  *aPos = Vector3Subtract(*aPos, Vector3Scale(mtvAxis, minOverlap));
  return true;
}

//----------------------------------------
// Physics System
//----------------------------------------

void PhysicsSystem(GameState_t *gs, float dt) {
  Vector3 *pos = gs->entities.positions;
  Vector3 *vel = gs->entities.velocities;
  int playerId = gs->playerId;

  // Floor clamping
  for (int i = 0; i < gs->entities.count; i++) {
    pos[i] = Vector3Add(pos[i], Vector3Scale(vel[i], dt));
    ApplyTerrainCollision(gs, i);
  }

  // Damping
  for (int i = 0; i < gs->entities.count; i++) {
    vel[i].x *= 0.65f;
    vel[i].z *= 0.65f;
  }

  // Collision
  for (int i = 0; i < gs->entities.count; i++) {
    if (i == playerId)
      continue;

    bool collided = CheckAndResolveOBBCollision(
        &pos[playerId], &gs->entities.collisionCollections[playerId], &pos[i],
        &gs->entities.collisionCollections[i]);

    if (collided) {
      // slide along collision
      Vector3 mtvDir = Vector3Normalize(Vector3Subtract(pos[playerId], pos[i]));
      float velDot = Vector3DotProduct(vel[playerId], mtvDir);
      if (velDot > 0.0f) {
        vel[playerId] =
            Vector3Subtract(vel[playerId], Vector3Scale(mtvDir, velDot));
      }
    }
  }
}

// ---------------- Rendering ----------------

// --- Helper to draw a ModelCollection (solid or wireframe) ---
static void DrawModelCollection(ModelCollection_t *mc, Vector3 entityPos,
                                Color tint, bool wireframe) {
  int numModels = mc->countModels;

  for (int m = 0; m < numModels; m++) {
    Vector3 localOffset = mc->offsets[m];
    Orientation localRot = mc->orientations[m];
    int parentId = mc->parentIds[m];

    Vector3 parentWorldPos;
    float yaw = localRot.yaw;
    float pitch = localRot.pitch;
    float roll = localRot.roll;

    // --- Parent transform inheritance ---
    if (parentId != -1 && parentId < m) {
      Orientation parentRot = mc->orientations[parentId];
      float parentYaw = parentRot.yaw * -1.0f;
      float parentPitch = parentRot.pitch * 1.0f;
      float parentRoll = parentRot.roll * 1.0f;

      float yawLock = mc->rotLocks[m][0] ? 1.0f : 0.0f;
      float pitchLock = mc->rotLocks[m][1] ? 1.0f : 0.0f;
      float rollLock = mc->rotLocks[m][2] ? 1.0f : 0.0f;

      yaw = parentYaw * yawLock + localRot.yaw * (1.0f - yawLock);
      pitch = parentPitch * pitchLock + localRot.pitch * (1.0f - pitchLock);
      roll = parentPitch * rollLock + localRot.roll * (1.0f - rollLock);

      parentWorldPos = Vector3Add(entityPos, mc->offsets[parentId]);
      localOffset = Vector3Transform(localOffset, MatrixRotateY(parentYaw));
    } else {
      parentWorldPos = entityPos;
      yaw *= -1.0f;
    }

    // --- Final world position ---
    Vector3 drawPos = Vector3Add(parentWorldPos, localOffset);

    // --- Build rotation matrix ---
    Matrix rotMat = MatrixRotateY(yaw);
    rotMat = MatrixMultiply(MatrixRotateX(pitch), rotMat);
    rotMat = MatrixMultiply(MatrixRotateZ(roll), rotMat);

    // --- Draw model ---
    rlPushMatrix();
    rlTranslatef(drawPos.x, drawPos.y, drawPos.z);
    rlMultMatrixf(MatrixToFloat(rotMat));

    if (wireframe) {
      rlPushMatrix();
      rlSetLineWidth(3.0f); // <- make lines 3px thick
      DrawModelWires(mc->models[m], (Vector3){0, 0, 0}, 1.0f, tint);
      rlSetLineWidth(1.0f); // <- reset after drawing
      rlPopMatrix();
    } else {
      DrawModel(mc->models[m], (Vector3){0, 0, 0}, 1.0f, tint);
    }
    rlPopMatrix();
  }
}

// --- Main Render Function ---
void RenderSystem(GameState_t *gs, Camera3D camera) {
  BeginDrawing();
  ClearBackground((Color){20, 20, 30, 255});

  BeginMode3D(camera);

  DrawModel(gs->terrain.model, (Vector3){0, 0, 0}, 1.0f, BROWN);

  // --- Draw world terrain/chunks ---
  // for (int z = 0; z < WORLD_SIZE_Z; z++) {
  //   for (int x = 0; x < WORLD_SIZE_X; x++) {
  //     Chunk c = world[x][z];
  //     DrawModel(level->levelChunks[c.type], c.worldPos, 1.0f, WHITE);
  //   }
  // }

  // --- Boundary walls ---
  for (int i = -25; i <= 25; i += 10) {
    DrawCube((Vector3){i, 1, -25}, 2, 2, 2, GRAY);
    DrawCube((Vector3){i, 1, 25}, 2, 2, 2, GRAY);
    DrawCube((Vector3){-25, 1, i}, 2, 2, 2, GRAY);
    DrawCube((Vector3){25, 1, i}, 2, 2, 2, GRAY);
  }

  // --- Draw all entities ---
  for (int i = 0; i < gs->entities.count; i++) {
    Vector3 entityPos = gs->entities.positions[i];

    // Visual models (solid white)
    DrawModelCollection(&gs->entities.modelCollections[i], entityPos, WHITE,
                        false);

    // Movement collision boxes (green wireframe)
    DrawModelCollection(&gs->entities.collisionCollections[i], entityPos, GREEN,
                        true);

    // Hitboxes (red wireframe)
    DrawModelCollection(&gs->entities.hitboxCollections[i], entityPos, RED,
                        true);
  }

  EndMode3D();

  // draw UI segment

  DrawFPS(10, 10);

  // Draw player position
  Vector3 playerPos = gs->entities.positions[gs->playerId];
  char posText[64];
  snprintf(posText, sizeof(posText), "Player Pos: X: %.2f  Y: %.2f  Z: %.2f",
           playerPos.x, playerPos.y, playerPos.z);

  int textWidth = MeasureText(posText, 20);
  DrawText(posText, GetScreenWidth() - textWidth - 10, 10, 20, RAYWHITE);

  // draw torso leg orientation

  Orientation legs_orientation =
      gs->entities.modelCollections[gs->playerId].orientations[0];

  Orientation torso_orientation =
      gs->entities.modelCollections[gs->playerId].orientations[1];

  float legYaw = fmod(legs_orientation.yaw, 2 * PI);
  if (legYaw < 0)
    legYaw += 2 * PI;

  float torsoYaw = fmod(torso_orientation.yaw, 2 * PI);
  if (torsoYaw < 0)
    torsoYaw += 2 * PI;

  // difference
  float diff = fmod(torsoYaw - legYaw + PI, 2 * PI);
  if (diff < 0)
    diff += 2 * PI;
  diff -= PI;

  char rotText[64];
  snprintf(rotText, sizeof(rotText),
           "legs yaw: %f \ntorso yaw: %f \ndiff: %f\n", legYaw, torsoYaw, diff);

  int rotTextWidth = MeasureText(rotText, 20);
  DrawText(rotText, GetScreenWidth() - rotTextWidth - 10, 30, 20, RAYWHITE);

  float length = 50.0f;
  Vector2 arrowStart =
      (Vector2){GetScreenWidth() * 0.8, GetScreenHeight() * 0.8};

  diff += PI / 2.0f;

  float endX = arrowStart.x + cosf(-diff) * length;
  float endY = arrowStart.y + sinf(-diff) * length;

  float endXTorso = arrowStart.x;
  float endYTorso = arrowStart.y - length;

  // torso arrow
  DrawLineEx(arrowStart, (Vector2){endXTorso, endYTorso}, 3.0f, RED);
  // leg arrow
  DrawLineEx(arrowStart, (Vector2){endX, endY}, 3.0f, GREEN);

  // Draw other UI shapes
  DrawCircleV(arrowStart, 10, DARKBLUE);

  EndDrawing();
}
