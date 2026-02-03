#pragma once
#include <stdint.h>

typedef struct {
  uint32_t id;
  uint32_t generation;
  uint32_t type;
} entity_t;

typedef uint32_t componentId_t;
