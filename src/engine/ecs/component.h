
#pragma once
#include "component_internal.h"
#include "ecs_types.h"
#include "stdbool.h"
#include <stddef.h>

typedef struct componentPool_t componentPool_t;

void ComponentPoolInit(componentPool_t *componentPool, size_t elementSize);

void *ComponentAdd(componentPool_t *componentPool, uint32_t handle);

void ComponentRemove(componentPool_t *componentPool, uint32_t handle);

void *ComponentGet(componentPool_t *componentPool, uint32_t handle);

bool ComponentHas(const componentPool_t *componentPool, uint32_t handle);

uint32_t ComponentCreate(componentPool_t *pool);
