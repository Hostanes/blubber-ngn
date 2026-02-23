#include "game.h"
#include "omp.h"
#include <float.h>
#include <immintrin.h>
#include <math.h>

/* ================= Main ================= */

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

double BenchmarkMovementSystem(world_t *world, Engine *engine,
                               archetype_t *arch, float dt) {

  double start = GetTime();

  bool hasTimer = ArchetypeHas(arch, COMP_TIMER);

  archetypeColumn_t *posCol = ArchetypeFindColumn(arch, COMP_POSITION);
  archetypeColumn_t *velCol = ArchetypeFindColumn(arch, COMP_VELOCITY);

  Position *positions = (Position *)posCol->data;
  Velocity *velocities = (Velocity *)velCol->data;

  float *timers = hasTimer ? (float *)engine->timerPool.denseData : NULL;

  uint32_t count = arch->count;
  uint32_t i = 0;

  __m128 dtVec = _mm_set1_ps(dt);
  __m128 zero = _mm_setzero_ps();
  __m128 reset = _mm_set1_ps(5.0f);

  /* -------- SIMD -------- */

  for (; i + 3 < count; i += 4) {

    __m128 p = _mm_load_ps((float *)&positions[i]);
    __m128 v = _mm_load_ps((float *)&velocities[i]);

    p = _mm_add_ps(p, _mm_mul_ps(v, dtVec));

    _mm_store_ps((float *)&positions[i], p);

    if (hasTimer) {
      __m128 t = _mm_load_ps(&timers[i]);

      __m128 mask = _mm_cmple_ps(t, zero);
      t = _mm_blendv_ps(t, reset, mask);

      _mm_store_ps(&timers[i], t);
    }
  }

  /* -------- Scalar tail -------- */

  for (; i < count; ++i) {

    positions[i].x += velocities[i].x * dt;
    positions[i].y += velocities[i].y * dt;
    positions[i].z += velocities[i].z * dt;

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

  __m128 dtVec = _mm_set1_ps(dt);

  uint32_t i = 0;
  double start = GetTime();

  /* -------- SIMD -------- */

  for (; i + 3 < count; i += 4) {
    __m128 t = _mm_load_ps(&timers[i]); // aligned load
    t = _mm_sub_ps(t, dtVec);
    _mm_store_ps(&timers[i], t); // aligned store
  }

  /* -------- Scalar tail -------- */

  for (; i < count; ++i) {
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

  // warm up 200 iterations
  for (int i = 0; i < 200; i++) {
    BenchmarkTimerSystem(&engine->timerPool, dt);
    BenchmarkMovementSystem(world, engine, moveArch, dt);
    BenchmarkMovementSystem(world, engine, timerArch, dt);
  }

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
