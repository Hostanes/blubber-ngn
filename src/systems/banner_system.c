
#include "systems.h"

void TriggerMessage(GameState_t *gs, const char *msg) {
  strncpy(gs->banner.text, msg, sizeof(gs->banner.text) - 1);
  gs->banner.text[255] = '\0';

  gs->banner.state = BANNER_SLIDE_IN;
  gs->banner.active = true;
  gs->banner.timer = 0.0f;
}

void UpdateMessageBanner(GameState_t *gs, float dt) {
  MessageBanner_t *b = &gs->banner;

  if (!b->active)
    return;

  switch (b->state) {

  case BANNER_SLIDE_IN:
    b->y += b->speed * dt;
    if (b->y >= b->targetY) {
      b->y = b->targetY;
      b->timer = 0.0f;
      b->state = BANNER_VISIBLE;
    }
    break;

  case BANNER_VISIBLE:
    b->timer += dt;
    if (b->timer >= b->visibleTime) {
      b->state = BANNER_SLIDE_OUT;
    }
    break;

  case BANNER_SLIDE_OUT:
    b->y -= b->speed * dt;
    if (b->y <= b->hiddenY) {
      b->y = b->hiddenY;
      b->state = BANNER_HIDDEN;
      b->active = false;
    }
    break;

  case BANNER_HIDDEN:
  default:
    break;
  }
}

void DrawMessageBanner(GameState_t *gs) {
  MessageBanner_t *b = &gs->banner;
  if (!b->active)
    return;

  const int screenWidth = GetScreenWidth();
  const int screenHeight = GetScreenHeight();

  // Banner size = 50% screen width, 80px tall
  int bannerWidth = screenWidth * 0.5f;
  int bannerHeight = 80;

  // Center horizontally
  int bannerX = (screenWidth - bannerWidth) / 2;
  int bannerY = (int)b->y;

  // Background
  DrawRectangle(bannerX, bannerY, bannerWidth, bannerHeight,
                (Color){20, 20, 20, 220});

  // Text centering
  int fontSize = 32;
  int textWidth = MeasureText(b->text, fontSize);
  int textX = bannerX + (bannerWidth - textWidth) / 2;
  int textY = bannerY + (bannerHeight - fontSize) / 2;

  DrawText(b->text, textX, textY, fontSize, RAYWHITE);
}
