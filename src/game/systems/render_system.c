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

    // continue;

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

  // NavGrid *grid = &game->navGrid;

  // DrawNavGridBatched(grid);

  EndMode3D();
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

  // --- Health bar (bottom-right) ---
  Health *hp = ECS_GET(world, player, Health, COMP_HEALTH);
  if (hp && hp->max > 0.0f) {
    int barW = 220;
    int barH = 20;
    int margin = 20;
    int barX = margin;
    int barY = screenH - margin - barH;

    float ratio = hp->current / hp->max;
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;

    int fillW = (int)(barW * ratio);

    Color fill = ratio > 0.5f ? GREEN : (ratio > 0.25f ? YELLOW : RED);

    DrawRectangle(barX, barY, barW, barH, (Color){30, 10, 10, 200});
    DrawRectangle(barX, barY, fillW, barH, fill);
    DrawRectangleLines(barX, barY, barW, barH, WHITE);

    char hpBuf[32];
    snprintf(hpBuf, sizeof(hpBuf), "HP  %d / %d", (int)hp->current, (int)hp->max);
    DrawText(hpBuf, barX + 6, barY + 3, 14, WHITE);
  }

  EndDrawing();
}
