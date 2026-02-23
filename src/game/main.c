#include "game.h"
#include "omp.h"
#include <float.h>
#include <math.h>

/* ================= Utilities ================= */

typedef struct {
  double mean;
  double stddev;
  double min;
  double max;
} Stats;

static Stats ComputeStats(double *values, int count) {
  Stats s = {0};
  if (count == 0)
    return s;

  double sum = 0.0;
  s.min = DBL_MAX;
  s.max = -DBL_MAX;

  for (int i = 0; i < count; i++) {
    double v = values[i];
    sum += v;

    if (v < s.min)
      s.min = v;
    if (v > s.max)
      s.max = v;
  }

  s.mean = sum / count;

  double variance = 0.0;
  for (int i = 0; i < count; i++) {
    double diff = values[i] - s.mean;
    variance += diff * diff;
  }

  variance /= count;
  s.stddev = sqrt(variance);

  return s;
}

/* ================= Main ================= */

double BenchmarkMovementSystem(world_t *world, Engine *engine,
                               archetype_t *arch, float dt) {
  double start = GetTime();

  bool hasTimer = ArchetypeHas(arch, COMP_TIMER);

  archetypeColumn_t *posCol = ArchetypeFindColumn(arch, COMP_POSITION);
  archetypeColumn_t *velCol = ArchetypeFindColumn(arch, COMP_VELOCITY);
  componentPool_t *timerPool = &engine->timerPool;

  Position *positions = (Position *)posCol->data;
  Velocity *velocities = (Velocity *)velCol->data;
  float *timers = hasTimer ? (float *)timerPool->denseData : NULL;

  uint32_t count = arch->count;

#pragma omp parallel for if (count >= OMP_MIN_ITERATIONS)
  for (uint32_t i = 0; i < count; ++i) {

    positions[i].value.x += velocities[i].value.x * dt;
    positions[i].value.y += velocities[i].value.y * dt;
    positions[i].value.z += velocities[i].value.z * dt;

    if (hasTimer) {
      timers[i] -= dt;

      if (timers[i] <= 0.0f)
        timers[i] = 5.0f;
    }
  }

  double end = GetTime();
  return count / (end - start);
}

double BenchmarkTimerSystem(componentPool_t *pool, float dt) {

  float *timers = (float *)pool->denseData;
  uint32_t count = pool->count;

  double start = GetTime();

#pragma omp parallel for if (count >= OMP_MIN_ITERATIONS)
  for (uint32_t i = 0; i < count; ++i) {
    timers[i] -= dt;
  }

  double end = GetTime();
  return count / (end - start);
}

void RunBenchmark(world_t *world, Engine *engine, archetype_t *moveArch,
                  archetype_t *timerArch) {

  const float dt = 0.016f;
  const int iterations = 10000;

  double *moveResults = malloc(sizeof(double) * iterations * 2);
  double *timerResults = malloc(sizeof(double) * iterations);

  int moveIndex = 0;

  /* ---------- Warmup ---------- */

  for (int i = 0; i < 200; i++) {
    BenchmarkTimerSystem(&engine->timerPool, dt);
    BenchmarkMovementSystem(world, engine, moveArch, dt);
    BenchmarkMovementSystem(world, engine, timerArch, dt);
  }

  /* ---------- Measured ---------- */

  for (int i = 0; i < iterations; i++) {

    timerResults[i] = BenchmarkTimerSystem(&engine->timerPool, dt);

    moveResults[moveIndex++] =
        BenchmarkMovementSystem(world, engine, moveArch, dt);

    moveResults[moveIndex++] =
        BenchmarkMovementSystem(world, engine, timerArch, dt);
  }

  Stats moveStats = ComputeStats(moveResults, moveIndex);
  Stats timerStats = ComputeStats(timerResults, iterations);

  printf("\n=== Movement System ===\n");
  printf("Mean:    %.2f M updates/sec\n", moveStats.mean / 1e6);
  printf("StdDev:  %.2f M\n", moveStats.stddev / 1e6);
  printf("Min:     %.2f M\n", moveStats.min / 1e6);
  printf("Max:     %.2f M\n", moveStats.max / 1e6);

  printf("\n=== Timer System ===\n");
  printf("Mean:    %.2f M updates/sec\n", timerStats.mean / 1e6);
  printf("StdDev:  %.2f M\n", timerStats.stddev / 1e6);
  printf("Min:     %.2f M\n", timerStats.min / 1e6);
  printf("Max:     %.2f M\n", timerStats.max / 1e6);

  free(moveResults);
  free(timerResults);
}

int main(void) {

  omp_set_num_threads(omp_get_num_procs() / 2);

  Engine engine = EngineInit();
  GameWorld game = GameWorldCreate(&engine, engine.world);

  RunBenchmark(engine.world, &engine,
               WorldGetArchetype(engine.world, game.moveArchId),
               WorldGetArchetype(engine.world, game.timerArchId));

  EngineShutdown(&engine);
  return 0;
}
