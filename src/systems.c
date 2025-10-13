// systems.c
// Implements player input, physics, and rendering

#include "systems.h"
#include <float.h>
#include "game.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <math.h>
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
  gs->entities.collisionCollections[pid].orientations[pid].yaw = torso[pid].yaw;

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
      QueueSound(soundSys, SOUND_FOOTSTEP, pos[pid], 1.0f, 1.0f);
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

static Vector3 OBBGetAxis(const Matrix *rot, int index) {
  // returns the X/Y/Z axis of the rotation matrix
  switch (index) {
  case 0:
    return (Vector3){rot->m0, rot->m1, rot->m2}; // X axis
  case 1:
    return (Vector3){rot->m4, rot->m5, rot->m6}; // Y axis
  case 2:
    return (Vector3){rot->m8, rot->m9, rot->m10}; // Z axis
  default:
    return (Vector3){0, 0, 0};
  }
}

// Project vector onto axis
static float ProjectOBB(const Vector3 *cornerOffsets, int numCorners,
                        Vector3 axis, Vector3 center) {
  float min = FLT_MAX, max = -FLT_MAX;
  for (int i = 0; i < numCorners; i++) {
    Vector3 p = Vector3Add(cornerOffsets[i], center);
    float proj = Vector3DotProduct(p, axis);
    if (proj < min)
      min = proj;
    if (proj > max)
      max = proj;
  }
  return max - min; // length along axis
}

// Separating Axis Test for two OBBs
static bool CheckOBBCollision(Vector3 aPos, ModelCollection_t *aCC,
                              Vector3 bPos, ModelCollection_t *bCC) {
  if (aCC->countModels == 0 || bCC->countModels == 0)
    return false;

  Model aCube = aCC->models[0];
  Model bCube = bCC->models[0];
  Vector3 aOffset = aCC->offsets[0];
  Vector3 bOffset = bCC->offsets[0];

  BoundingBox aBBox = GetMeshBoundingBox(aCube.meshes[0]);
  BoundingBox bBBox = GetMeshBoundingBox(bCube.meshes[0]);

  Vector3 aHalf = {(aBBox.max.x - aBBox.min.x) * 0.5f,
                   (aBBox.max.y - aBBox.min.y) * 0.5f,
                   (aBBox.max.z - aBBox.min.z) * 0.5f};
  Vector3 bHalf = {(bBBox.max.x - bBBox.min.x) * 0.5f,
                   (bBBox.max.y - bBBox.min.y) * 0.5f,
                   (bBBox.max.z - bBBox.min.z) * 0.5f};

  Vector3 aCenter = Vector3Add(aPos, aOffset);
  Vector3 bCenter = Vector3Add(bPos, bOffset);

  Matrix aRot = MatrixRotateXYZ((Vector3){aCC->orientations[0].pitch,
                                          aCC->orientations[0].yaw,
                                          aCC->orientations[0].roll});
  Matrix bRot = MatrixRotateXYZ((Vector3){bCC->orientations[0].pitch,
                                          bCC->orientations[0].yaw,
                                          bCC->orientations[0].roll});

  // Generate the 8 corners of each box in local space
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

  // Separating axes: 3 axes of each box
  Vector3 axes[6];
  for (int i = 0; i < 3; i++) {
    axes[i] = OBBGetAxis(&aRot, i);
    axes[i + 3] = OBBGetAxis(&bRot, i);
  }

  // Test all axes
  for (int i = 0; i < 6; i++) {
    float aProj = ProjectOBB(aCorners, 8, axes[i], aCenter);
    float bProj = ProjectOBB(bCorners, 8, axes[i], bCenter);

    Vector3 delta = Vector3Subtract(bCenter, aCenter);
    float dist = fabsf(Vector3DotProduct(delta, axes[i]));
    if (dist > (aProj + bProj) * 0.5f) {
      // Separating axis found
      return false;
    }
  }

  return true; // no separating axis -> collision
}

// ---------------- Physics System ----------------

void PhysicsSystem(GameState_t *gs, float dt) {
  Vector3 *pos = gs->entities.positions;
  Vector3 *vel = gs->entities.velocities;
  int playerId = gs->playerId;

  // ---------------- Movement ----------------
  for (int i = 0; i < gs->entities.count; i++) {
    pos[i] = Vector3Add(pos[i], Vector3Scale(vel[i], dt));
    vel[i].x *= 0.65f;
    vel[i].z *= 0.65f;
  }

  // ---------------- Collision ----------------
  for (int i = 0; i < gs->entities.count; i++) {
    if (i == playerId)
      continue;

    if (CheckOBBCollision(pos[playerId],
                          &gs->entities.collisionCollections[playerId], pos[i],
                          &gs->entities.collisionCollections[i])) {
      // Simple push-out: move player opposite to velocity
      Vector3 dir = Vector3Normalize(Vector3Subtract(pos[playerId], pos[i]));
      pos[playerId] = Vector3Add(pos[playerId], Vector3Scale(dir, 0.1f));
      vel[playerId].x = vel[playerId].z = 0;
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

  // --- Draw world terrain/chunks ---
  for (int z = 0; z < WORLD_SIZE_Z; z++) {
    for (int x = 0; x < WORLD_SIZE_X; x++) {
      Chunk c = world[x][z];
      DrawModel(level->levelChunks[c.type], c.worldPos, 1.0f, WHITE);
    }
  }

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

  DrawFPS(10, 10);
  EndDrawing();
}
