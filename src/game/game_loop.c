#include "components/muzzle.h"
#include "editor.h"
#include "game.h"
#include "rlgl.h"
#include "systems/systems.h"
#include "systems/wave_system.h"
#include "world_spawn.h"
#include <dirent.h>
#include <raylib.h>
#include <string.h>

#define FPS_SAMPLES 120

float fpsSamples[FPS_SAMPLES] = {0};
int fpsIndex = 0;
int fpsCount = 0;
float fpsSum = 0.0f;
float fpsAverage = 0.0f;

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

  if (DrawButton("PLAY", (Vector2){GetScreenWidth() / 2 - 100, 300})) {
    game->gameState = GAMESTATE_LEVELSELECT;
  }

  if (DrawButton("LEVEL EDITOR", (Vector2){GetScreenWidth() / 2 - 100, 380})) {
    game->gameState = GAMESTATE_EDITOR;
  }

  if (DrawButton("SETTINGS", (Vector2){GetScreenWidth() / 2 - 100, 460})) {
    game->gameState = GAMESTATE_SETTINGS;
  }

  if (DrawButton("QUIT", (Vector2){GetScreenWidth() / 2 - 100, 540})) {
    CloseWindow();
  }

  EndDrawing();
}

#define MAX_LEVEL_FILES 64

static char levelPaths[MAX_LEVEL_FILES][256];
static char levelDisplayNames[MAX_LEVEL_FILES][128];
static int  numLevelFiles = 0;
static bool levelListLoaded = false;

static void LoadLevelList(void) {
  numLevelFiles = 0;
  DIR *dir = opendir("assets/levels");
  if (!dir) return;

  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL && numLevelFiles < MAX_LEVEL_FILES) {
    const char *name = entry->d_name;
    int len = (int)strlen(name);
    if (len < 6 || strcmp(name + len - 5, ".json") != 0)
      continue;

    snprintf(levelPaths[numLevelFiles], 256, "assets/levels/%s", name);
    snprintf(levelDisplayNames[numLevelFiles], 128, "%.*s", len - 5, name);
    numLevelFiles++;
  }
  closedir(dir);
  levelListLoaded = true;
}

void RenderLevelSelect(GameWorld *game) {
  if (!levelListLoaded)
    LoadLevelList();

  static int selected = 0;
  if (selected >= numLevelFiles && numLevelFiles > 0)
    selected = numLevelFiles - 1;

  if (numLevelFiles > 0) {
    if (IsKeyPressed(KEY_UP))
      selected = (selected - 1 + numLevelFiles) % numLevelFiles;
    if (IsKeyPressed(KEY_DOWN))
      selected = (selected + 1) % numLevelFiles;

    float scroll = GetMouseWheelMove();
    if (scroll > 0) selected = (selected - 1 + numLevelFiles) % numLevelFiles;
    if (scroll < 0) selected = (selected + 1) % numLevelFiles;
  }

  BeginDrawing();
  ClearBackground(BLACK);
  DrawText("SELECT LEVEL", GetScreenWidth() / 2 - 110, 80, 40, RAYWHITE);

  if (numLevelFiles == 0) {
    DrawText("No levels found in assets/levels/",
             GetScreenWidth() / 2 - 200, 300, 20, GRAY);
  } else {
    int itemW  = 400;
    int itemH  = 60;
    int startX = GetScreenWidth() / 2 - itemW / 2;
    int startY = 200;

    for (int i = 0; i < numLevelFiles; i++) {
      Rectangle r = {startX, startY + i * (itemH + 10), itemW, itemH};
      bool hovered = CheckCollisionPointRec(GetMousePosition(), r);
      if (hovered) selected = i;

      if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        snprintf(game->targetLevelPath, sizeof(game->targetLevelPath),
                 "%s", levelPaths[i]);
        levelListLoaded = false;
        game->gameState = GAMESTATE_LOADING;
      }

      Color bg = (i == selected) ? (Color){50, 80, 180, 255}
                                  : (Color){35, 35, 35, 255};
      DrawRectangleRec(r, bg);
      DrawRectangleLinesEx(r, 2,
          (i == selected) ? SKYBLUE : (Color){70, 70, 70, 255});
      DrawText(levelDisplayNames[i],
               startX + 20, startY + i * (itemH + 10) + 18, 22,
               (i == selected) ? YELLOW : LIGHTGRAY);
    }

    if (IsKeyPressed(KEY_ENTER)) {
      snprintf(game->targetLevelPath, sizeof(game->targetLevelPath),
               "%s", levelPaths[selected]);
      levelListLoaded = false;
      game->gameState = GAMESTATE_LOADING;
    }
  }

  if (IsKeyPressed(KEY_ESCAPE)) {
    levelListLoaded = false;
    game->gameState = GAMESTATE_MAINMENU;
  }

  DrawText("[UP/DOWN/SCROLL] Navigate   [ENTER] Play   [ESC] Back",
           GetScreenWidth() / 2 - 220, GetScreenHeight() - 40, 16, GRAY);
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

  const int spawnBatch = 20000; // spawn per tick

  spawnTimer += dt;
  destroyTimer += dt;

  /* --- Spawn --- */
  if (spawnTimer > 0.2f) {

    for (int i = 0; i < spawnBatch; i++) {

      float x = GetRandomValue(-150, 150);
      float z = GetRandomValue(-150, 150);
      float y = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap, x, z);
      SpawnEnemyGrunt(world, game, (Vector3){x, y, z});
    }

    spawnTimer = 0.0f;
  }

  /* --- Destroy --- */
  if (destroyTimer > 0.2f) {

    archetype_t *arch = WorldGetArchetype(world, game->enemyGruntArchId);

    int toDestroy = spawnBatch;

    for (uint32_t i = 0; i < arch->count && toDestroy > 0; i++) {

      entity_t e = arch->entities[i];
      Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
      active->value = false;

      OnDeath *od = ECS_GET(world, e, OnDeath, COMP_ONDEATH);
      if (od && od->fn)
        od->fn(world, e);
      WorldDestroyEntity(world, e);

      toDestroy--;
    }

    destroyTimer = 0.0f;
  }
}

void RunGameLoop(Engine *engine, GameWorld *game) {
  world_t *world = engine->world;
  Camera3D *camera = &engine->camera;
  static EditorState editorState = {0};

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    switch (game->gameState) {

    case GAMESTATE_MAINMENU:
      RenderMainMenu(game);
      break;

    case GAMESTATE_SETTINGS:
      RenderSettingsMenu(game);
      break;

    case GAMESTATE_LEVELSELECT:
      RenderLevelSelect(game);
      break;

    case GAMESTATE_LOADING: {
      BeginDrawing();
      ClearBackground(BLACK);
      DrawText("LOADING LEVEL...", GetScreenWidth() / 2 - 100,
               GetScreenHeight() / 2, 30, RAYWHITE);
      EndDrawing();

      HeightMap_Free(&game->terrainHeightMap);
      NavGrid_Destroy(&game->navGrid);
      WorldClear(world);

      SpawnLevelFromFile(world, game, game->targetLevelPath);
      WaveSystem_Init(game);

      game->gameState = GAMESTATE_INLEVEL;
      DisableCursor();
      break;
    } break;

    case GAMESTATE_EDITOR: {
      if (!editorState.initialized) {
        EditorInit(&editorState, game, world);
        DisableCursor();
      }
      EditorUpdate(&editorState, game, world);
      EditorRender(&editorState, game);
      if (editorState.requestQuit) {
        editorState.initialized = false;
        editorState.requestQuit = false;
        game->gameState = GAMESTATE_MAINMENU;
        EnableCursor();
      }
    } break;

    case GAMESTATE_INLEVEL: {
      WaveSystem_Update(world, game, dt);
      TimerSystem(&engine->timerPool, dt);

      PlayerControlSystem(world, game, game->player, dt);

      ApplyGravity(world, game, dt);

      PlayerWeaponSystem(world, game, game->player, dt);
      PlayerShootSystem(world, game, game->player);
      PlayerWeaponSwitchSystem(world, game, game->player);

      CollisionSyncSystem(world);

      PlayerMoveAndCollide(world, game, dt);

      // Deliver queued paths before state machines run
      EnemyPathQueue_Flush(NAV_PATHS_PER_FRAME);

      EnemyGruntAISystem(world, game,
                         WorldGetArchetype(world, game->enemyGruntArchId), dt);
      EnemyRangerAISystem(world, game,
                          WorldGetArchetype(world, game->enemyRangerArchId), dt);

      EnemyAimSystem(world, game,
                     WorldGetArchetype(world, game->enemyGruntArchId), dt);
      EnemyRangerAimSystem(world, game,
                           WorldGetArchetype(world, game->enemyRangerArchId), dt);

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

      MovementSystem(world, WorldGetArchetype(world, game->missileArchId), dt);
      HomingMissileSystem(world, game,
                          WorldGetArchetype(world, game->missileArchId), dt);

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
        game->gameState = GAMESTATE_PAUSED;
        EnableCursor();
      }
    } break;

    case GAMESTATE_PAUSED: {
      int sw = GetScreenWidth(), sh = GetScreenHeight();

      BeginDrawing();
      ClearBackground(BLACK);
      DrawText("PAUSED", sw/2 - 55, sh/2 - 150, 40, RAYWHITE);

      if (DrawButton("RESUME", (Vector2){(float)(sw/2 - 100), (float)(sh/2 - 50)})) {
        game->gameState = GAMESTATE_INLEVEL;
        DisableCursor();
      }
      if (DrawButton("MAIN MENU", (Vector2){(float)(sw/2 - 100), (float)(sh/2 + 30)})) {
        game->gameState = GAMESTATE_MAINMENU;
        WorldClear(world);
        EnableCursor();
      }
      DrawText("[ESC] Resume", sw/2 - 60, sh - 40, 16, GRAY);
      EndDrawing();

      // ESC check after EndDrawing so PollInputEvents has refreshed input state,
      // preventing the same keypress that opened the menu from immediately closing it.
      if (game->gameState == GAMESTATE_PAUSED && IsKeyPressed(KEY_ESCAPE)) {
        game->gameState = GAMESTATE_INLEVEL;
        DisableCursor();
      }
    } break;
    }
  }
}
