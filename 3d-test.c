#include "raylib.h"
#include "raymath.h"
#include "rcamera.h"
#include <math.h>

#define MAX_COLUMNS 20

typedef struct Player_t {
  Camera camera;
  Vector3 velocity;
  float pitch;
  float yaw;
} Player_t;

int main() {

  const int screen_width = 800;
  const int screen_height = 600;

  InitWindow(screen_width, screen_height, "raylib basic 3d test");

  Player_t player = {0};
  player.camera.position = (Vector3){0.0f, 2.0f, 10.0f};
  player.camera.up = (Vector3){0.0f, 1.0f, 0.0f};
  player.camera.fovy = 90.0f;
  player.camera.projection = CAMERA_PERSPECTIVE;
  player.pitch = 0.0f;
  player.yaw = 0.0f;

  float mouse_sensitivity = 0.003f;

  /*
    Generate random columns of random colors and positions
  */
  float heights[MAX_COLUMNS] = {0};
  Vector3 positions[MAX_COLUMNS] = {0};
  Color colors[MAX_COLUMNS] = {0};
  for (int i = 0; i < MAX_COLUMNS; i++) {
    heights[i] = (float)GetRandomValue(1, 12);
    positions[i] = (Vector3){(float)GetRandomValue(-15, 15), heights[i] / 2.0f,
                             (float)GetRandomValue(-15, 15)};
    colors[i] = (Color){(float)GetRandomValue(20, 255),
                        (float)GetRandomValue(10, 55), 30, 255};
  }

  DisableCursor();

  SetTargetFPS(60);

  // ============================
  // MAIN LOOP
  // ============================
  while (!WindowShouldClose()) {

    // ============================
    // Update Loop
    // ===========================

    // CAMERA UPDATE ----------

    float delta = GetFrameTime();

    Vector2 mouse_delta = GetMouseDelta();

    player.yaw -= mouse_delta.x * mouse_sensitivity;
    player.pitch -= mouse_delta.y * mouse_sensitivity;

    // Clamp pitch to avoid flipping
    if (player.pitch > 1.5f)
      player.pitch = 1.5f;
    if (player.pitch < -1.5f)
      player.pitch = -1.5f;

    Vector3 direction = {cosf(player.pitch) * sinf(player.yaw),
                         sinf(player.pitch),
                         cosf(player.pitch) * cosf(player.yaw)};

    player.camera.target = Vector3Add(player.camera.position, direction);

    // MOVEMENT LOGIC ----------
    Vector3 movement = {0};

    const float move_speed = 3.0f * delta;
    float sprint_multiplier = 1.0f;

    if (IsKeyDown(KEY_LEFT_SHIFT)) {
      sprint_multiplier = 2.0f;
      TraceLog(LOG_INFO, "left shift was pressed");
    }

    Vector3 forward = Vector3Normalize((Vector3){direction.x, 0, direction.z});
    Vector3 right = Vector3CrossProduct(forward, (Vector3){0, 1, 0});

    if (IsKeyDown(KEY_W))
      movement = Vector3Add(movement, forward);
    if (IsKeyDown(KEY_S))
      movement = Vector3Subtract(movement, forward);
    if (IsKeyDown(KEY_D))
      movement = Vector3Add(movement, right);
    if (IsKeyDown(KEY_A))
      movement = Vector3Subtract(movement, right);

    if (Vector3Length(movement) > 0) {
      movement = Vector3Normalize(movement);
    }

    movement = Vector3Scale(movement, move_speed * sprint_multiplier);

    player.velocity.y -= 50.0f * delta;

    player.camera.position = Vector3Add(player.camera.position, movement);

    // JUMP LOGIC ----------

    const float GRAVITY = 1.0f;
    const float JUMP_FORCE = 10.0f;
    const float GROUND_LEVEL = 2.0f;
    const float GROUND_THRESHOLD = 0.1f;

    player.velocity.y -= GRAVITY * delta;

    bool isGrounded =
        (player.camera.position.y <= GROUND_LEVEL + GROUND_THRESHOLD);

    if (IsKeyPressed(KEY_SPACE) && isGrounded) {
      player.velocity.y = JUMP_FORCE;
      TraceLog(LOG_INFO, "Jump!");
    }

    player.camera.position.x += movement.x;
    player.camera.position.z += movement.z;
    player.camera.position.y += player.velocity.y * delta;

    if (isGrounded && player.velocity.y <= 0) {
      player.camera.position.y = GROUND_LEVEL;
      player.velocity.y = 0.0f;
    }

    player.camera.target = Vector3Add(player.camera.position, direction);

    // ============================
    // END Update Loop
    // ===========================

    // ============================
    // Render Loop
    // ============================

    BeginDrawing();

    ClearBackground(RAYWHITE);

    BeginMode3D(player.camera);

    DrawPlane((Vector3){0.0f, 0.0f, 0.0f}, (Vector2){32.0f, 32.0f},
              LIGHTGRAY); // Draw ground
    DrawCube((Vector3){-16.0f, 2.5f, 0.0f}, 1.0f, 5.0f, 32.0f,
             BLUE); // Draw a blue wall
    DrawCube((Vector3){16.0f, 2.5f, 0.0f}, 1.0f, 5.0f, 32.0f,
             LIME); // Draw a green wall
    DrawCube((Vector3){0.0f, 2.5f, 16.0f}, 32.0f, 5.0f, 1.0f,
             GOLD); // Draw a yellow wall

    // Draw some cubes around
    for (int i = 0; i < MAX_COLUMNS; i++) {
      DrawCube(positions[i], 2.0f, heights[i], 2.0f, colors[i]);
      DrawCubeWires(positions[i], 2.0f, heights[i], 2.0f, MAROON);
    }

    EndMode3D();

    EndDrawing();

    // ============================
    // END Render Loop
    // ============================
  }

  CloseWindow();

  return 0;
}
