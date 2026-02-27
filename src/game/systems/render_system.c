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

void RenderMainMenu(GameWorld *game) {
  BeginDrawing();
  ClearBackground(DARKGRAY);

  const char *title = "FPS test game";
  const char *prompt = "Press ENTER to start";

  int screenW = GetScreenWidth();
  int screenH = GetScreenHeight();

  DrawText(title, screenW / 2 - MeasureText(title, 40) / 2, screenH / 2 - 60,
           40, RAYWHITE);

  DrawText(prompt, screenW / 2 - MeasureText(prompt, 20) / 2, screenH / 2 + 10,
           20, LIGHTGRAY);

  EndDrawing();
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

    ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);

    for (uint32_t m = 0; m < mc->count; ++m) {
      ModelInstance_t *mi = &mc->models[m];

      if (!mi->isActive)
        continue;

      mi->model.transform = mi->finalTransform;

      DrawModel(mi->model, (Vector3){0, 0, 0}, 1.0f, WHITE);
    }

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

    continue;

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

    Matrix T_world = MatrixTranslate(pos->value.x, pos->value.y, pos->value.z);
    Matrix R_entityYaw = MatrixRotateY(ori->yaw);
    Matrix entityTransform = MatrixMultiply(R_entityYaw, T_world);

    for (uint32_t m = 0; m < mc->count; ++m) {
      ModelInstance_t *mi = &mc->models[m];

      // Base local transform (scale + local rotation + offset)
      Matrix S = MatrixScale(mi->scale.x, mi->scale.y, mi->scale.z);
      Matrix R_local = MatrixRotateXYZ(mi->rotation);
      Matrix T_local =
          MatrixTranslate(mi->offset.x, mi->offset.y, mi->offset.z);

      Matrix local = MatrixMultiply(S, MatrixMultiply(R_local, T_local));

      Matrix final;

      switch (mi->rotationMode) {
      case MODEL_ROT_WORLD: {
        // Only position, no entity rotation
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

        final = MatrixMultiply(
            local, MatrixMultiply(R_pitch, MatrixMultiply(R_yaw, T)));
      } break;

      case MODEL_ROT_FULL:
      default: {
        Matrix T = MatrixTranslate(pos->value.x, pos->value.y, pos->value.z);

        Matrix R_yaw = MatrixRotateY(ori->yaw);

        final = MatrixMultiply(local, MatrixMultiply(R_yaw, T));
      } break;
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

    if (!BitsetContainsAll(&arch->mask, &modelMask))
      continue;
    RenderArchetype(world, arch);
  }

  DrawTriggerAABBs(world, game->tutorialBoxArchId);

  NavGrid *grid = &game->navGrid;

  DrawNavGridBatched(grid);

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

  EndDrawing();
}
