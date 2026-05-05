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

// Small arrow button: "<" or ">"
static bool DrawArrowButton(const char *label, int x, int y) {
  Rectangle r = {(float)x, (float)y, 36, 36};
  bool hov = CheckCollisionPointRec(GetMousePosition(), r);
  DrawRectangleRec(r, hov ? GRAY : LIGHTGRAY);
  DrawText(label, x + 10, y + 8, 20, hov ? YELLOW : BLACK);
  return hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
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
    game->settingsPrevState = GAMESTATE_MAINMENU;
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

static const int s_resW[] = {1280, 1600, 1920, 2560};
static const int s_resH[] = {720,  900,  1080, 1440};
#define NUM_RES 4

static int CurrentResIndex(GameWorld *game) {
  for (int i = 0; i < NUM_RES; i++)
    if (s_resW[i] == game->resWidth && s_resH[i] == game->resHeight)
      return i;
  return 0;
}

void RenderSettingsMenu(GameWorld *game, Camera3D *camera) {
  int sw = GetScreenWidth(), sh = GetScreenHeight();
  int cx = sw / 2;

  BeginDrawing();
  ClearBackground((Color){10, 10, 18, 255});

  DrawText("SETTINGS", cx - MeasureText("SETTINGS", 40) / 2, 60, 40, RAYWHITE);

  int rowY   = 180;
  int rowGap = 80;
  int labelX = cx - 300;
  int valX   = cx - 60;
  int arrowL = cx - 110;
  int arrowR = cx + 80;

  // ---- FOV ----
  DrawText("FIELD OF VIEW", labelX, rowY + 8, 20, LIGHTGRAY);
  if (DrawArrowButton("<", arrowL, rowY)) {
    game->fov -= 5.0f;
    if (game->fov < 45.0f) game->fov = 45.0f;
    camera->fovy = game->fov;
  }
  DrawText(TextFormat("%.0f", game->fov), valX, rowY + 8, 20, RAYWHITE);
  if (DrawArrowButton(">", arrowR, rowY)) {
    game->fov += 5.0f;
    if (game->fov > 120.0f) game->fov = 120.0f;
    camera->fovy = game->fov;
  }
  rowY += rowGap;

  // ---- Fullscreen / Windowed ----
  DrawText("DISPLAY MODE", labelX, rowY + 8, 20, LIGHTGRAY);
  {
    const char *modeLabel = game->fullscreen ? "FULLSCREEN" : "WINDOWED";
    Rectangle r = {(float)(cx - 110), (float)rowY, 220, 40};
    bool hov = CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, hov ? GRAY : LIGHTGRAY);
    int tw = MeasureText(modeLabel, 20);
    DrawText(modeLabel, (int)(r.x + r.width / 2 - tw / 2), rowY + 10, 20,
             hov ? YELLOW : BLACK);
    if (hov && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
      game->fullscreen = !game->fullscreen;
      ToggleFullscreen();
    }
  }
  rowY += rowGap;

  // ---- Resolution (windowed only) ----
  Color resColor = game->fullscreen ? DARKGRAY : LIGHTGRAY;
  DrawText("RESOLUTION", labelX, rowY + 8, 20, resColor);
  {
    int ri = CurrentResIndex(game);
    if (!game->fullscreen && DrawArrowButton("<", arrowL, rowY)) {
      ri = (ri - 1 + NUM_RES) % NUM_RES;
      game->resWidth  = s_resW[ri];
      game->resHeight = s_resH[ri];
      SetWindowSize(game->resWidth, game->resHeight);
    }
    const char *resLabel = TextFormat("%dx%d", s_resW[ri], s_resH[ri]);
    DrawText(resLabel, valX - 20, rowY + 8, 20, game->fullscreen ? DARKGRAY : RAYWHITE);
    if (!game->fullscreen && DrawArrowButton(">", arrowR, rowY)) {
      ri = (ri + 1) % NUM_RES;
      game->resWidth  = s_resW[ri];
      game->resHeight = s_resH[ri];
      SetWindowSize(game->resWidth, game->resHeight);
    }
  }
  rowY += rowGap + 20;

  // ---- Back ----
  if (DrawButton("BACK", (Vector2){(float)(cx - 100), (float)rowY}) ||
      IsKeyPressed(KEY_ESCAPE)) {
    game->gameState = game->settingsPrevState;
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
      RenderSettingsMenu(game, camera);
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

      EnemyPathQueue_Reset();
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
      camera->fovy = game->fov;
      UpdateSoundSystem(&game->soundSystem, world, game, dt);
      WaveSystem_Update(world, game, dt);
      InfoBoxTriggerSystem(world, game);
      MessageSystem_Update(&game->messageSystem, dt);
      TimerSystem(&engine->timerPool, dt);

      PlayerControlSystem(world, game, game->player, dt);

      ApplyGravity(world, game, dt);

      PlayerWeaponSystem(world, game, game->player, dt);
      PlayerShootSystem(world, game, game->player, dt);
      PlayerWeaponSwitchSystem(world, game, game->player);

      CollisionSyncSystem(world);

      PlayerMoveAndCollide(world, game, dt);

      // Deliver queued paths before state machines run
      EnemyPathQueue_Flush(world, NAV_PATHS_PER_FRAME);

      EnemyGruntAISystem(world, game,
                         WorldGetArchetype(world, game->enemyGruntArchId), dt);
      EnemyRangerAISystem(world, game,
                          WorldGetArchetype(world, game->enemyRangerArchId), dt);
      EnemyMeleeAISystem(world, game,
                         WorldGetArchetype(world, game->enemyMeleeArchId), dt);

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
      MovementSystem(world, WorldGetArchetype(world, game->enemyMeleeArchId),
                     dt);

      BulletSystem(world, game, WorldGetArchetype(world, game->bulletArchId),
                   dt);

      ParticleSystem(world, WorldGetArchetype(world, game->particleArchId), dt);

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

      if (DrawButton("RESUME", (Vector2){(float)(sw/2 - 100), (float)(sh/2 - 80)})) {
        game->gameState = GAMESTATE_INLEVEL;
        DisableCursor();
      }
      if (DrawButton("SETTINGS", (Vector2){(float)(sw/2 - 100), (float)(sh/2)})) {
        game->settingsPrevState = GAMESTATE_PAUSED;
        game->gameState = GAMESTATE_SETTINGS;
      }
      if (DrawButton("MAIN MENU", (Vector2){(float)(sw/2 - 100), (float)(sh/2 + 80)})) {
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
