
#include "../engine/ecs/entity.h"
#include "../engine/ecs/entity_internal.h"
#include "../engine/ecs/component.h"
#include <stdio.h>

int main(void) {
  entityManager_t entityManager;
  EntityManagerInit(&entityManager);

  componentPool_t pool;
  ComponentPoolInit(&pool, sizeof(int));

  entity_t e1 = {.id = 1};
  entity_t e2 = {.id = 5};

  int *a = ComponentAdd(&pool, e1);
  int *b = ComponentAdd(&pool, e2);

  *a = 42;
  *b = 7;

  ComponentRemove(&pool, e1);

  return 0;
}
