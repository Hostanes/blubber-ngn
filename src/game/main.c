#include "game.h"
#include "omp.h"

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
