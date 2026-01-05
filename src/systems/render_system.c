
#include "systems.h"
#include <raylib.h>

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
                                Color tint, bool wireframe, bool drawOutline,
                                Shader outlineShader, float outlineSize,
                                Color outlineColor, int entityId) {
  int numModels = mc->countModels;

  // shader uniform locations (cached static)
  static int locOutlineSize = -1;
  static int locOutlineColor = -1;
  static Shader cachedShader = {0};

  if (drawOutline) {
    // cache uniform locations once per shader instance
    if (cachedShader.id != outlineShader.id) {
      cachedShader = outlineShader;
      locOutlineSize = GetShaderLocation(outlineShader, "outlineSize");
      locOutlineColor = GetShaderLocation(outlineShader, "outlineColor");
    }
  }

  for (int m = 0; m < numModels; m++) {
    if (!mc->isActive[m])
      continue;

    Vector3 drawPos;
    float yaw, pitch, roll;

    // If caller provided global transforms, use them.
    if (mc->globalPositions != NULL && mc->globalOrientations != NULL) {
      drawPos = mc->globalPositions[m];
      Orientation g = mc->globalOrientations[m];
      yaw = g.yaw;
      pitch = -g.pitch;
      roll = g.roll;
    } else {
      // Fallback: compute world transform
      Vector3 localOffset = mc->offsets[m];
      Orientation localRot = mc->orientations[m];
      int parentId = mc->parentIds[m];

      Vector3 parentWorldPos;
      yaw = localRot.yaw;
      pitch = localRot.pitch;
      roll = localRot.roll;

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
      }

      drawPos = Vector3Add(parentWorldPos, localOffset);
    }

    // Build rotation matrix
    Matrix rotMat = MatrixRotateY(yaw);
    rotMat = MatrixMultiply(MatrixRotateX(pitch), rotMat);
    rotMat = MatrixMultiply(MatrixRotateZ(roll), rotMat);

    rlPushMatrix();
    rlTranslatef(drawPos.x, drawPos.y, drawPos.z);
    rlMultMatrixf(MatrixToFloat(rotMat));

    // ---------------------------
    // OUTLINE PASS (inverse hull)
    // ---------------------------
    if (drawOutline &&
        (entityId != 0 || m != 1) && // skip only when entityId==0 AND m==1
        outlineShader.id > 0 && !wireframe) {

      float s = outlineSize;
      Vector4 col = (Vector4){
          outlineColor.r / 255.0f,
          outlineColor.g / 255.0f,
          outlineColor.b / 255.0f,
          outlineColor.a / 255.0f,
      };

      if (locOutlineSize >= 0)
        SetShaderValue(outlineShader, locOutlineSize, &s, SHADER_UNIFORM_FLOAT);
      if (locOutlineColor >= 0)
        SetShaderValue(outlineShader, locOutlineColor, &col,
                       SHADER_UNIFORM_VEC4);

      int matCount = mc->models[m].materialCount;
      Shader saved[16];
      if (matCount > 16)
        matCount = 16;

      for (int k = 0; k < matCount; k++) {
        saved[k] = mc->models[m].materials[k].shader;
        mc->models[m].materials[k].shader = outlineShader;
      }

      rlEnableBackfaceCulling();
      rlSetCullFace(RL_CULL_FACE_FRONT);
      DrawModel(mc->models[m], (Vector3){0, 0, 0}, 1.0f, WHITE);
      rlSetCullFace(RL_CULL_FACE_BACK);

      for (int k = 0; k < matCount; k++) {
        mc->models[m].materials[k].shader = saved[k];
      }
    }

    // ---------------------------
    // NORMAL / WIREFRAME PASS
    // ---------------------------
    if (wireframe) {
      rlSetLineWidth(1.0f);
      DrawModelWires(mc->models[m], (Vector3){0, 0, 0}, 1.0f, tint);
      rlSetLineWidth(1.0f);
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

    Vector3 p = eng->projectiles.positions[i];
    Vector3 v = eng->projectiles.velocities[i];
    int type = eng->projectiles.types[i];

    if (type == 1) {
      // Type 1: yellow sphere
      DrawSphere(p, eng->projectiles.radii[i], YELLOW);
    } else if (type == 2) {
      // Type 2: bigger red sphere
      float r = eng->projectiles.radii[i] * 2.0f;
      DrawSphere(p, r, RED);
    } else if (type == 3) {
      // Type 3: cylinder pointing in movement direction

      // If velocity is near-zero, default forward
      float speed = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
      Vector3 dir = (speed > 0.001f)
                        ? (Vector3){v.x / speed, v.y / speed, v.z / speed}
                        : (Vector3){0, 0, 1};

      // Cylinder dimensions
      float length = 35.0f;
      float radius = 0.5f;

      // Raylib cylinder points from start->end
      Vector3 front = Vector3Add(p, Vector3Scale(dir, length * 0.5f));
      Vector3 back = Vector3Add(p, Vector3Scale(dir, -length * 0.5f));

      DrawCylinderEx(back, front, radius, radius, 8, BLUE);

      // Spawn thruster particle (type 2) at the back
      // Slightly behind the cylinder so it doesn't clip
      Vector3 thrusterPos = Vector3Add(back, Vector3Scale(dir, -2.0f));

    } else if (type == P_MISSILE) {

      float speed = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
      Vector3 dir = (speed > 0.001f)
                        ? (Vector3){v.x / speed, v.y / speed, v.z / speed}
                        : (Vector3){0, 0, 1};

      // Cylinder dimensions
      float length = 35.0f;
      float radius = 0.5f;

      // Raylib cylinder points from start->end
      Vector3 front = Vector3Add(p, Vector3Scale(dir, length * 0.5f));
      Vector3 back = Vector3Add(p, Vector3Scale(dir, -length * 0.5f));

      DrawCylinderEx(back, front, radius, radius, 8, RED);

      // Spawn thruster particle (type 2) at the back
      // Slightly behind the cylinder so it doesn't clip
      Vector3 thrusterPos = Vector3Add(back, Vector3Scale(dir, -2.0f));

    } else {
      // Unknown type fallback
      DrawSphere(p, eng->projectiles.radii[i], WHITE);
    }
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
    return 1.2f; //
  case 5:
    return 150.0f; //
  case 6:
    return 20.0f; //
  case 8:
    return 80.0f; //
  case 9:
    return 25.0f; //
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
      c = YELLOW;
      break;

    case 1: // smoke
      c = (Color){160 * 0.8, 160 * 0.8, 180 * 0.8, 255};
      break;

    case 2: // desert dust
      c = (Color){194, 178, 128, 255};
      break;
    case 5:
      c = (Color){247, 243, 128, 255};
      break;
    case 6:
      c = (Color){30, 13, 9, 255};
      break;
    case 7:
      c = (Color){30, 13, 9, 255};
      break;
    case 8:
      c = (Color){30, 13, 9, 255};
      break;
    case 9:
      c = (Color){15, 6, 6, 255};
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

static void DrawValueBar(int x, int y, int w, int h, float value,
                         float maxValue, Color fillColor, Color backColor,
                         Color borderColor, Color textColor) {
  // clamp
  if (value < 0)
    value = 0;
  if (value > maxValue)
    value = maxValue;

  float t = (maxValue > 0.0f) ? (value / maxValue) : 0.0f;
  int fillW = (int)((w - 2) * t);

  // background + border
  DrawRectangle(x, y, w, h, backColor);
  DrawRectangleLines(x, y, w, h, borderColor);

  // fill
  DrawRectangle(x + 1, y + 1, fillW, h - 2, fillColor);

  // text centered
  char buf[32];
  snprintf(buf, sizeof(buf), "%d/%d", (int)(value + 0.5f),
           (int)(maxValue + 0.5f));

  int fontSize = h - 6;
  if (fontSize < 10)
    fontSize = 10;

  int tw = MeasureText(buf, fontSize);
  int tx = x + (w - tw) / 2;
  int ty = y + (h - fontSize) / 2;

  DrawText(buf, tx, ty, fontSize, textColor);
}

static entity_t FindActiveAlpha(GameState_t *gs, Engine_t *eng) {
  for (int i = 0; i < MAX_POOL_ALPHA; i++) {
    entity_t a = gs->waves.alphaPool[i];
    if (!a)
      continue;

    int idx = GetEntityIndex(a);
    if (!eng->em.alive[idx])
      continue;

    // also ignore parked ones (extra safety)
    Vector3 *pos =
        (Vector3 *)getComponent(&eng->actors, a, gs->compReg.cid_Positions);
    if (pos && pos->y < -5000.0f)
      continue;

    return a;
  }
  return (entity_t)0;
}

static void QuitGameNow(void) {
  // if you have custom shutdown, call it here
  CloseAudioDevice(); // safe even if already closed
  CloseWindow();
  exit(0); // guarantees full shutdown
}

static void DrawPauseOverlay(GameState_t *gs, Engine_t *eng) {
  if (!gs->paused)
    return;

  int w = eng->config.window_width;
  int h = eng->config.window_height;

  // dim screen
  DrawRectangle(0, 0, w, h, (Color){0, 0, 0, 160});

  // panel
  Rectangle panel = {w / 2.0f - 170, h / 2.0f - 150, 340, 300};
  DrawRectangleRec(panel, (Color){20, 20, 20, 220});
  DrawRectangleLinesEx(panel, 2.0f, (Color){255, 255, 255, 120});

  DrawText("PAUSED", (int)panel.x + 110, (int)panel.y + 18, 28, RAYWHITE);

  // buttons
  Rectangle resumeBtn = {panel.x + 70, panel.y + 75, 200, 50};
  Rectangle menuBtn = {panel.x + 70, panel.y + 140, 200, 50};
  Rectangle quitBtn = {panel.x + 70, panel.y + 205, 200, 50};

  Vector2 m = GetMousePosition();
  bool hoverResume = CheckCollisionPointRec(m, resumeBtn);
  bool hoverMenu = CheckCollisionPointRec(m, menuBtn);
  bool hoverQuit = CheckCollisionPointRec(m, quitBtn);

  DrawRectangleRec(resumeBtn, hoverResume ? DARKGRAY : GRAY);
  DrawText("RESUME", (int)resumeBtn.x + 55, (int)resumeBtn.y + 12, 24, WHITE);

  DrawRectangleRec(menuBtn, hoverMenu ? DARKGRAY : GRAY);
  DrawText("MAIN MENU", (int)menuBtn.x + 35, (int)menuBtn.y + 12, 24, WHITE);

  DrawRectangleRec(quitBtn, hoverQuit ? (Color){160, 60, 60, 255}
                                      : (Color){130, 40, 40, 255});
  DrawText("QUIT", (int)quitBtn.x + 75, (int)quitBtn.y + 12, 24, WHITE);

  // click handlers
  if (hoverResume && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    gs->paused = false;
    DisableCursor();
  }

  if (hoverMenu && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    // go back to menu so player can choose Tutorial or Survival again
    gs->paused = false;
    EnableCursor();

    // Optional: clear some transient state so it doesn't carry over
    gs->isZooming = false;
    gs->heatMeter = 30; // or whatever your default is
    gs->pHeadbobTimer = 0.0f;

    // Stop waves immediately (so nothing runs if you accidentally render it)
    gs->waves.state = WAVE_FINISHED;

    gs->state = STATE_MAINMENU;
  }

  if (hoverQuit && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    QuitGameNow();
  }

  DrawText("ESC: Resume", (int)panel.x + 105, (int)panel.y + 270, 18,
           (Color){200, 200, 200, 255});
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

    EntityType_t type = eng->actors.types[i];

    Color outlineColor = {173, 7, 1, 255};
    bool drawOutline = true;
    float outlineThickness = 0.15;
    if (eng->em.alive[i]) {
      outlineColor = (Color){173, 7, 1, 255};
    } else {
      outlineColor = (Color){1, 1, 1, 255};
    }
    if (i == gs->playerId) {
      outlineThickness = 0.05f;
      outlineColor = (Color){1, 1, 1, 255};
    }
    if (type == ENTITY_ENVIRONMENT) {
      outlineColor = BLACK;
      outlineThickness = 15;
    }
    if (type == ENTITY_ROCK) {
      outlineColor = BLACK;
      outlineThickness = 0.05;
    }

    // Visual models (solid white)
    DrawModelCollection(&eng->actors.modelCollections[i], entityPos, WHITE,
                        false, drawOutline, gs->outlineShader, outlineThickness,
                        outlineColor, i);

    // Movement collision boxes (green wireframe)
    DrawModelCollection(&eng->actors.collisionCollections[i], entityPos, GREEN,
                        true, false, gs->outlineShader, outlineThickness,
                        outlineColor, i);

    // Color hitboxColor = RED;
    // if (!eng->em.alive[i]) {
    //   hitboxColor = BLACK;
    // }
    // // Hitboxes (red wireframe)
    // DrawModelCollection(&eng->actors.hitboxCollections[i], entityPos,
    //                     hitboxColor, true, false, gs->outlineShader, 0,
    //                     BLACK, i);
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
                        false, true, gs->outlineShader, 0.6f, BLACK, i);

    // Movement collision boxes (green wireframe)
    DrawModelCollection(&eng->statics.collisionCollections[i], entityPos, GREEN,
                        true, false, gs->outlineShader, 0.0f, BLACK, -1);

    // Color hitboxColor = RED;
    // // Hitboxes (red wireframe)
    // DrawModelCollection(&eng->statics.hitboxCollections[i], entityPos,
    //                     hitboxColor, true, false, gs->outlineShader, 0,
    //                     BLACK, -1);
  }

  EndMode3D();

  // draw UI segment

  DrawFPS(10, 10);

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

  float hitpoints = eng->actors.hitPoints[gs->playerId];
  int playerHeatMeter = gs->heatMeter;

  // --- Bottom HUD bars ---
  int barW = 320;
  int barH = 28;
  int pad = 12;
  int labelSize = 18;

  int yBottom = eng->config.window_height - pad - barH;

  // ---------------------
  // HITPOINTS (left)
  // ---------------------
  int hpX = pad;
  int hpLabelY = yBottom - labelSize - 4;

  DrawText("HITPOINTS", hpX, hpLabelY, labelSize, RAYWHITE);

  DrawValueBar(hpX, yBottom, barW, barH, hitpoints, 200.0f,
               (Color){40, 200, 70, 255},   // fill
               (Color){20, 20, 20, 180},    // back
               (Color){255, 255, 255, 180}, // border
               RAYWHITE);

  // ---------------------
  // HEAT (right)
  // ---------------------
  int heatX = eng->config.window_width - pad - barW;
  int heatLabelY = yBottom - labelSize - 4;

  DrawText("HEAT", heatX, heatLabelY, labelSize, RAYWHITE);

  DrawValueBar(heatX, yBottom, barW, barH, (float)playerHeatMeter, 100.0f,
               (Color){220, 80, 60, 255},   // fill
               (Color){20, 20, 20, 180},    // back
               (Color){255, 255, 255, 180}, // border
               RAYWHITE);

  // ---------------------
  // ALPHA TANK HP (bottom center, large)
  // ---------------------
  entity_t alpha = FindActiveAlpha(gs, eng);
  if (alpha) {
    float alphaHP = eng->actors.hitPoints[alpha];
    float alphaMaxHP = 500.0f;

    // Bigger than player bars
    int alphaBarW = 640;
    int alphaBarH = 36;

    // Position it ABOVE player HP/heat bars
    int alphaPad = 16;
    int alphaY = yBottom - alphaBarH - labelSize - alphaPad - 8;
    int alphaX = (eng->config.window_width - alphaBarW) / 2;

    DrawText("Heavy Tank", alphaX, alphaY - labelSize - 4, labelSize, RAYWHITE);

    DrawValueBar(alphaX, alphaY, alphaBarW, alphaBarH, alphaHP, alphaMaxHP,
                 (Color){180, 60, 220, 255},  // fill
                 (Color){20, 20, 20, 200},    // back
                 (Color){255, 255, 255, 200}, // border
                 RAYWHITE);
  }
  DrawCircleLines(eng->config.window_width / 2, eng->config.window_height / 2,
                  10, RED);

  DrawMessageBanner(gs);
  DrawPauseOverlay(gs, eng);

  // ---------------------
  // Tips panel (bottom center, above bars)
  // ---------------------
  if (gs->tips.visible) {
    const char *tipText = gTips[gs->tips.index];

    int panelW = 520;
    int panelH = 220;

    int panelX = (eng->config.window_width - panelW) / 2;
    int panelY = yBottom - panelH - 90; // tweak vertical placement

    // background panel
    DrawRectangle(panelX, panelY, panelW, panelH, (Color){0, 0, 0, 160});
    DrawRectangleLines(panelX, panelY, panelW, panelH,
                       (Color){255, 255, 255, 120});

    // header
    char header[64];
    snprintf(header, sizeof(header), "TIP %d/%d  (LEFT/RIGHT)",
             gs->tips.index + 1, gTipsCount);
    DrawText(header, panelX + 12, panelY + 10, 18, RAYWHITE);

    // body text
    DrawText(tipText, panelX + 12, panelY + 36, 18, RAYWHITE);
  }

  EndDrawing();
}
