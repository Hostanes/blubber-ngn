#include "game.h"
#include <float.h>
#include <math.h>

#define POOL_SIZE       100000
#define WARMUP_ITERS    200
#define MEASURE_ITERS   1000
#define FRAME_BUDGET_MS 16.667

typedef struct {
  double mean;
  double stddev;
  double min;
  double max;
} Stats;

static Stats ComputeStats(double *values, int count) {
  Stats s = {0};
  s.min = DBL_MAX;
  s.max = -DBL_MAX;

  double sum = 0.0;
  for (int i = 0; i < count; i++) {
    sum += values[i];
    if (values[i] < s.min) s.min = values[i];
    if (values[i] > s.max) s.max = values[i];
  }
  s.mean = sum / count;

  double var = 0.0;
  for (int i = 0; i < count; i++) {
    double d = values[i] - s.mean;
    var += d * d;
  }
  s.stddev = sqrt(var / count);
  return s;
}

static void ChurnEntities(world_t *world, const bitset_t *mask,
                          entity_t *pool, uint32_t K) {
  for (uint32_t j = 0; j < K; j++)
    WorldDestroyEntity(world, pool[j]);

  for (uint32_t j = 0; j < K; j++) {
    pool[j] = WorldCreateEntity(world, mask);
    Position *p = ECS_GET(world, pool[j], Position, COMP_POSITION);
    Velocity *v = ECS_GET(world, pool[j], Velocity, COMP_VELOCITY);
    p->value = (Vector3){0};
    v->value = (Vector3){0.1f, 0, 0.1f};
  }
}

int main(void) {
  Engine engine = EngineInit();

  uint32_t bits[] = {COMP_POSITION, COMP_VELOCITY};
  bitset_t mask = MakeMask(bits, 2);

  uint32_t archId = WorldCreateArchetype(engine.world, &mask);
  archetype_t *arch = WorldGetArchetype(engine.world, archId);
  ArchetypeAddInline(arch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(arch, COMP_VELOCITY, sizeof(Velocity));

  entity_t *pool = malloc(sizeof(entity_t) * POOL_SIZE);
  for (uint32_t i = 0; i < POOL_SIZE; i++) {
    pool[i] = WorldCreateEntity(engine.world, &mask);
    Position *p = ECS_GET(engine.world, pool[i], Position, COMP_POSITION);
    Velocity *v = ECS_GET(engine.world, pool[i], Velocity, COMP_VELOCITY);
    p->value = (Vector3){0};
    v->value = (Vector3){(float)(i % 10) * 0.1f, 0, (float)((i * 7) % 10) * 0.1f};
  }

  static const uint32_t churnRates[] = {100, 500, 1000, 5000, 10000, 50000};
  int numRates = (int)(sizeof(churnRates) / sizeof(churnRates[0]));

  double *results = malloc(sizeof(double) * MEASURE_ITERS);

  printf("=== Benchmark C: Entity Churn ===\n");
  printf("Pool: %d entities | Components: Position + Velocity\n", POOL_SIZE);
  printf("Frame budget: %.2f ms (60 fps)\n\n", FRAME_BUDGET_MS);
  printf("%-10s  %-10s  %-10s  %-10s  %-10s  %s\n",
         "K (churn)", "Mean ms", "StdDev", "Min ms", "Max ms", "60fps?");
  printf("----------------------------------------------------------------------\n");

  for (int r = 0; r < numRates; r++) {
    uint32_t K = churnRates[r];

    for (int w = 0; w < WARMUP_ITERS; w++)
      ChurnEntities(engine.world, &mask, pool, K);

    for (int i = 0; i < MEASURE_ITERS; i++) {
      double start = GetTime();
      ChurnEntities(engine.world, &mask, pool, K);
      results[i] = (GetTime() - start) * 1000.0;
    }

    Stats s = ComputeStats(results, MEASURE_ITERS);
    printf("%-10u  %-10.3f  %-10.3f  %-10.3f  %-10.3f  %s\n",
           K, s.mean, s.stddev, s.min, s.max,
           s.mean <= FRAME_BUDGET_MS ? "YES" : "NO");
  }

  free(results);
  free(pool);
  EngineShutdown(&engine);
  return 0;
}
