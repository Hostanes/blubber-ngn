#pragma once

#define MSG_QUEUE_CAP 16
#define MSG_TEXT_LEN  256

typedef struct {
  char  text[MSG_TEXT_LEN];
  float duration;
  int   fontSize;  // 0 = use default
} MsgEntry;

typedef enum {
  MSG_IDLE = 0,
  MSG_SLIDING_IN,
  MSG_VISIBLE,
  MSG_SLIDING_OUT,
} MsgState;

typedef struct {
  MsgEntry queue[MSG_QUEUE_CAP];
  int      qHead;
  int      qCount;

  MsgState state;
  char     text[MSG_TEXT_LEN];
  float    duration;
  float    timer;
  float    slideY;
  float    charsShown;

  char     lines[8][MSG_TEXT_LEN];
  int      lineCount;
  int      fontSize;   // active message's font size (0 = default)

  float    queuedNotifTimer;  // > 0 while "queued" badge is visible
} MessageSystem_t;

void MessageSystem_Init(MessageSystem_t *ms);
void MessageSystem_Push(MessageSystem_t *ms, const char *text, float duration, int fontSize);
void MessageSystem_Update(MessageSystem_t *ms, float dt);
void MessageSystem_Render(const MessageSystem_t *ms);
