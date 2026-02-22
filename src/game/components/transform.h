#pragma once
#include "../../engine/memory/memory_aligned.h"
#include <immintrin.h>

typedef struct ECS_ALIGN(16) {
  float x;
  float y;
  float z;
  float w; // padding
} Position;
