#include "systems.h"

void MainMenuSystem(GameState_t *gs, Engine_t *eng) {

  BeginDrawing();
  ClearBackground(BLACK);

  // Simple button rectangle
  Rectangle startButton = {eng->config.window_width / 2.0f - 100,
                           eng->config.window_height / 2.0f - 25, 200, 50};

  // Check if mouse is over the button
  Vector2 mousePos = GetMousePosition();
  bool hovering = CheckCollisionPointRec(mousePos, startButton);

  // Draw the button
  DrawRectangleRec(startButton, hovering ? DARKGRAY : GRAY);
  DrawText("START", startButton.x + 50, startButton.y + 12, 24, WHITE);

  // Check for click
  if (hovering && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    InitGameDuel(eng);
    gs->state = STATE_INLEVEL;
    TriggerMessage(
        gs, "Welcome to the survival mode\nsurvive as many waves as possible");
    DisableCursor();
  }

  EndDrawing();
}
