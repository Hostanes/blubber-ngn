#include "systems.h"

static void QuitGameNow(void) {
  // if you have custom shutdown, call it here
  CloseAudioDevice(); // safe even if already closed
  CloseWindow();
  exit(0); // guarantees full shutdown
}

static bool ButtonDraw(Rectangle r, const char *label, int fontSize,
                       bool hover) {
  DrawRectangleRec(r, hover ? DARKGRAY : GRAY);

  int tw = MeasureText(label, fontSize);
  DrawText(label, (int)(r.x + (r.width - tw) / 2),
           (int)(r.y + (r.height - fontSize) / 2), fontSize, WHITE);
  return hover;
}

void MainMenuSystem(GameState_t *gs, Engine_t *eng) {
  gs->paused = false;
  gs->isZooming = false;

  BeginDrawing();
  ClearBackground(BLACK);

  float w = (float)eng->config.window_width;
  float h = (float)eng->config.window_height;

  const char *title = "MECH ARENA";
  int titleSize = 52;
  int titleW = MeasureText(title, titleSize);
  DrawText(title, (int)(w / 2 - titleW / 2), (int)(h * 0.18f), titleSize,
           RAYWHITE);

  const char *subtitle = "Survival waves or tutorial range";
  int subSize = 18;
  int subW = MeasureText(subtitle, subSize);
  DrawText(subtitle, (int)(w / 2 - subW / 2), (int)(h * 0.18f + 60), subSize,
           (Color){200, 200, 200, 255});

  float btnW = 280;
  float btnH = 56;
  float gap = 14;

  float bx = w / 2.0f - btnW / 2.0f;
  float by = h * 0.40f;

  Rectangle startBtn = (Rectangle){bx, by + 0 * (btnH + gap), btnW, btnH};
  Rectangle tutorialBtn = (Rectangle){bx, by + 1 * (btnH + gap), btnW, btnH};
  Rectangle quitBtn = (Rectangle){bx, by + 2 * (btnH + gap), btnW, btnH};

  Vector2 m = GetMousePosition();
  bool hoverStart = CheckCollisionPointRec(m, startBtn);
  bool hoverTut = CheckCollisionPointRec(m, tutorialBtn);
  bool hoverQuit = CheckCollisionPointRec(m, quitBtn);

  // draw
  ButtonDraw(startBtn, "START (SURVIVAL)", 22, hoverStart);
  ButtonDraw(tutorialBtn, "TUTORIAL", 22, hoverTut);
  ButtonDraw(quitBtn, "QUIT", 22, hoverQuit);

  // handle click ONCE
  bool clicked = IsMouseButtonPressed(MOUSE_LEFT_BUTTON);
  if (clicked) {
    if (hoverStart) {
      ResetGameDuel(gs, eng);
      StartGameDuel(gs, eng);
      TriggerMessage(
          gs, "Welcome to survival mode\nsurvive as many waves as possible");
      DisableCursor();
      gs->state = STATE_INLEVEL;
    } else if (hoverTut) {
      StartGameTutorial(gs, eng);
      TriggerMessage(gs, "Welcome to the shooting range");
      DisableCursor();
      gs->state = STATE_INLEVEL;
    } else if (hoverQuit) {
      QuitGameNow();
    }
  }

  const char *hint = "ESC pauses in-game";
  int hintW = MeasureText(hint, 18);
  DrawText(hint, (int)(w / 2 - hintW / 2), (int)(h * 0.88f), 18,
           (Color){180, 180, 180, 255});

  EndDrawing();
}
