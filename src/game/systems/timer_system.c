#include "systems.h"

void TimerSystem(componentPool_t *timerPool, float dt) {
  Timer *timers = (Timer *)timerPool->denseData;
#pragma omp parallel for if (timerPool->count >= OMP_MIN_ITERATIONS)
  for (uint32_t i = 0; i < timerPool->count; ++i) {
    timers[i].value -= dt;
    timers[i].value = timers[i].value <= 0 ? 0 : timers[i].value;
  }
}
