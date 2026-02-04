
#include "../engine/ecs/archetype.h"
#include "../engine/ecs/component.h"
#include "../engine/ecs/entity.h"
#include "../engine/ecs/entity_internal.h"
#include "../engine/ecs/world.h"
#include "../engine/util/bitset.h"
#include <stdio.h>

enum {
  COMP_POSITION = 0,
  COMP_HEALTH = 1,
  COMP_TIMER = 2,
};

typedef struct {
  float x, y, z;
} Position;

typedef struct {
  int value;
} Health;

typedef struct {
  float time;
} Timer;

static bitset_t MakeMask(uint32_t *bits, uint32_t count) {
  bitset_t mask;
  BitsetInit(&mask, 64);

  for (uint32_t i = 0; i < count; ++i) {
    BitsetSet(&mask, bits[i]);
  }

  return mask;
}

int main(void) {
  world_t *world = WorldCreate();

  /* ---------- global pooled components ---------- */

  componentPool_t timerPool;
  ComponentPoolInit(&timerPool, sizeof(Timer));

  /* ---------- archetype 1: Position + Timer ---------- */

  uint32_t maskA_bits[] = {COMP_POSITION, COMP_TIMER};
  bitset_t maskA = MakeMask(maskA_bits, 2);

  archetype_t *archA = WorldCreateArchetype(world, &maskA);
  ArchetypeAddInline(archA, COMP_POSITION, sizeof(Position));
  ArchetypeAddPool(archA, COMP_TIMER, &timerPool);

  /* ---------- archetype 2: Position + Health + Timer ---------- */

  uint32_t maskB_bits[] = {COMP_POSITION, COMP_HEALTH, COMP_TIMER};
  bitset_t maskB = MakeMask(maskB_bits, 3);

  archetype_t *archB = WorldCreateArchetype(world, &maskB);
  ArchetypeAddInline(archB, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(archB, COMP_HEALTH, sizeof(Health));
  ArchetypeAddPool(archB, COMP_TIMER, &timerPool);

  /* ---------- create entities ---------- */

  entity_t e1 = WorldCreateEntity(world, &maskA);
  entity_t e2 = WorldCreateEntity(world, &maskB);

  /* ---------- write components ---------- */

  Position *p1 = WorldGetComponent(world, e1, COMP_POSITION);
  Timer *t1 = WorldGetComponent(world, e1, COMP_TIMER);

  p1->x = 1;
  p1->y = 2;
  p1->z = 3;
  t1->time = 5.0f;

  Position *p2 = WorldGetComponent(world, e2, COMP_POSITION);
  Health *h2 = WorldGetComponent(world, e2, COMP_HEALTH);
  Timer *t2 = WorldGetComponent(world, e2, COMP_TIMER);

  p2->x = 10;
  p2->y = 20;
  p2->z = 30;
  h2->value = 100;
  t2->time = 9.0f;

  /* ---------- read back ---------- */

  printf("Entity A: pos=(%.1f %.1f %.1f) timer=%.1f\n", p1->x, p1->y, p1->z,
         t1->time);

  printf("Entity B: pos=(%.1f %.1f %.1f) hp=%d timer=%.1f\n", p2->x, p2->y,
         p2->z, h2->value, t2->time);

  p1->x += 55;
  p1->y += 55;
  p1->z += 55;
  t1->time -= 1.0f;

  p2->x += 5;
  p2->y += 5;
  p2->z += 5;
  h2->value -= 50;
  t2->time -= 1.0f;

  printf("Entity A: pos=(%.1f %.1f %.1f) timer=%.1f\n", p1->x, p1->y, p1->z,
         t1->time);

  printf("Entity B: pos=(%.1f %.1f %.1f) hp=%d timer=%.1f\n", p2->x, p2->y,
         p2->z, h2->value, t2->time);

  WorldDestroy(world);
  return 0;
}
