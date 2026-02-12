#include "game.h"
#include "omp.h"

/* ================= Utilities ================= */

void ModelCollectionInit(ModelCollection_t *mc, uint32_t initialCapacity) {
  mc->count = 0;
  mc->capacity = initialCapacity;
  mc->models = malloc(sizeof(ModelInstance_t) * initialCapacity);
}

void ModelCollectionAdd(ModelCollection_t *mc, ModelInstance_t instance) {
  if (mc->count >= mc->capacity) {
    mc->capacity = mc->capacity ? mc->capacity * 2 : 2;
    mc->models = realloc(mc->models, sizeof(ModelInstance_t) * mc->capacity);
  }
  mc->models[mc->count++] = instance;
}

void ModelCollectionFree(ModelCollection_t *mc) {
  free(mc->models);
  mc->models = NULL;
  mc->count = mc->capacity = 0;
}

static Vector3 ResolveModelRotation(const Orientation *entityOri,
                                    const ModelInstance_t *mi) {
  Vector3 result = mi->rotation;

  switch (mi->rotationMode) {
  case MODEL_ROT_WORLD:
    // no entity rotation
    break;

  case MODEL_ROT_YAW_ONLY:
    result.y += entityOri->yaw;
    break;

  case MODEL_ROT_YAW_PITCH:
    result.y += entityOri->yaw;
    result.x += entityOri->pitch;
    break;

  case MODEL_ROT_FULL:
    result.x += entityOri->pitch;
    result.y += entityOri->yaw;
    break;
  }

  return result;
}

/* ================= Main ================= */

int main(void) {

  omp_set_num_threads(omp_get_num_procs() / 2);

  Engine engine = EngineInit();
  GameWorld game = GameWorldCreate(&engine, engine.world);
  RunGameLoop(&engine, &game);
  EngineShutdown(&engine);
  return 0;
}
