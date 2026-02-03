#pragma once
#include <stdint.h>

struct entityManager_t {
  uint32_t *generations; // index = entity id
  uint32_t *freeIds;     // stack of reusable ids
  uint32_t freeCount;    //

  uint32_t capacity; // total allocated ids
  uint32_t nextId;
};
