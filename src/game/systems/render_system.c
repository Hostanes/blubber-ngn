#include "../game.h"
#include "rlgl.h"
#include "systems.h"
#include <raylib.h>

static bitset_t modelMask;
static bool modelMaskInit = false;

static void EnsureModelMask(void) {
  if (modelMaskInit)
    return;

  BitsetInit(&modelMask, 64);
  BitsetSet(&modelMask, COMP_MODEL);

  modelMaskInit = true;
}

static bitset_t activeMask;
static bool activeMaskInit = false;

static void EnsureActiveMask(void) {
  if (activeMaskInit)
    return;

  BitsetInit(&activeMask, 64);
  BitsetSet(&activeMask, COMP_ACTIVE);

  activeMaskInit = true;
}

void DrawNavGridBatched(NavGrid *grid) {
  rlBegin(RL_LINES);

  float half = grid->cellSize * 0.5f;
  float yOffset = 0.7f;

  for (int y = 0; y < grid->height; y++) {
    for (int x = 0; x < grid->width; x++) {
      NavCell *cell = &grid->cells[NavGrid_Index(grid, x, y)];

      Color c = GREEN;
      if (cell->type == NAV_CELL_WALL)
        c = RED;
      else if (cell->type == NAV_CELL_COVER_LOW)
        c = BLUE;

      rlColor4ub(c.r, c.g, c.b, 255);

      Vector3 center = NavGrid_CellCenter(grid, x, y);

      float x0 = center.x - half;
      float x1 = center.x + half;
      float z0 = center.z - half;
      float z1 = center.z + half;
      float yPos = center.y + yOffset;

      // Top edge
      rlVertex3f(x0, yPos, z0);
      rlVertex3f(x1, yPos, z0);

      // Right edge
      rlVertex3f(x1, yPos, z0);
      rlVertex3f(x1, yPos, z1);

      // Bottom edge
      rlVertex3f(x1, yPos, z1);
      rlVertex3f(x0, yPos, z1);

      // Left edge
      rlVertex3f(x0, yPos, z1);
      rlVertex3f(x0, yPos, z0);
    }
  }

  rlEnd();
}

void DrawWallSegmentWireframes(world_t *world, uint32_t wallSegArchId) {
  archetype_t *arch = WorldGetArchetype(world, wallSegArchId);
  if (!arch)
    return;

  for (uint32_t i = 0; i < arch->count; ++i) {
    entity_t e = arch->entities[i];
    WallSegmentCollider *wall =
        ECS_GET(world, e, WallSegmentCollider, COMP_WALL_SEGMENT_COLLIDER);
    if (!wall)
      continue;

    float dx  = wall->worldB.x - wall->worldA.x;
    float dz  = wall->worldB.z - wall->worldA.z;
    float len = sqrtf(dx * dx + dz * dz);

    Vector3 perp = {0, 0, 0};
    if (len > 1e-6f)
      perp = (Vector3){-dz / len * wall->radius, 0.0f, dx / len * wall->radius};

    float yb = wall->yBottom, yt = wall->yTop;
    Vector3 c[8] = {
        {wall->worldA.x + perp.x, yb, wall->worldA.z + perp.z},
        {wall->worldA.x - perp.x, yb, wall->worldA.z - perp.z},
        {wall->worldB.x - perp.x, yb, wall->worldB.z - perp.z},
        {wall->worldB.x + perp.x, yb, wall->worldB.z + perp.z},
        {wall->worldA.x + perp.x, yt, wall->worldA.z + perp.z},
        {wall->worldA.x - perp.x, yt, wall->worldA.z - perp.z},
        {wall->worldB.x - perp.x, yt, wall->worldB.z - perp.z},
        {wall->worldB.x + perp.x, yt, wall->worldB.z + perp.z},
    };

    Color col = ORANGE;
    DrawLine3D(c[0], c[1], col); DrawLine3D(c[1], c[2], col);
    DrawLine3D(c[2], c[3], col); DrawLine3D(c[3], c[0], col);
    DrawLine3D(c[4], c[5], col); DrawLine3D(c[5], c[6], col);
    DrawLine3D(c[6], c[7], col); DrawLine3D(c[7], c[4], col);
    DrawLine3D(c[0], c[4], col); DrawLine3D(c[1], c[5], col);
    DrawLine3D(c[2], c[6], col); DrawLine3D(c[3], c[7], col);
  }
}

void DrawSpawnerWireframes(world_t *world, uint32_t spawnerArchId) {
  archetype_t *arch = WorldGetArchetype(world, spawnerArchId);
  if (!arch) return;

  for (uint32_t i = 0; i < arch->count; i++) {
    entity_t e = arch->entities[i];
    Position     *pos = ECS_GET(world, e, Position,     COMP_POSITION);
    EnemySpawner *sp  = ECS_GET(world, e, EnemySpawner, COMP_ENEMY_SPAWNER);
    if (!pos || !sp) continue;
    Color col = (sp->enemyType == 0) ? RED : BLUE;
    DrawSphereWires(pos->value, 1.5f, 8, 8, col);
    DrawLine3D(Vector3Add(pos->value, (Vector3){-2,0,0}),
               Vector3Add(pos->value, (Vector3){ 2,0,0}), col);
    DrawLine3D(Vector3Add(pos->value, (Vector3){0,0,-2}),
               Vector3Add(pos->value, (Vector3){0,0, 2}), col);
  }
}

void DrawTriggerAABBs(world_t *world, uint32_t triggerArchId) {
  archetype_t *arch = WorldGetArchetype(world, triggerArchId);
  if (!arch)
    return;

  for (uint32_t i = 0; i < arch->count; ++i) {
    entity_t e = arch->entities[i];

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value)
      continue;

    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
    AABBCollider *aabb = ECS_GET(world, e, AABBCollider, COMP_AABB_COLLIDER);

    if (!pos || !aabb)
      continue;

    Vector3 center = pos->value;
    Vector3 size = Vector3Scale(aabb->halfExtents, 2.0f);

    DrawCubeWires(center, size.x, size.y, size.z, PURPLE);
  }
}

void RenderArchetype(world_t *world, archetype_t *arch) {
  bool hasActive = BitsetContainsAll(&arch->mask, &activeMask);
  bool hasAABB = ArchetypeHas(arch, COMP_AABB_COLLIDER);
  bool hasCapsule = ArchetypeHas(arch, COMP_CAPSULE_COLLIDER);
  bool hasSphere = ArchetypeHas(arch, COMP_SPHERE_COLLIDER);

  for (uint32_t i = 0; i < arch->count; ++i) {
    entity_t e = arch->entities[i];

    if (hasActive) {
      Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
      if (!active->value)
        continue;
    }
    // printf("render entity id %u\n", e.id);

    ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);

    // printf("model count %d\n", mc->count);
    for (uint32_t m = 0; m < mc->count; ++m) {
      ModelInstance_t *mi = &mc->models[m];

      if (!mi->isActive)
        continue;

      mi->model.transform = mi->finalTransform;

      DrawModel(mi->model, (Vector3){0, 0, 0}, 1.0f, WHITE);
    }

    // DEBUG continue
    continue;

    // Debug Render Muzzles (if present)
    if (ArchetypeHas(arch, COMP_MUZZLES)) {
      MuzzleCollection_t *muzzles =
          ECS_GET(world, e, MuzzleCollection_t, COMP_MUZZLES);

      if (muzzles) {
        for (int k = 0; k < muzzles->count; ++k) {
          Muzzle_t *m = &muzzles->Muzzles[k];

          DrawSphereWires(m->worldPosition, 0.1f, 4, 6, RED);
        }
      }
    }

    // Debug Collider Rendering
    if (hasAABB) {
      // printf("drawing aabb\n");
      AABBCollider *aabb = ECS_GET(world, e, AABBCollider, COMP_AABB_COLLIDER);

      Position *pos = ECS_GET(world, e, Position, COMP_POSITION);

      Vector3 center = pos->value;
      Vector3 size = Vector3Scale(aabb->halfExtents, 2.0f);

      DrawCubeWires(center, size.x, size.y, size.z, GREEN);
    }

    if (hasSphere) {
      // printf("drawing sphere\n");
      SphereCollider *sphere =
          ECS_GET(world, e, SphereCollider, COMP_SPHERE_COLLIDER);

      Position *pos = ECS_GET(world, e, Position, COMP_POSITION);

      DrawSphereWires(pos->value, sphere->radius, 8, 8, BLUE);
    }

    if (hasCapsule) {
      CapsuleCollider *cap =
          ECS_GET(world, e, CapsuleCollider, COMP_CAPSULE_COLLIDER);

      Vector3 a = cap->worldA;
      Vector3 b = cap->worldB;

      float radius = cap->radius;

      // draw line
      DrawLine3D(a, b, YELLOW);

      // draw spheres at ends
      DrawSphereWires(a, radius, 8, 8, RED);
      DrawSphereWires(b, radius, 8, 8, RED);
    }
  }
}

void ComputeArchetypeTransforms(world_t *world, archetype_t *arch) {
  bool hasActive = BitsetContainsAll(&arch->mask, &activeMask);

#pragma omp parallel for if (arch->count >= OMP_MIN_ITERATIONS)
  for (uint32_t i = 0; i < arch->count; ++i) {
    entity_t e = arch->entities[i];

    if (hasActive) {
      Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
      if (!active->value)
        continue;
    }

    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
    Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
    ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);

    for (uint32_t m = 0; m < mc->count; ++m) {
      ModelInstance_t *mi = &mc->models[m];

      Matrix S = MatrixScale(mi->scale.x, mi->scale.y, mi->scale.z);
      Matrix T_neg_pivot = MatrixTranslate(-mi->pivot.x, -mi->pivot.y, -mi->pivot.z);
      Matrix R_local = MatrixRotateXYZ(mi->rotation);
      Matrix T_pos_pivot = MatrixTranslate(mi->pivot.x, mi->pivot.y, mi->pivot.z);
      Matrix T_local = MatrixTranslate(mi->offset.x, mi->offset.y, mi->offset.z);
      // Rotate around pivot: translate to pivot, rotate, translate back, then apply offset
      Matrix local = MatrixMultiply(S, MatrixMultiply(T_neg_pivot, MatrixMultiply(R_local, MatrixMultiply(T_pos_pivot, T_local))));

      Matrix final;

      if (mi->parentIndex >= 0 && (uint32_t)mi->parentIndex < m) {
        final = MatrixMultiply(local, mc->models[mi->parentIndex].finalTransform);
      } else {
        switch (mi->rotationMode) {
        case MODEL_ROT_WORLD: {
          Matrix T = MatrixTranslate(pos->value.x, pos->value.y, pos->value.z);
          final = MatrixMultiply(local, T);
        } break;

        case MODEL_ROT_YAW_ONLY: {
          Matrix T = MatrixTranslate(pos->value.x, pos->value.y, pos->value.z);
          Matrix R_yaw = MatrixRotateY(ori->yaw);
          final = MatrixMultiply(local, MatrixMultiply(R_yaw, T));
        } break;

        case MODEL_ROT_YAW_PITCH: {
          Matrix T = MatrixTranslate(pos->value.x, pos->value.y, pos->value.z);
          Matrix R_yaw = MatrixRotateY(ori->yaw);
          Matrix R_pitch = MatrixRotateX(-ori->pitch);
          final = MatrixMultiply(local, MatrixMultiply(R_pitch, MatrixMultiply(R_yaw, T)));
        } break;

        case MODEL_ROT_FULL:
        default: {
          Matrix T = MatrixTranslate(pos->value.x, pos->value.y, pos->value.z);
          Matrix R_yaw = MatrixRotateY(ori->yaw);
          final = MatrixMultiply(local, MatrixMultiply(R_yaw, T));
        } break;
        }
      }

      mi->finalTransform = final;
    }
  }
}

static void DrawHealthBars(world_t *world, Camera *camera) {
  const int BAR_W      = 60;
  const int BAR_H      = 5;
  const int Y_OFFSET   = 40; // pixels above projected position

  for (uint32_t i = 0; i < world->archetypeCount; ++i) {
    archetype_t *arch = &world->archetypes[i];

    if (!ArchetypeHas(arch, COMP_HEALTH))  continue;
    if (!ArchetypeHas(arch, COMP_POSITION)) continue;
    if (ArchetypeHas(arch, COMP_TYPE_PLAYER)) continue; // player uses HUD bar

    bool hasShield = ArchetypeHas(arch, COMP_SHIELD);

    for (uint32_t j = 0; j < arch->count; ++j) {
      entity_t e = arch->entities[j];

      Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
      if (!active || !active->value) continue;

      Position *pos   = ECS_GET(world, e, Position, COMP_POSITION);
      Health   *hp    = ECS_GET(world, e, Health,   COMP_HEALTH);
      if (!pos || !hp || hp->max <= 0.0f) continue;

      Shield *sh = hasShield ? ECS_GET(world, e, Shield, COMP_SHIELD) : NULL;

      // Skip entities behind the camera — GetWorldToScreen mirrors them
      Vector3 above     = {pos->value.x, pos->value.y + 2.8f, pos->value.z};
      Vector3 camFwd    = Vector3Normalize(Vector3Subtract(camera->target, camera->position));
      Vector3 toEntity  = Vector3Subtract(above, camera->position);
      if (Vector3DotProduct(camFwd, toEntity) < 0.1f) continue;

      Vector2 screen = GetWorldToScreen(above, *camera);

      if (screen.x < -BAR_W || screen.x > GetScreenWidth() + BAR_W) continue;
      if (screen.y < 0 || screen.y > GetScreenHeight()) continue;

      int bx = (int)screen.x - BAR_W / 2;
      int by = (int)screen.y - Y_OFFSET;

      float shieldMax = (sh && sh->max > 0.0f) ? sh->max : 0.0f;
      float totalMax  = hp->max + shieldMax;

      // Width of each section proportional to its max value
      int shW = (shieldMax > 0.0f) ? (int)(BAR_W * (shieldMax / totalMax)) : 0;
      int hpW = BAR_W - shW;

      // --- Health section (left) ---
      {
        float hpRatio = hp->current / hp->max;
        if (hpRatio < 0.0f) hpRatio = 0.0f;
        if (hpRatio > 1.0f) hpRatio = 1.0f;
        int hpFill = (int)(hpW * hpRatio);

        Color hpColor = hpRatio > 0.5f ? RED
                      : hpRatio > 0.25f ? ORANGE : MAROON;

        DrawRectangle(bx,       by, hpW, BAR_H, (Color){50, 10, 10, 200});
        DrawRectangle(bx,       by, hpFill, BAR_H, hpColor);
        DrawRectangleLines(bx,  by, hpW, BAR_H, (Color){200, 80, 80, 200});
      }

      // --- Shield section (right) ---
      if (shW > 0) {
        float shRatio = (sh->current > 0.0f) ? (sh->current / sh->max) : 0.0f;
        int   shFill  = (int)(shW * shRatio);

        DrawRectangle(bx + hpW,       by, shW, BAR_H, (Color){10, 20, 60, 200});
        DrawRectangle(bx + hpW,       by, shFill, BAR_H, (Color){60, 140, 255, 230});
        DrawRectangleLines(bx + hpW,  by, shW, BAR_H, (Color){80, 160, 255, 200});
      }

      // Outer border encompassing the full bar
      DrawRectangleLines(bx, by, BAR_W, BAR_H, (Color){180, 180, 180, 160});
    }
  }
}

void RenderLevelSystem(world_t *world, GameWorld *game, Camera *camera) {

  EnsureModelMask();

  // ---- PHASE 1: Compute transforms (parallel safe)
  for (uint32_t i = 0; i < world->archetypeCount; ++i) {
    archetype_t *arch = &world->archetypes[i];

    if (!BitsetContainsAll(&arch->mask, &modelMask))
      continue;

    ComputeArchetypeTransforms(world, arch);
  }

  // ---- PHASE 2: Render (main thread only)
  BeginDrawing();
  ClearBackground(SKYBLUE);
  BeginMode3D(*camera);

  DrawModel(game->terrainModel, (Vector3){0, 0, 0}, 1.0f, WHITE);

  for (uint32_t i = 0; i < world->archetypeCount; ++i) {
    archetype_t *arch = &world->archetypes[i];

    if (!ArchetypeHas(arch, COMP_MODEL))
      continue;
    RenderArchetype(world, arch);
  }

  DrawTriggerAABBs(world, game->tutorialBoxArchId);
  DrawWallSegmentWireframes(world, game->wallSegArchId);
  DrawSpawnerWireframes(world, game->spawnerArchId);

  // Draw particles (explosion fire + smoke) with alpha blending
  {
    archetype_t *pArch = WorldGetArchetype(world, game->particleArchId);
    if (pArch) {
      BeginBlendMode(BLEND_ALPHA);
      for (uint32_t i = 0; i < pArch->count; i++) {
        entity_t e = pArch->entities[i];
        Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
        if (!active || !active->value) continue;

        Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
        Particle *p   = ECS_GET(world, e, Particle, COMP_PARTICLE);
        if (!pos || !p) continue;

        float ratio = p->lifetime / p->maxLifetime;
        float r     = p->radius * ratio;
        Color c     = p->color;
        c.a = (uint8_t)(p->color.a * ratio);

        DrawSphere(pos->value, r, c);
      }
      EndBlendMode();
    }
  }

  EndMode3D();
  DrawHealthBars(world, camera);
  DrawFPS(10, 10);

  int screenW = GetScreenWidth();
  int screenH = GetScreenHeight();

  int centerX = screenW / 2;
  int centerY = screenH / 2;

  int size = 6; // half-length of crosshair lines
  int gap = 4;  // space in center
  int thickness = 2;

  // Horizontal left
  DrawLineEx((Vector2){centerX - gap - size, centerY},
             (Vector2){centerX - gap, centerY}, thickness, RED);

  // Horizontal right
  DrawLineEx((Vector2){centerX + gap, centerY},
             (Vector2){centerX + gap + size, centerY}, thickness, RED);

  // Vertical top
  DrawLineEx((Vector2){centerX, centerY - gap - size},
             (Vector2){centerX, centerY - gap}, thickness, RED);

  // Vertical bottom
  DrawLineEx((Vector2){centerX, centerY + gap},
             (Vector2){centerX, centerY + gap + size}, thickness, RED);

  // --- Player debug info (top-right) ---
  archetype_t *playerArch = &world->archetypes[0];
  entity_t player = playerArch->entities[0];

  Position *pos = ECS_GET(world, player, Position, COMP_POSITION);
  Velocity *vel = ECS_GET(world, player, Velocity, COMP_VELOCITY);

  char buf[256];
  snprintf(buf, sizeof(buf),
           "Player Pos:\n"
           "  x: %.2f\n  y: %.2f\n  z: %.2f\n\n"
           "Velocity:\n"
           "  x: %.2f\n  y: %.2f\n  z: %.2f",
           pos->value.x, pos->value.y, pos->value.z, vel->value.x, vel->value.y,
           vel->value.z);

  DrawText(buf, screenW - 260, 10, 16, WHITE);

  // --- Player HP + Shield bar (bottom-left) ---
  Health *hp = ECS_GET(world, player, Health, COMP_HEALTH);
  Shield *sh = ECS_GET(world, player, Shield, COMP_SHIELD);
  if (hp && hp->max > 0.0f) {
    const int margin  = 20;
    const int barH    = 20;
    const int barX    = margin;
    const int barY    = screenH - margin - barH;

    float shMax   = (sh && sh->max > 0.0f) ? sh->max : 0.0f;
    float totalMax = hp->max + shMax;

    // Total bar width scales with total max EHP, capped at 300px
    int barW = (int)(300.0f * (totalMax / (hp->max > 0 ? hp->max : 1)));
    if (barW > 300) barW = 300;
    if (barW < 100) barW = 100;

    int shW = (shMax > 0.0f) ? (int)(barW * (shMax / totalMax)) : 0;
    int hpW = barW - shW;

    // Health section (left)
    {
      float hpRatio = hp->current / hp->max;
      if (hpRatio < 0.0f) hpRatio = 0.0f;
      if (hpRatio > 1.0f) hpRatio = 1.0f;
      int hpFill = (int)(hpW * hpRatio);

      Color fill = hpRatio > 0.5f ? GREEN : (hpRatio > 0.25f ? YELLOW : RED);

      DrawRectangle(barX,      barY, hpW, barH, (Color){30, 10, 10, 200});
      DrawRectangle(barX,      barY, hpFill, barH, fill);
      DrawRectangleLines(barX, barY, hpW, barH, (Color){200, 200, 200, 200});

      char hpBuf[24];
      snprintf(hpBuf, sizeof(hpBuf), "HP %d", (int)hp->current);
      DrawText(hpBuf, barX + 4, barY + 3, 14, WHITE);
    }

    // Shield section (right)
    if (shW > 0 && sh) {
      float shRatio = (sh->current > 0.0f) ? (sh->current / sh->max) : 0.0f;
      int   shFill  = (int)(shW * shRatio);

      DrawRectangle(barX + hpW,       barY, shW, barH, (Color){10, 20, 60, 200});
      DrawRectangle(barX + hpW,       barY, shFill, barH, (Color){60, 140, 255, 230});
      DrawRectangleLines(barX + hpW,  barY, shW, barH, (Color){80, 180, 255, 255});

      char shBuf[24];
      snprintf(shBuf, sizeof(shBuf), "%d", (int)sh->current);
      DrawText(shBuf, barX + hpW + 4, barY + 3, 14, WHITE);
    }

    // Outer border
    DrawRectangleLines(barX, barY, barW, barH, WHITE);
  }

  // Wave HUD
  {
    WaveState *ws = &game->waveState;
    if (ws->allWavesComplete) {
      DrawText("ALL WAVES COMPLETE", screenW/2 - 155, 50, 28, GOLD);
    } else if (ws->currentWave > 0) {
      DrawText(TextFormat("WAVE %d", ws->currentWave), screenW/2 - 55, 50, 32, RAYWHITE);
      if (ws->waveActive) {
        DrawText(TextFormat("Enemies: %d", ws->enemiesAlive), screenW/2 - 55, 90, 20, ORANGE);
      } else {
        int secs = (int)ws->nextWaveTimer + 1;
        DrawText(TextFormat("Next wave in %d", secs), screenW/2 - 90, 90, 20, YELLOW);
      }
    } else {
      int secs = (int)ws->nextWaveTimer + 1;
      DrawText(TextFormat("Wave starts in %d", secs), screenW/2 - 105, 50, 24, YELLOW);
    }
  }

  EndDrawing();
}
