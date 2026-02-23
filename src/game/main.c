#include "game.h"
#include "omp.h"
#include <float.h>
#include <math.h>

#define BENCHMARK_ITERATIONS 1000
#define WARMUP_ITERATIONS 200

typedef struct {
  double mean;
  double stddev;
  double min;
  double max;
} Stats;

static Stats ComputeStats(double *values, uint32_t count) {
  Stats s = {0};
  s.min = DBL_MAX;
  s.max = -DBL_MAX;

  double sum = 0.0;
  for (uint32_t i = 0; i < count; ++i) {
    sum += values[i];
    if (values[i] < s.min)
      s.min = values[i];
    if (values[i] > s.max)
      s.max = values[i];
  }

  s.mean = sum / count;

  double variance = 0.0;
  for (uint32_t i = 0; i < count; ++i) {
    double diff = values[i] - s.mean;
    variance += diff * diff;
  }

  s.stddev = sqrt(variance / count);

  return s;
}

double BenchmarkMovementSystem(archetype_t *arch, float dt) {

  archetypeColumn_t *posCol = ArchetypeFindColumn(arch, COMP_POSITION);
  archetypeColumn_t *velCol = ArchetypeFindColumn(arch, COMP_VELOCITY);

  Position *positions = (Position *)posCol->data;
  Velocity *velocities = (Velocity *)velCol->data;

  double start = GetTime();

#pragma omp parallel for if (arch->count >= OMP_MIN_ITERATIONS)
  for (uint32_t i = 0; i < arch->count; i++) {
    positions[i].x += velocities[i].x * dt;
    positions[i].y += velocities[i].y * dt;
    positions[i].z += velocities[i].z * dt;
  }

  double end = GetTime();

  return arch->count / (end - start);
}

double BenchmarkTimerSystemPool(componentPool_t *pool, float dt) {
  Timer *timers = (Timer *)pool->denseData;
  uint32_t count = pool->count;

  double start = GetTime();

#pragma omp parallel for if (count >= OMP_MIN_ITERATIONS)
  for (uint32_t i = 0; i < count; ++i) {
    timers[i].value -= dt;
  }

  double end = GetTime();

  return count / (end - start);
}

void RunBenchmark(Engine *engine, GameWorld *gw) {

  const float dt = 0.016f;
  double results[BENCHMARK_ITERATIONS];

  // Warmup
  for (int w = 0; w < WARMUP_ITERATIONS; ++w) {

    BenchmarkTimerSystemPool(&engine->timerPool, dt);

    for (uint32_t a = 0; a < gw->archCount; ++a) {
      archetype_t *arch = WorldGetArchetype(engine->world, gw->archIds[a]);

      BenchmarkMovementSystem(arch, dt);
    }
  }

  // Measured runs
  for (int i = 0; i < BENCHMARK_ITERATIONS; ++i) {

    double totalUpdates = 0.0;
    double start = GetTime();

    // Count entities (movement work)
    for (uint32_t a = 0; a < gw->archCount; ++a) {
      archetype_t *arch = WorldGetArchetype(engine->world, gw->archIds[a]);

      totalUpdates += arch->count;
    }

    // Timer system (pooled, once globally)
    BenchmarkTimerSystemPool(&engine->timerPool, dt);

    // Movement system (still archetype-based)
    for (uint32_t a = 0; a < gw->archCount; ++a) {
      archetype_t *arch = WorldGetArchetype(engine->world, gw->archIds[a]);

      BenchmarkMovementSystem(arch, dt);
    }

    double end = GetTime();

    results[i] = totalUpdates / (end - start);
  }

  Stats s = ComputeStats(results, BENCHMARK_ITERATIONS);

  printf("=== Pooled Timer Layout (Movement + Timer) ===\n");
  printf("Number of archetypes: %d\n", gw->archCount);
  printf("Mean:   %.2f M updates/sec\n", s.mean / 1e6);
  printf("StdDev: %.2f M\n", s.stddev / 1e6);
  printf("Min:    %.2f M\n", s.min / 1e6);
  printf("Max:    %.2f M\n", s.max / 1e6);
}

int main(void) {

  omp_set_num_threads(omp_get_num_procs() / 2);

  Engine engine = EngineInit();
  GameWorld game = GameWorldCreate(&engine, engine.world);

  RunBenchmark(&engine, &game);

  EngineShutdown(&engine);
  return 0;
}
