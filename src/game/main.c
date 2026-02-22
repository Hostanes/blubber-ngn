#include "game.h"
#include "omp.h"
#include <immintrin.h>

/* ================= Main ================= */

void BenchmarkMovementSystem(world_t *world, archetype_t *arch, float dt) {
  double start = GetTime();

  bool hasTimer = ArchetypeHas(arch, COMP_TIMER);

  archetypeColumn_t *posCol = ArchetypeFindColumn(arch, COMP_POSITION);

  archetypeColumn_t *velCol = ArchetypeFindColumn(arch, COMP_VELOCITY);

  archetypeColumn_t *timerCol =
      hasTimer ? ArchetypeFindColumn(arch, COMP_TIMER) : NULL;

  Position *positions = (Position *)posCol->data;
  Velocity *velocities = (Velocity *)velCol->data;
  Timer *timers = hasTimer ? (Timer *)timerCol->data : NULL;

  uint32_t count = arch->count;
  uint32_t i = 0;

  __m128 dtVec = _mm_set1_ps(dt);
  __m128 zero = _mm_setzero_ps();
  __m128 reset = _mm_set1_ps(5.0f);

  for (; i + 3 < count; i += 4) {
    // load 4 positions
    __m128 p0 = _mm_load_ps((float *)&positions[i + 0]);
    __m128 p1 = _mm_load_ps((float *)&positions[i + 1]);
    __m128 p2 = _mm_load_ps((float *)&positions[i + 2]);
    __m128 p3 = _mm_load_ps((float *)&positions[i + 3]);

    // load 4 velocities
    __m128 v0 = _mm_load_ps((float *)&velocities[i + 0]);
    __m128 v1 = _mm_load_ps((float *)&velocities[i + 1]);
    __m128 v2 = _mm_load_ps((float *)&velocities[i + 2]);
    __m128 v3 = _mm_load_ps((float *)&velocities[i + 3]);

    // p += v * dt
    p0 = _mm_add_ps(p0, _mm_mul_ps(v0, dtVec));
    p1 = _mm_add_ps(p1, _mm_mul_ps(v1, dtVec));
    p2 = _mm_add_ps(p2, _mm_mul_ps(v2, dtVec));
    p3 = _mm_add_ps(p3, _mm_mul_ps(v3, dtVec));

    _mm_store_ps((float *)&positions[i + 0], p0);
    _mm_store_ps((float *)&positions[i + 1], p1);
    _mm_store_ps((float *)&positions[i + 2], p2);
    _mm_store_ps((float *)&positions[i + 3], p3);

    if (hasTimer) {
      __m128 t0 = _mm_load_ps((float *)&timers[i + 0]);
      __m128 t1 = _mm_load_ps((float *)&timers[i + 1]);
      __m128 t2 = _mm_load_ps((float *)&timers[i + 2]);
      __m128 t3 = _mm_load_ps((float *)&timers[i + 3]);

      __m128 m0 = _mm_cmple_ps(t0, zero);
      __m128 m1 = _mm_cmple_ps(t1, zero);
      __m128 m2 = _mm_cmple_ps(t2, zero);
      __m128 m3 = _mm_cmple_ps(t3, zero);

      t0 = _mm_blendv_ps(t0, reset, m0);
      t1 = _mm_blendv_ps(t1, reset, m1);
      t2 = _mm_blendv_ps(t2, reset, m2);
      t3 = _mm_blendv_ps(t3, reset, m3);

      _mm_store_ps((float *)&timers[i + 0], t0);
      _mm_store_ps((float *)&timers[i + 1], t1);
      _mm_store_ps((float *)&timers[i + 2], t2);
      _mm_store_ps((float *)&timers[i + 3], t3);
    }
  }

  // scalar tail
  for (; i < count; ++i) {
    positions[i].x += velocities[i].x * dt;
    positions[i].y += velocities[i].y * dt;
    positions[i].z += velocities[i].z * dt;

    if (hasTimer && timers[i].value <= 0.0f)
      timers[i].value = 5.0f;
  }

  double end = GetTime();

  printf("Entities: %u | %.2f M updates/sec\n", count,
         (count / (end - start)) / 1000000.0);
}

void BenchmarkTimerSystem(world_t *world, archetype_t *arch, float dt) {
  archetypeColumn_t *timerCol = ArchetypeFindColumn(arch, COMP_TIMER);

  Timer *timers = (Timer *)timerCol->data;

  uint32_t count = arch->count;
  uint32_t i = 0;

  __m128 dtVec = _mm_set1_ps(dt);

  for (; i + 3 < count; i += 4) {
    __m128 t0 = _mm_load_ps((float *)&timers[i + 0]);
    __m128 t1 = _mm_load_ps((float *)&timers[i + 1]);
    __m128 t2 = _mm_load_ps((float *)&timers[i + 2]);
    __m128 t3 = _mm_load_ps((float *)&timers[i + 3]);

    t0 = _mm_sub_ps(t0, dtVec);
    t1 = _mm_sub_ps(t1, dtVec);
    t2 = _mm_sub_ps(t2, dtVec);
    t3 = _mm_sub_ps(t3, dtVec);

    _mm_store_ps((float *)&timers[i + 0], t0);
    _mm_store_ps((float *)&timers[i + 1], t1);
    _mm_store_ps((float *)&timers[i + 2], t2);
    _mm_store_ps((float *)&timers[i + 3], t3);
  }

  for (; i < count; ++i)
    timers[i].value -= dt;
}

void RunBenchmark(world_t *world, archetype_t *moveArch,
                  archetype_t *timerArch) {
  const float dt = 0.016f;

  for (int i = 0; i < 1000; i++) {

    BenchmarkTimerSystem(world, timerArch, dt);

    BenchmarkMovementSystem(world, moveArch, dt);
    BenchmarkMovementSystem(world, timerArch, dt);
  }
}

int main(void) {

  omp_set_num_threads(omp_get_num_procs() / 2);

  Engine engine = EngineInit();
  GameWorld game = GameWorldCreate(&engine, engine.world);
  RunBenchmark(engine.world, WorldGetArchetype(engine.world, game.moveArchId),
               WorldGetArchetype(engine.world, game.timerArchId));
  EngineShutdown(&engine);
  return 0;
}
