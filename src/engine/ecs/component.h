
#pragma once
#include "component_internal.h"
#include "ecs_types.h"
#include "stdbool.h"
#include <stddef.h>

typedef struct componentPool_t componentPool_t;

void ComponentPoolInit(componentPool_t *componentPool, size_t elementSize);

void *ComponentAdd(componentPool_t *componentPool, entity_t entity);

void ComponentRemove(componentPool_t *componentPool, entity_t entity);

void *ComponentGet(componentPool_t *componentPool, entity_t entity);

bool ComponentHas(const componentPool_t *componentPool, entity_t entity);
