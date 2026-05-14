#pragma once
#include <stdbool.h>

typedef struct {
  float halfExtent;
  char  message[256];
  float duration;
  int   triggersLeft;   // -1 = infinite, 0 = exhausted, >0 = remaining
  bool  playerInside;   // true while player is inside — triggers only on entry
  float markerHeight;   // Y offset of rotating model from box center
  int   fontSize;       // 0 = use default (15)
} InfoBox;
