#pragma once
#include "../engine/ecs/component_registry.h"
#include "../engine/ecs/world.h"
#include <stdint.h>

// Loads an archetype from a JSON file.
// Reads the "components" array, builds the bitmask, creates the archetype,
// and adds all columns via the component registry.
// Returns the archetype ID, or UINT32_MAX on failure.
uint32_t ArchetypeLoader_FromFile(world_t *world, const ComponentRegistry *reg,
                                  const char *path);
