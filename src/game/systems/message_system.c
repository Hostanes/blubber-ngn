#include "message_system.h"
#include "raylib.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define PANEL_W          640
#define PANEL_PAD        16
#define PANEL_TEXT_W     (PANEL_W - PANEL_PAD * 2)
#define HEADER_FONT      17
#define BODY_FONT        15
#define LINE_H           (BODY_FONT + 6)
#define TYPEWRITER_SPD   60.0f
#define SLIDE_SPD        1400.0f
#define TARGET_Y         30.0f
#define OFFSCREEN_Y      -800.0f

static const Color C_BG      = {5,   10,  22,  210};
static const Color C_BORDER  = {0,   210, 190, 255};
static const Color C_CORNER  = {0,   255, 230, 255};
static const Color C_HEADER  = {0,   255, 230, 255};
static const Color C_SEP     = {0,   80,  70,  180};
static const Color C_BODY    = {100, 255, 170, 255};
static const Color C_CURSOR  = {0,   255, 230, 255};
static const Color C_SCAN    = {0,   0,   0,   36};
static const Color C_BAR     = {0,   200, 180, 255};
static const Color C_BARBASE = {0,   40,  36,  255};

static int BuildLines(const char *text, char lines[][MSG_TEXT_LEN], int maxLines) {
  int lineIdx = 0;
  lines[0][0] = '\0';
  int lineLen  = 0;
  const char *p = text;

  while (*p && lineIdx < maxLines) {
    const char *wStart = p;
    while (*p && *p != ' ') p++;
    int wLen = (int)(p - wStart);
    if (*p == ' ') p++;
    if (wLen == 0) continue;

    char candidate[MSG_TEXT_LEN];
    if (lineLen > 0)
      snprintf(candidate, sizeof(candidate), "%s %.*s", lines[lineIdx], wLen, wStart);
    else
      snprintf(candidate, sizeof(candidate), "%.*s", wLen, wStart);

    if (MeasureText(candidate, BODY_FONT) > PANEL_TEXT_W && lineLen > 0) {
      lineIdx++;
      if (lineIdx >= maxLines) break;
      snprintf(lines[lineIdx], MSG_TEXT_LEN, "%.*s", wLen, wStart);
      lineLen = wLen;
    } else {
      strncpy(lines[lineIdx], candidate, MSG_TEXT_LEN - 1);
      lines[lineIdx][MSG_TEXT_LEN - 1] = '\0';
      lineLen = (int)strlen(lines[lineIdx]);
    }
  }
  return lineIdx + 1;
}

static int PanelH(int lineCount) {
  // top_pad + header + gap + separator + gap + lines + gap + bar + bot_pad
  return 12 + (HEADER_FONT + 4) + 4 + 1 + 10 + lineCount * LINE_H + 10 + 6 + 12;
}

void MessageSystem_Init(MessageSystem_t *ms) {
  memset(ms, 0, sizeof(*ms));
  ms->slideY = OFFSCREEN_Y;
}

void MessageSystem_Push(MessageSystem_t *ms, const char *text, float duration) {
  if (ms->qCount >= MSG_QUEUE_CAP) return;
  int tail = (ms->qHead + ms->qCount) % MSG_QUEUE_CAP;
  strncpy(ms->queue[tail].text, text, MSG_TEXT_LEN - 1);
  ms->queue[tail].text[MSG_TEXT_LEN - 1] = '\0';
  ms->queue[tail].duration = duration;
  ms->qCount++;
}

void MessageSystem_Update(MessageSystem_t *ms, float dt) {
  switch (ms->state) {
  case MSG_IDLE:
    if (ms->qCount > 0) {
      MsgEntry entry = ms->queue[ms->qHead];
      ms->qHead  = (ms->qHead + 1) % MSG_QUEUE_CAP;
      ms->qCount--;
      strncpy(ms->text, entry.text, MSG_TEXT_LEN - 1);
      ms->text[MSG_TEXT_LEN - 1] = '\0';
      ms->duration   = entry.duration;
      ms->timer      = entry.duration;
      ms->slideY     = OFFSCREEN_Y;
      ms->charsShown = 0.0f;
      ms->lineCount  = BuildLines(ms->text, ms->lines, 8);
      ms->state      = MSG_SLIDING_IN;
    }
    break;

  case MSG_SLIDING_IN:
    ms->slideY     += SLIDE_SPD * dt;
    ms->charsShown += TYPEWRITER_SPD * dt;
    if (ms->slideY >= TARGET_Y) {
      ms->slideY = TARGET_Y;
      ms->state  = MSG_VISIBLE;
    }
    break;

  case MSG_VISIBLE: {
    int total = 0;
    for (int i = 0; i < ms->lineCount; i++) total += (int)strlen(ms->lines[i]);
    ms->charsShown += TYPEWRITER_SPD * dt;
    if (ms->charsShown > (float)total) ms->charsShown = (float)total;
    ms->timer -= dt;
    if (ms->timer <= 0.0f) ms->state = MSG_SLIDING_OUT;
    break;
  }

  case MSG_SLIDING_OUT:
    ms->slideY -= SLIDE_SPD * dt;
    if (ms->slideY <= OFFSCREEN_Y) {
      ms->slideY = OFFSCREEN_Y;
      ms->state  = MSG_IDLE;
    }
    break;
  }
}

void MessageSystem_Render(const MessageSystem_t *ms) {
  if (ms->state == MSG_IDLE) return;

  int sw     = GetScreenWidth();
  int panelH = PanelH(ms->lineCount);
  int px     = sw / 2 - PANEL_W / 2;
  int py     = (int)ms->slideY;

  // Background
  DrawRectangle(px, py, PANEL_W, panelH, C_BG);

  // Scanlines
  for (int sy = py; sy < py + panelH; sy += 4)
    DrawRectangle(px, sy, PANEL_W, 2, C_SCAN);

  // Border
  DrawRectangleLines(px, py, PANEL_W, panelH, C_BORDER);

  // Corner brackets
  int cs = 12;
  DrawRectangle(px,             py,              cs, 2,  C_CORNER);
  DrawRectangle(px,             py,              2,  cs, C_CORNER);
  DrawRectangle(px + PANEL_W - cs, py,           cs, 2,  C_CORNER);
  DrawRectangle(px + PANEL_W - 2,  py,           2,  cs, C_CORNER);
  DrawRectangle(px,             py + panelH - 2, cs, 2,  C_CORNER);
  DrawRectangle(px,             py + panelH - cs, 2, cs, C_CORNER);
  DrawRectangle(px + PANEL_W - cs, py + panelH - 2, cs, 2,  C_CORNER);
  DrawRectangle(px + PANEL_W - 2,  py + panelH - cs, 2, cs, C_CORNER);

  int y = py + 12;

  // Header (centered)
  const char *hdr = "//  INCOMING TRANSMISSION  //";
  int hdrW = MeasureText(hdr, HEADER_FONT);
  DrawText(hdr, px + PANEL_W / 2 - hdrW / 2, y, HEADER_FONT, C_HEADER);
  y += HEADER_FONT + 4;

  // Separator
  DrawRectangle(px + PANEL_PAD, y, PANEL_W - PANEL_PAD * 2, 1, C_SEP);
  y += 11;

  // Message body with typewriter reveal
  int charsLeft = (int)ms->charsShown;
  for (int i = 0; i < ms->lineCount; i++) {
    int len  = (int)strlen(ms->lines[i]);
    int draw = charsLeft < len ? charsLeft : len;
    if (draw < 0) draw = 0;

    char partial[MSG_TEXT_LEN];
    memcpy(partial, ms->lines[i], draw);
    partial[draw] = '\0';
    DrawText(partial, px + PANEL_PAD, y, BODY_FONT, C_BODY);

    // Blinking cursor on the line currently being typed
    if (charsLeft >= 0 && charsLeft < len) {
      int cx = px + PANEL_PAD + MeasureText(partial, BODY_FONT);
      if (((int)(GetTime() * 6)) % 2 == 0)
        DrawText("_", cx, y, BODY_FONT, C_CURSOR);
    }

    charsLeft -= len;
    y += LINE_H;
  }

  y += 10;

  // Duration progress bar
  float progress = (ms->duration > 0.0f) ? (ms->timer / ms->duration) : 1.0f;
  if (progress < 0.0f) progress = 0.0f;
  int barW = PANEL_W - PANEL_PAD * 2;
  DrawRectangle(px + PANEL_PAD, y, barW, 6, C_BARBASE);
  DrawRectangle(px + PANEL_PAD, y, (int)(barW * progress), 6, C_BAR);
}
