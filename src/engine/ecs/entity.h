
#pragma once
#include "ecs_types.h"
#include "stdbool.h"

typedef struct entityManager_t entityManager_t;

void EntityManagerInit(entityManager_t *entityManager);
void EntityManagerShutdown(entityManager_t *entityManager);

entity_t EntityCreate(entityManager_t *entityManager);
void EntityDestroy(entityManager_t *entityManager, entity_t entity);

bool EntityIsAlive(const entityManager_t *entityManager, entity_t entity);
