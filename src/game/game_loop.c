
#include "ecs_get.h"
#include "game.h"
#include "raylib.h"
#include "raymath.h"
#include "systems/systems.h"

void UpdateCubesSystem(world_t *world, archetype_t *arch, float dt) {
#pragma omp parallel for if (arch->count >= OMP_MIN_ITERATIONS)
  for (uint32_t i = 0; i < arch->count; ++i) {
    entity_t e = arch->entities[i];

    Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
    ori->yaw += 1.0f * dt;

    ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);
    ModelInstance_t *gun = &mc->models[1];

    // Oscillate between -PI/4 and +PI/4
    const float amplitude = PI / 4.0f;
    const float speed = 1.5f; // radians per second (frequency control)

    gun->rotation.x = sinf(GetTime() * speed) * amplitude;
  }
}

void RunGameLoop(Engine *engine, GameWorld *game) {
  world_t *world = engine->world;
  Camera3D *camera = &engine->camera;

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    TimerSystem(&engine->timerPool, dt);

    PlayerControlSystem(world, game->player);
    MovementSystem(world, game->playerArch, dt);
    PlayerWeaponSystem(world, game->player);
    UpdateCubesSystem(world, game->boxArch, dt);

    Orientation *ori =
        ECS_GET(world, game->player, Orientation, COMP_ORIENTATION);
    Position *pos = ECS_GET(world, game->player, Position, COMP_POSITION);

    camera->position = pos->value;
    camera->target =
        Vector3Add(pos->value, (Vector3){cosf(ori->pitch) * sinf(ori->yaw),
                                         sinf(ori->pitch),
                                         cosf(ori->pitch) * cosf(ori->yaw)});
    camera->up = (Vector3){0, 1, 0};

    BeginDrawing();
    ClearBackground(SKYBLUE);
    BeginMode3D(*camera);

    RenderSystem(world, game->boxArch);
    RenderSystem(world, game->playerArch);

    EndMode3D();
    DrawFPS(10, 10);
    EndDrawing();
  }
}
