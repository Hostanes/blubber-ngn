#include "systems.h"

void MainMenuSystem(GameState_t *gs, Engine_t *eng) {

  BeginDrawing();
  ClearBackground(BLACK);

  float cx = eng->config.window_width / 2.0f;
  float cy = eng->config.window_height / 2.0f;

  Rectangle startButton = {cx - 100, cy - 70, 200, 50};
  Rectangle tutorialButton = {cx - 100, cy + 10, 200, 50};

  Vector2 mousePos = GetMousePosition();

  bool hoveringStart = CheckCollisionPointRec(mousePos, startButton);
  bool hoveringTutorial = CheckCollisionPointRec(mousePos, tutorialButton);

  // Draw START
  DrawRectangleRec(startButton, hoveringStart ? DARKGRAY : GRAY);
  DrawText("START", (int)startButton.x + 50, (int)startButton.y + 12, 24,
           WHITE);

  // Draw TUTORIAL
  DrawRectangleRec(tutorialButton, hoveringTutorial ? DARKGRAY : GRAY);
  DrawText("TUTORIAL", (int)tutorialButton.x + 30, (int)tutorialButton.y + 12,
           24, WHITE);

  // Click START => Survival mode (your current behavior)
  if (hoveringStart && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    ResetGameDuel(gs, eng);
    StartGameDuel(gs, eng);
    TriggerMessage(
        gs, "Welcome to the survival mode\nsurvive as many waves as possible");
    DisableCursor();
    gs->state = STATE_INLEVEL;
  }

  // Click TUTORIAL => Tutorial mode
  if (hoveringTutorial && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    StartGameTutorial(gs, eng);
    DisableCursor();
    gs->state = STATE_INLEVEL;
  }

  EndDrawing();
}
