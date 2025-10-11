// systems.c
// Implements player input, physics, and rendering

#include "systems.h"
#include "game.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
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
  Orientation *leg = &gs->entities.modelCollections[0].orientations[0];
  Orientation *torso = &gs->entities.modelCollections[0].orientations[1];

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

void PhysicsSystem(GameState_t *gs, float dt) {
  Vector3 *pos = gs->entities.positions;
  Vector3 *vel = gs->entities.velocities;

  for (int i = 0; i < gs->entities.count; i++) {
    pos[i].x += vel[i].x * dt;
    pos[i].y += vel[i].y * dt;
    pos[i].z += vel[i].z * dt;

    // simple drag
    vel[i].x *= 0.65f;
    vel[i].z *= 0.65f;
  }
}

// ---------------- Rendering ----------------

void RenderSystem(GameState_t *gs, Camera3D camera) {
  BeginDrawing();
  ClearBackground((Color){20, 20, 30, 255});

  BeginMode3D(camera);

  // ground
  // DrawModel(ground, (Vector3){0, 0, 0}, 1, WHITE);

  for (int z = 0; z < WORLD_SIZE_Z; z++) {
    for (int x = 0; x < WORLD_SIZE_X; x++) {
      Chunk c = world[x][z];
      // printf("%f, %f, %f\n", c.worldPos.x, c.worldPos.y, c.worldPos.z);
      DrawModel(level->levelChunks[c.type], c.worldPos, 1.0f, WHITE);
    }
  }

  // DrawModel(terrain, (Vector3){0, 0, 0}, 1.0f, WHITE);

  // walls
  for (int i = -25; i <= 25; i += 10) {
    DrawCube((Vector3){i, 1, -25}, 2, 2, 2, GRAY);
    DrawCube((Vector3){i, 1, 25}, 2, 2, 2, GRAY);
    DrawCube((Vector3){-25, 1, i}, 2, 2, 2, GRAY);
    DrawCube((Vector3){25, 1, i}, 2, 2, 2, GRAY);
  }

  for (int i = 0; i < gs->entities.count; i++) {
    Vector3 entityPos = gs->entities.positions[i];
    ModelCollection_t *mc = &gs->entities.modelCollections[i];
    int numModels = mc->countModels;

    for (int m = 0; m < numModels; m++) {
      Vector3 localOffset = mc->offsets[m]; // Offset in local space (relative
                                            // to parent if parented)
      Orientation localRot = mc->orientations[m];
      int parentId = mc->parentIds[m];

      Vector3 parentWorldPos;
      float yaw = localRot.yaw; // local rotation
      float pitch = localRot.pitch;
      float roll = localRot.roll;

      // Compute parent world position if parented
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

        // Parent world position
        parentWorldPos = Vector3Add(entityPos, mc->offsets[parentId]);

        // Rotate child offset relative to parent yaw
        localOffset = Vector3Transform(localOffset, MatrixRotateY(parentYaw));
      } else {
        parentWorldPos = entityPos;
        yaw *= -1.0f;
      }

      // Final world position of the model
      Vector3 drawPos = Vector3Add(parentWorldPos, localOffset);

      // Build the model rotation matrix (local pitch/roll + inherited yaw)
      Matrix rotMat = MatrixRotateY(yaw);
      rotMat = MatrixMultiply(MatrixRotateX(pitch), rotMat);
      rotMat = MatrixMultiply(MatrixRotateZ(roll), rotMat);

      // Draw model using rlgl with full matrix
      rlPushMatrix();
      rlTranslatef(drawPos.x, drawPos.y, drawPos.z);
      rlMultMatrixf(MatrixToFloat(rotMat));
      DrawModel(mc->models[m], (Vector3){0, 0, 0}, 1.0f, WHITE);
      rlPopMatrix();
    }
  }

  // // player arrow
  // int pid = gs->playerId;
  // Vector3 pos = gs->entities.positions[pid];
  // float yaw = gs->entities.legOrientation[pid].yaw;
  // Vector3 forward = {cosf(yaw), 0, sinf(yaw)};

  // // Cylinder
  // // Vector3 base = {pos.x, pos.y + 0.8f, pos.z};
  // // Vector3 shaftEnd = {base.x + forward.x * 0.95f, base.y,
  // //                     base.z + forward.z * 0.95f};
  // // DrawCylinderEx(base, shaftEnd, 0.1f, 0.1f, 8, BLUE);
  // // Vector3 headEnd = {shaftEnd.x + forward.x * 0.55f, shaftEnd.y,
  // //                    shaftEnd.z + forward.z * 0.55f};
  // // DrawCylinderEx(shaftEnd, headEnd, 0.25f, 0.0f, 8, RED);

  // Matrix legTransform = MatrixRotateY(gs->entities.legOrientation[pid].yaw);
  // legTransform = MatrixMultiply(
  //     MatrixRotateX(gs->entities.legOrientation[pid].pitch), legTransform);
  // legTransform = MatrixMultiply(
  //     MatrixRotateZ(gs->entities.legOrientation[pid].roll), legTransform);

  // // Translate to player position
  // legTransform =
  //     MatrixMultiply(MatrixTranslate(pos.x, pos.y, pos.z), legTransform);

  // DrawModelEx(mechLeg, pos, (Vector3){0, 1, 0},
  //             gs->entities.legOrientation[pid].yaw * RAD2DEG * -1,
  //             (Vector3){1, 1, 1}, WHITE);
  // // DrawModelWiresEx(mechLeg, pos, (Vector3){0, 1, 0},
  // //                  gs->entities.legOrientation[pid].yaw * RAD2DEG * -1,
  // //                  (Vector3){1, 1, 1}, GREEN);

  // DrawCircle3D(pos, 3.0f, (Vector3){1, 0, 0}, 90.0f, (Color){0, 0, 0, 100});

  EndMode3D();

  DrawFPS(10, 10);
  EndDrawing();
}
