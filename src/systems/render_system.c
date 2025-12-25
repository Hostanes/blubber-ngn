
#include "systems.h"

#define BOB_AMOUNT 0.5f

// --- Update world-space transforms for a ModelCollection ---
static void UpdateModelCollectionWorldTransforms(ModelCollection_t *mc,
                                                 Vector3 entityPos,
                                                 Vector3 cameraTarget,
                                                 int entityType) {
  for (int m = 0; m < mc->countModels; m++) {
    Vector3 localOffset = mc->offsets[m];
    Orientation localRot = mc->orientations[m];
    int parentId = mc->parentIds[m];

    Vector3 parentWorldPos;
    float yaw = localRot.yaw;
    float pitch = localRot.pitch;
    float roll = localRot.roll;

    if (parentId != -1 && parentId < m) {
      Orientation parentRot = mc->globalOrientations[parentId];
      float parentYaw = parentRot.yaw * 1.0f;
      float parentPitch = parentRot.pitch * 1.0f;
      float parentRoll = parentRot.roll * 1.0f;

      float yawLock = mc->rotLocks[m][0] ? 1.0f : 0.0f;
      float pitchLock = mc->rotLocks[m][1] ? 1.0f : 0.0f;
      float rollLock = mc->rotLocks[m][2] ? 1.0f : 0.0f;

      yaw = parentYaw * yawLock + localRot.yaw * (1.0f - yawLock);
      pitch = parentPitch * pitchLock + localRot.pitch * (1.0f - pitchLock);
      roll = parentPitch * rollLock + localRot.roll * (1.0f - rollLock);

      parentWorldPos = mc->globalPositions[parentId];
      localOffset = Vector3Transform(localOffset, MatrixRotateY(parentYaw));
    } else {
      parentWorldPos = entityPos;
      yaw *= -1.0f;
    }
    // --- Apply local rotation offset normally ---
    yaw += mc->localRotationOffset[m].yaw;
    pitch += mc->localRotationOffset[m].pitch;
    roll += mc->localRotationOffset[m].roll;

    // --- Apply per-axis inversion ---
    if (mc->rotInverts[m][0])
      yaw *= -1.0f;
    if (mc->rotInverts[m][1])
      pitch *= -1.0f;
    if (mc->rotInverts[m][2])
      roll *= -1.0f;

    mc->globalPositions[m] = Vector3Add(parentWorldPos, localOffset);
    mc->globalOrientations[m] = (Orientation){yaw, pitch, roll};
  }
}

// --- Helper to draw a ModelCollection (solid or wireframe) ---
// Uses precomputed globalPositions/globalOrientations if available.
// Otherwise, computes the same local->world transform as before.
static void DrawModelCollection(ModelCollection_t *mc, Vector3 entityPos,
                                Color tint, bool wireframe) {
  int numModels = mc->countModels;
  for (int m = 0; m < numModels; m++) {
    Vector3 drawPos;
    float yaw, pitch, roll;

    // If the caller has provided precomputed global transforms, use them.
    if (mc->globalPositions != NULL && mc->globalOrientations != NULL) {
      drawPos = mc->globalPositions[m];
      Orientation g = mc->globalOrientations[m];
      yaw = g.yaw;
      pitch = -g.pitch;
      roll = g.roll;
    } else {
      // Fallback: compute world transform exactly like previous code.
      Vector3 localOffset = mc->offsets[m];
      Orientation localRot = mc->orientations[m];
      int parentId = mc->parentIds[m];

      Vector3 parentWorldPos;
      yaw = localRot.yaw;
      pitch = localRot.pitch;
      roll = localRot.roll;

      // --- Parent transform inheritance (same rules as before) ---
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
        // yaw *= -1.0f;
      }

      // --- Final world position ---
      drawPos = Vector3Add(parentWorldPos, localOffset);
    }

    // --- Build rotation matrix (same order as before) ---
    Matrix rotMat = MatrixRotateY(yaw);
    rotMat = MatrixMultiply(MatrixRotateX(pitch), rotMat);
    rotMat = MatrixMultiply(MatrixRotateZ(roll), rotMat);

    if (!mc->isActive[m]) {
      continue;
    }

    // --- Draw model using world transform ---
    rlPushMatrix();
    rlTranslatef(drawPos.x, drawPos.y, drawPos.z);
    rlMultMatrixf(MatrixToFloat(rotMat));

    if (wireframe) {
      rlPushMatrix();
      rlSetLineWidth(1.0f); // make lines 3px thick
      DrawModelWires(mc->models[m], (Vector3){0, 0, 0}, 1.0f, tint);
      rlSetLineWidth(1.0f); // reset after drawing
      rlPopMatrix();
    } else {
      DrawModel(mc->models[m], (Vector3){0, 0, 0}, 1.0f, tint);
    }
    rlPopMatrix();
  }
}

void DrawProjectiles(Engine_t *eng) {
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    if (!eng->projectiles.active[i])
      continue;

    DrawSphere(eng->projectiles.positions[i], eng->projectiles.radii[i],
               YELLOW);
  }
}

float ParticleBaseSize(int type) {
  switch (type) {
  case 0:
    return 5.0f; //
  case 1:
    return 2.0f; //
  case 2:
    return 2.5f; //
  case 3:
    return 2.0f; //
  case 4:
    return 3.0f; //
  default:
    return 1.0f;
  }
}

void DrawParticles(ParticlePool_t *pp) {
  for (int i = 0; i < MAX_PARTICLES; i++) {

    if (!pp->active[i])
      continue;

    int type = pp->types[i];
    Vector3 pos = pp->positions[i];
    float life = pp->lifetimes[i];
    float startLife = pp->startLifetimes[i];

    if (startLife <= 0.0f)
      continue; // safety

    // t = remaining life fraction (1 → 0)
    float t = life / startLife;

    // Size shrinks linearly
    float baseSize = ParticleBaseSize(type);
    float size = baseSize * t; // size goes from baseSize → 0

    // Fade out linearly
    float alpha = t / 2; // 1 → 0

    Color c = WHITE;
    switch (type) {
    case 0: // default
      c = WHITE;
      break;

    case 1: // smoke
      c = (Color){160 * 0.8, 160 * 0.8, 180 * 0.8, 255};
      break;

    case 2: // desert dust
      c = (Color){194, 178, 128, 255};
      break;
    }

    c.a = (unsigned char)(alpha * 255);

    // Render
    DrawSphereEx(pos, size, 8, 8, c);
  }
}

void DrawRaycasts(GameState_t *gs, Engine_t *eng) {
  for (int i = 0; i < eng->em.count; i++) {
    if (!eng->em.alive[i])
      continue; // skip dead entities
    if (!(eng->em.masks[i] & C_RAYCAST))
      continue; // skip entities without raycast

    for (int j = 0; j < eng->actors.rayCounts[i]; j++) {

      Raycast_t *raycast = &eng->actors.raycasts[i][j];

      // Color for debug: player = RED, others = BLUE
      Color c = (i == gs->playerId) ? RED : BLUE;

      DrawRay(raycast->ray, c);
    }
  }
}

// --- Main Render Function ---
void RenderSystem(GameState_t *gs, Engine_t *eng, Camera3D camera) {

  const float bobAmount = BOB_AMOUNT; // height in meters, visual only

  int pid = gs->playerId;
  Vector3 *playerPos = (Vector3 *)getComponent(&eng->actors, gs->playerId,
                                               gs->compReg.cid_Positions);
  ModelCollection_t *mc = &eng->actors.modelCollections[pid];

  // --- Compute headbob ---
  float t = eng->actors.stepCycle[pid];
  float bobTri = (t < 0.5f) ? (t * 2.0f) : (2.0f - t * 2.0f); // 0->1->0
  bobTri = 1.0f - bobTri;                                     // flip to drop
  float torsoBobY = bobTri * bobAmount;

  // --- Update torso position with bob ---
  Vector3 torsoPos = *playerPos;
  torsoPos.y += 10.0f + torsoBobY; // base height + bob

  // Update torso model collection transforms
  UpdateModelCollectionWorldTransforms(mc, torsoPos, camera.target,
                                       eng->actors.types[pid]);

  // --- Compute forward from torso orientation ---
  Orientation torsoOri = mc->globalOrientations[1]; // torso model index

  // Rotate yaw 90° left relative to torso
  float camYaw = torsoOri.yaw;
  float camPitch = torsoOri.pitch;

  // Convert yaw/pitch to forward vector correctly
  Vector3 forward;
  forward.x = sinf(camYaw) * cosf(camPitch);
  forward.y = sinf(camPitch);
  forward.z = cosf(camYaw) * cosf(camPitch);

  // --- Small camera offset/jitter around torso ---
  float camBobX = 0;
  // ((float)GetRandomValue(-100, 100) / 1000.0f); // small X jitter
  float camBobZ = 0;
  // ((float)GetRandomValue(-100, 100) / 1000.0f); // small Z jitter
  float camBobY = torsoBobY * 0.0f; // reduced vertical bob

  // Apply small camera bob
  Vector3 eye = {torsoPos.x + camBobX, torsoPos.y + camBobY,
                 torsoPos.z + camBobZ};

  camera.position = eye;
  camera.target = Vector3Add(eye, forward);

  Matrix proj = MatrixPerspective(
      camera.fovy * DEG2RAD,
      (float)eng->config.window_width / (float)eng->config.window_height,
      eng->config.near_plane, eng->config.far_plane);

  BeginDrawing();
  ClearBackground((Color){20, 20, 30, 255});

  BeginMode3D(camera);

  rlSetMatrixProjection(proj);

  DrawModel(gs->terrain.model, (Vector3){0, 0, 0}, 1.0f, BROWN);

  // --- Draw world terrain/chunks ---
  // for (int z = 0; z < WORLD_SIZE_Z; z++) {
  //   for (int x = 0; x < WORLD_SIZE_X; x++) {
  //     Chunk c = world[x][z];
  //     DrawModel(level->levelChunks[c.type], c.worldPos, 1.0f, WHITE);
  //   }
  // }

  // --- Draw all entities ---
  DrawProjectiles(eng);

  DrawParticles(&eng->particles);

  for (int i = 0; i < eng->em.count; i++) {
    Vector3 entityPos =
        *(Vector3 *)getComponent(&eng->actors, i, gs->compReg.cid_Positions);

    // Update world transforms
    UpdateModelCollectionWorldTransforms(&eng->actors.modelCollections[i],
                                         entityPos, camera.target,
                                         eng->actors.types[i]);
    UpdateModelCollectionWorldTransforms(&eng->actors.collisionCollections[i],
                                         entityPos, camera.target, 0);
    UpdateModelCollectionWorldTransforms(&eng->actors.hitboxCollections[i],
                                         entityPos, camera.target, 0);

    // DrawRaycasts(gs, eng);

    // Visual models (solid white)
    DrawModelCollection(&eng->actors.modelCollections[i], entityPos, WHITE,
                        false);

    // Movement collision boxes (green wireframe)
    DrawModelCollection(&eng->actors.collisionCollections[i], entityPos, GREEN,
                        true);

    Color hitboxColor = RED;
    if (!eng->em.alive[i]) {
      hitboxColor = BLACK;
    }
    // Hitboxes (red wireframe)
    DrawModelCollection(&eng->actors.hitboxCollections[i], entityPos,
                        hitboxColor, true);
  }

  for (int i = 0; i < MAX_STATICS; i++) {
    if (eng->statics.modelCollections[i].countModels == 0)
      continue; // ✅ skip unused static entries
    Vector3 entityPos = eng->statics.positions[i];
    // TODO replaced types with 0, currently types arent used for anything so
    // keep in mind for later ig

    // Update world transforms
    UpdateModelCollectionWorldTransforms(&eng->statics.modelCollections[i],
                                         entityPos, camera.target, 0);
    UpdateModelCollectionWorldTransforms(&eng->statics.collisionCollections[i],
                                         entityPos, camera.target, 0);
    UpdateModelCollectionWorldTransforms(&eng->statics.hitboxCollections[i],
                                         entityPos, camera.target, 0);

    // Visual models (solid white)
    DrawModelCollection(&eng->statics.modelCollections[i], entityPos, WHITE,
                        false);

    // Movement collision boxes (green wireframe)
    DrawModelCollection(&eng->statics.collisionCollections[i], entityPos, GREEN,
                        true);

    Color hitboxColor = RED;
    // Hitboxes (red wireframe)
    DrawModelCollection(&eng->statics.hitboxCollections[i], entityPos,
                        hitboxColor, true);
  }

  EndMode3D();

  // draw UI segment

  DrawFPS(10, 10);

  // Prepare debug string
  char debugOri[128];
  snprintf(debugOri, sizeof(debugOri),
           "Torso Yaw: %.2f  Pitch: %.2f  Roll: %.2f\n"
           "Camera Yaw: %.2f  Pitch: %.2f\n Convergence distance %f",
           torsoOri.yaw, torsoOri.pitch, torsoOri.roll, camYaw, camPitch,
           eng->actors.raycasts[pid][0].distance);

  // Draw text at top-left
  DrawText(debugOri, 10, 40, 20, RAYWHITE);

  // Draw player position
  char posText[64];
  snprintf(posText, sizeof(posText), "Player Pos: X: %.2f  Y: %.2f  Z: %.2f",
           playerPos->x, playerPos->y, playerPos->z);

  int textWidth = MeasureText(posText, 20);
  DrawText(posText, eng->config.window_width - textWidth - 10, 10, 20,
           RAYWHITE);

  // draw torso leg orientation
  Orientation legs_orientation =
      eng->actors.modelCollections[gs->playerId].orientations[0];

  Orientation torso_orientation =
      eng->actors.modelCollections[gs->playerId].orientations[1];

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

  char rotText[64];
  snprintf(rotText, sizeof(rotText),
           "legs yaw: %f \ntorso yaw: %f \ndiff: %f\n", legYaw, torsoYaw, diff);

  int rotTextWidth = MeasureText(rotText, 20);
  DrawText(rotText, eng->config.window_width - rotTextWidth - 10, 30, 20,
           RAYWHITE);

  float length = 50.0f;
  Vector2 arrowStart = (Vector2){eng->config.window_width * 0.8,
                                 eng->config.window_height * 0.8};

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

  DrawCircleLines(eng->config.window_width / 2, eng->config.window_height / 2,
                  10, RED);

  DrawMessageBanner(gs);

  EndDrawing();
}
