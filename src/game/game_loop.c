#include "game.h"
#include "systems/systems.h"
#include "world_spawn.h"
#include <raylib.h>

// Helper for UI buttons
bool DrawButton(const char *text, Vector2 pos) {
  Rectangle bounds = {pos.x, pos.y, 200, 50};
  bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);

  DrawRectangleRec(bounds, hovered ? GRAY : LIGHTGRAY);
  DrawText(text, pos.x + 20, pos.y + 15, 20, hovered ? YELLOW : BLACK);

  return hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
}

void RenderMainMenu(GameWorld *game) {
  BeginDrawing();
  ClearBackground(BLACK);

  DrawText("WHALEFALL", GetScreenWidth() / 2 - 150, 150, 40, RAYWHITE);

  // Simple vertical menu
  if (DrawButton("START GAME", (Vector2){GetScreenWidth() / 2 - 100, 300})) {
    game->gameState = GAMESTATE_LOADING;
  }

  if (DrawButton("SETTINGS", (Vector2){GetScreenWidth() / 2 - 100, 380})) {
    game->gameState = GAMESTATE_SETTINGS;
  }

  if (DrawButton("QUIT", (Vector2){GetScreenWidth() / 2 - 100, 460})) {
    CloseWindow();
  }

  EndDrawing();
}

void RenderSettingsMenu(GameWorld *game) {
  BeginDrawing();
  ClearBackground(BLACK);

  DrawText("SETTINGS", GetScreenWidth() / 2 - 80, 100, 40, RAYWHITE);

  // Placeholder settings logic
  DrawText(TextFormat("Target FPS: %d", game->targetFPS), 100, 250, 20,
           RAYWHITE);
  if (IsKeyPressed(KEY_RIGHT))
    game->targetFPS += 30;
  if (IsKeyPressed(KEY_LEFT))
    game->targetFPS -= 30;

  DrawText("Press [ESC] to go back", GetScreenWidth() / 2 - 120, 600, 20, GRAY);

  if (IsKeyPressed(KEY_ESCAPE)) {
    game->gameState = GAMESTATE_MAINMENU;
  }

  EndDrawing();
}

void EnemyBenchmarkSystem(world_t *world, GameWorld *game, float dt) {

  static float spawnTimer = 0.0f;
  static float destroyTimer = 0.0f;

  const int spawnBatch = 50;   // spawn per tick
  const int destroyBatch = 50; // destroy per tick

  spawnTimer += dt;
  destroyTimer += dt;

  /* --- Spawn --- */
  if (spawnTimer > 0.3f) {

    for (int i = 0; i < spawnBatch; i++) {

      float x = GetRandomValue(-150, 150);
      float z = GetRandomValue(-150, 150);

      SpawnEnemyRanger(world, game, (Vector3){x, 0, z});
    }

    spawnTimer = 0.0f;
  }

  /* --- Destroy --- */
  if (destroyTimer > 0.3f) {

    archetype_t *arch = WorldGetArchetype(world, game->enemyRangerArchId);

    int toDestroy = destroyBatch;

    for (uint32_t i = 0; i < arch->count && toDestroy > 0; i++) {

      entity_t e = arch->entities[i];
      Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
      active->value = false;
      WorldDestroyEntity(world, e); // or your destroy function

      toDestroy--;
    }

    destroyTimer = 0.0f;
  }
}

void RunGameLoop(Engine *engine, GameWorld *game) {
  world_t *world = engine->world;
  Camera3D *camera = &engine->camera;

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    switch (game->gameState) {

    case GAMESTATE_MAINMENU:
      RenderMainMenu(game);
      break;

    case GAMESTATE_SETTINGS:
      RenderSettingsMenu(game);
      break;
    case GAMESTATE_LOADING: {
      BeginDrawing();
      ClearBackground(BLACK);
      DrawText("LOADING LEVEL...", GetScreenWidth() / 2 - 100,
               GetScreenHeight() / 2, 30, RAYWHITE);
      EndDrawing();

      WorldClear(world);
      SpawnLevel01(world, game);
      game->gameState = GAMESTATE_INLEVEL;
      DisableCursor();
      break;
    } break;

    case GAMESTATE_INLEVEL: {

      TimerSystem(&engine->timerPool, dt);

      EnemyBenchmarkSystem(world, game, dt);

      PlayerControlSystem(world, game, game->player, dt);

      ApplyGravity(world, game, dt);

      PlayerWeaponSystem(world, game->player, dt);
      PlayerShootSystem(world, game, game->player);
      PlayerWeaponSwitchSystem(world, game, game->player);

      CollisionSyncSystem(world);

      PlayerMoveAndCollide(world, game, dt);

      // EnemyGruntAISystem(world, game,
      //                    WorldGetArchetype(world, game->enemyGruntArchId),
      //                    dt);

      // EnemyRangerAISystem(
      //     world, game, WorldGetArchetype(world, game->enemyRangerArchId),
      //     dt);

      // EnemyAISystem(world, game,
      //               WorldGetArchetype(world, game->enemyGruntArchId), dt);
      // EnemyAISystem(world, game,
      //               WorldGetArchetype(world, game->enemyRangerArchId), dt);

      EnemyRangerAimSystem(
          world, game, WorldGetArchetype(world, game->enemyRangerArchId), dt);

      EnemyRangerFireSystem(
          world, game, WorldGetArchetype(world, game->enemyRangerArchId), dt);

      EnemyFireSystem(world, game,
                      WorldGetArchetype(world, game->enemyGruntArchId));

      MovementSystem(world, WorldGetArchetype(world, game->enemyGruntArchId),
                     dt);
      MovementSystem(world, WorldGetArchetype(world, game->enemyRangerArchId),
                     dt);

      BulletSystem(world, game, WorldGetArchetype(world, game->bulletArchId),
                   dt);

      Orientation *ori =
          ECS_GET(world, game->player, Orientation, COMP_ORIENTATION);
      Position *pos = ECS_GET(world, game->player, Position, COMP_POSITION);

      camera->position = pos->value;
      // camera->position.y += PLAYER_HEIGHT;
      camera->target =
          Vector3Add(camera->position, (Vector3){
                                           cosf(ori->pitch) * sinf(ori->yaw),
                                           sinf(ori->pitch),
                                           cosf(ori->pitch) * cosf(ori->yaw),
                                       });
      camera->up = (Vector3){0, 1, 0};

      RenderLevelSystem(world, game, camera);

      if (IsKeyPressed(KEY_ESCAPE)) {
        game->gameState = GAMESTATE_MAINMENU;
        WorldClear(world);
        EnableCursor();
      }
    } break;
    }
  }
}
