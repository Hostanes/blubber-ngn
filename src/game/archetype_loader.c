#include "archetype_loader.h"
#include "../engine/ecs/archetype.h"
#include "../engine/util/bitset.h"
#include "../engine/util/json_reader.h"
#include "ecs_get.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_ARCH_COMPONENTS 64
#define COMP_NAME_LEN       64

uint32_t ArchetypeLoader_FromFile(world_t *world, const ComponentRegistry *reg,
                                  const char *path) {
  FILE *f = fopen(path, "r");
  if (!f) {
    fprintf(stderr, "ArchetypeLoader: could not open '%s'\n", path);
    return UINT32_MAX;
  }

  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  rewind(f);

  char *json = malloc((size_t)size + 1);
  fread(json, 1, (size_t)size, f);
  json[size] = '\0';
  fclose(f);

  char names[MAX_ARCH_COMPONENTS][COMP_NAME_LEN];
  int count = JsonReadStringArray(json, "components", (char *)names,
                                  MAX_ARCH_COMPONENTS, COMP_NAME_LEN);
  free(json);

  if (count == 0) {
    fprintf(stderr, "ArchetypeLoader: no components in '%s'\n", path);
    return UINT32_MAX;
  }

  // Build component ID array for the bitmask
  uint32_t ids[MAX_ARCH_COMPONENTS];
  int idCount = 0;
  for (int i = 0; i < count; i++) {
    const ComponentDef *def = ComponentRegistry_Find(reg, names[i]);
    if (!def) {
      fprintf(stderr, "ArchetypeLoader: unknown component '%s' in '%s'\n",
              names[i], path);
      continue;
    }
    ids[idCount++] = def->id;
  }

  bitset_t mask = MakeMask(ids, (uint32_t)idCount);

  uint32_t archId = WorldCreateArchetype(world, &mask);
  archetype_t *arch = WorldGetArchetype(world, archId);
  arch->id = archId;

  for (int i = 0; i < count; i++)
    ComponentRegistry_AddToArchetype(reg, arch, names[i]);

  return archId;
}
