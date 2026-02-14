
#include "game.h"
#include "systems/systems.h"

void UpdateCubesSystem(world_t *world, archetype_t *arch, float dt) {
  float t = GetTime();
#pragma omp parallel for if (arch->count >= OMP_MIN_ITERATIONS)
  for (uint32_t i = 0; i < arch->count; ++i) {
    entity_t e = arch->entities[i];

    Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
    ori->yaw += 1.0f * dt;

    ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);
    ModelInstance_t *gun = &mc->models[1];

    // Oscillate between -PI/4 and +PI/4
    const float amplitude = PI / 4.0f;
    const float speed =
        1.5f - i * 0.01; // radians per second (frequency control)

    gun->rotation.x = sinf(t * speed) * amplitude;
  }
}

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

      RenderMainMenu(game);
    } break;

    case GAMESTATE_INLEVEL: {
      TimerSystem(&engine->timerPool, dt);

      PlayerControlSystem(world, game, game->player, dt);
      PlayerShootSystem(world, game, game->player);
      PlayerWeaponSystem(world, game->player, dt);

      ApplyGravity(world, game, dt);

      UpdateObstacleCollision(world,
                              WorldGetArchetype(world, game->obstacleArchId));

      PlayerMoveAndCollide(world, game, dt);

      BulletSystem(world, WorldGetArchetype(world, game->bulletArchId), dt);

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
    } break;
    }
  }
}
