#pragma once
#include <stdbool.h>

typedef struct {
  float halfExtent;
  char  message[256];
  float duration;
  bool  triggered;
} InfoBox;
