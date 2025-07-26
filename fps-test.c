#include "raylib.h"
#include "raymath.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_COLUMNS 20

// ============================
// STRUCT DEFINITIONS
// ============================

typedef struct Player_t {
  Camera camera;
  Vector3 velocity;
  float pitch;
  float yaw;
} Player_t;

typedef struct Column_t {
  Vector3 position;
  float height;
  Color color;
} Column_t;

typedef struct World_t {
  Vector3 ground_size;
  Vector3 wall_positions[3];
  Vector3 wall_sizes[3];
  Color wall_colors[3];
  Column_t columns[MAX_COLUMNS];
  int column_count;
} World_t;

// ============================
// MAIN
// ============================

int main() {

  const int screen_width = 800;
  const int screen_height = 600;

  InitWindow(screen_width, screen_height, "raylib 3D world with movement");

  SetTargetFPS(60);
  DisableCursor(); 

  // ----------------------------
  // Initialize Player
  // ----------------------------

  Player_t player = {0};
  player.camera.position = (Vector3){0.0f, 2.0f, 10.0f};
  player.camera.up = (Vector3){0.0f, 1.0f, 0.0f};
  player.camera.fovy = 90.0f;
  player.camera.projection = CAMERA_PERSPECTIVE;
  player.pitch = 0.0f;
  player.yaw = 0.0f;
  player.velocity = (Vector3){0};

  float mouse_sensitivity = 0.003f;

  // ----------------------------
  // Initialize World
  // ----------------------------

  World_t world = {0};
  world.ground_size = (Vector3){32.0f, 0.5f, 32.0f};

  // Walls: Left, Right, Back
  world.wall_sizes[0] = (Vector3){1.0f, 5.0f, 32.0f}; // Left
  world.wall_positions[0] = (Vector3){-16.0f, 2.5f, 0.0f};
  world.wall_colors[0] = BLUE;

  world.wall_sizes[1] = (Vector3){1.0f, 5.0f, 32.0f}; // Right
  world.wall_positions[1] = (Vector3){16.0f, 2.5f, 0.0f};
  world.wall_colors[1] = LIME;

  world.wall_sizes[2] = (Vector3){32.0f, 5.0f, 1.0f}; // Back
  world.wall_positions[2] = (Vector3){0.0f, 2.5f, -16.0f};
  world.wall_colors[2] = GOLD;

  // Columns
  world.column_count = MAX_COLUMNS;
  for (int i = 0; i < MAX_COLUMNS; i++) {
    float height = (float)GetRandomValue(1, 12);
    world.columns[i].height = height;
    world.columns[i].position =
        (Vector3){(float)GetRandomValue(-14, 14), height / 2.0f,
                  (float)GetRandomValue(-14, 14)};
    world.columns[i].color =
        (Color){(unsigned char)GetRandomValue(50, 255),
                (unsigned char)GetRandomValue(50, 255),
                (unsigned char)GetRandomValue(50, 255), 255};
  }

  // ============================
  // MAIN LOOP
  // ============================

  while (!WindowShouldClose()) {
    float delta = GetFrameTime();

    // ----------------------------
    // CAMERA & INPUT UPDATE
    // ----------------------------

    Vector2 mouse_delta = GetMouseDelta();
    player.yaw -= mouse_delta.x * mouse_sensitivity;
    player.pitch -= mouse_delta.y * mouse_sensitivity;

    // Clamp pitch to prevent flipping
    player.pitch = Clamp(player.pitch, -1.5f, 1.5f);

    // Calculate direction vector
    Vector3 direction = {cosf(player.pitch) * sinf(player.yaw),
                         sinf(player.pitch),
                         cosf(player.pitch) * cosf(player.yaw)};

    Vector3 forward = Vector3Normalize((Vector3){direction.x, 0, direction.z});
    Vector3 right =
        Vector3Normalize(Vector3CrossProduct(forward, (Vector3){0, 1, 0}));

    // Movement Input
    Vector3 movement = {0};
    const float move_speed = 3.0f;
    float speed = move_speed;

    if (IsKeyDown(KEY_LEFT_SHIFT)) {
      speed *= 2.0f;
    }

    if (IsKeyDown(KEY_W))
      movement = Vector3Add(movement, forward);
    if (IsKeyDown(KEY_S))
      movement = Vector3Subtract(movement, forward);
    if (IsKeyDown(KEY_D))
      movement = Vector3Add(movement, right);
    if (IsKeyDown(KEY_A))
      movement = Vector3Subtract(movement, right);

    if (Vector3Length(movement) > 0.0f) {
      movement = Vector3Normalize(movement);
      movement = Vector3Scale(movement, speed * delta);
    }

    player.camera.position = Vector3Add(player.camera.position, movement);

    // Gravity & Jumping
    const float GRAVITY = 20.0f;
    const float JUMP_FORCE = 8.0f;
    const float GROUND_LEVEL = 2.0f;

    player.velocity.y -= GRAVITY * delta;

    bool isGrounded = (player.camera.position.y <= GROUND_LEVEL + 0.1f);

    if (IsKeyPressed(KEY_SPACE) && isGrounded) {
      player.velocity.y = JUMP_FORCE;
    }

    player.camera.position.y += player.velocity.y * delta;

    if (isGrounded && player.velocity.y < 0) {
      player.camera.position.y = GROUND_LEVEL;
      player.velocity.y = 0;
    }

    player.camera.target = Vector3Add(player.camera.position, direction);

    // ----------------------------
    // RENDER
    // ----------------------------

    BeginDrawing();
    ClearBackground(RAYWHITE);
    BeginMode3D(player.camera);

    // Draw Ground
    DrawPlane((Vector3){0.0f, 0.0f, 0.0f},
              (Vector2){world.ground_size.x, world.ground_size.z}, LIGHTGRAY);

    // Draw Walls
    for (int i = 0; i < 3; i++) {
      DrawCube(world.wall_positions[i], world.wall_sizes[i].x,
               world.wall_sizes[i].y, world.wall_sizes[i].z,
               world.wall_colors[i]);

      DrawCubeWires(world.wall_positions[i], world.wall_sizes[i].x,
                    world.wall_sizes[i].y, world.wall_sizes[i].z, BLACK);
    }

    // Draw Columns
    for (int i = 0; i < world.column_count; i++) {
      Column_t col = world.columns[i];
      DrawCube(col.position, 1.0f, col.height, 1.0f, col.color);
      DrawCubeWires(col.position, 1.0f, col.height, 1.0f, MAROON);
    }

    EndMode3D();

    DrawText("Move: WASD | Jump: Space | Look: Mouse | Sprint: Shift", 10, 10,
             20, DARKGREEN);

    EndDrawing();
  }

  CloseWindow();
  return 0;
}
