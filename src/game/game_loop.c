
#include "game.h"
#include "systems/systems.h"



void RunGameLoop(Engine *engine, GameWorld *game) {
  world_t *world = engine->world;
  Camera3D *camera = &engine->camera;

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    switch (game->gameState) {

    case GAMESTATE_MAINMENU: {
      if (IsKeyPressed(KEY_ENTER)) {
        game->gameState = GAMESTATE_INLEVEL;
        DisableCursor();
      }

    } break;

    case GAMESTATE_INLEVEL: {

    } break;
    }
  }
}
