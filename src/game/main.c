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

void BenchmarkMovementSystem(world_t *world, archetype_t *arch, float dt) {
  double start = GetTime();

#pragma omp parallel for if (arch->count >= OMP_MIN_ITERATIONS)
  for (uint32_t i = 0; i < arch->count; i++) {
    entity_t e = arch->entities[i];

    Position *p = ECS_GET(world, e, Position, COMP_POSITION);
    Velocity *v = ECS_GET(world, e, Velocity, COMP_VELOCITY);

    p->value.x += v->value.x * dt;
    p->value.y += v->value.y * dt;
    p->value.z += v->value.z * dt;
  }

  double end = GetTime();

  double ms = (end - start) * 1000.0;
  double updatesPerSecond = arch->count / (end - start);

  printf("Entities: %u | Time: %.3f ms | %.2f M updates/sec\n", arch->count, ms,
         updatesPerSecond / 1000000.0);
}

void RunBenchmark(world_t *world, archetype_t *arch) {
  const float dt = 0.016f;

  for (int i = 0; i < 10000; i++) {
    BenchmarkMovementSystem(world, arch, dt);
  }
}

int main(void) {

  omp_set_num_threads(omp_get_num_procs() / 2);

  Engine engine = EngineInit();
  GameWorld game = GameWorldCreate(&engine, engine.world);
  RunBenchmark(engine.world, WorldGetArchetype(engine.world, game.benchArchId));
  EngineShutdown(&engine);
  return 0;
}
