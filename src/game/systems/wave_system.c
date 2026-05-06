#include "wave_system.h"
#include "../game.h"
#include "../level_creater_helper.h"

#define NEXT_WAVE_DELAY  5.0f
#define FIRST_WAVE_DELAY 3.0f

typedef struct { int grunts; int rangers; int melee; int drones; } WaveDef;

static const WaveDef kWaves[] = {
  { 2, 0, 2, 0},
  { 4, 1, 3, 1},
  { 5, 2, 4, 1},
  { 7, 3, 4, 2},
  { 8, 4, 5, 2},
  {10, 5, 6, 3},
};
#define WAVE_COUNT ((int)(sizeof(kWaves) / sizeof(kWaves[0])))

void WaveSystem_Init(GameWorld *gw) {
  MissionType mt                 = gw->waveState.missionType;
  gw->waveState.currentWave      = 0;
  gw->waveState.enemiesAlive     = 0;
  gw->waveState.nextWaveTimer    = FIRST_WAVE_DELAY;
  gw->waveState.waveActive       = false;
  gw->waveState.allWavesComplete = false;
  gw->waveState.missionType      = mt;
}

static int CountLiveEnemies(world_t *world, GameWorld *gw) {
  int count = 0;
  archetype_t *archs[] = {
      WorldGetArchetype(world, gw->enemyGruntArchId),
      WorldGetArchetype(world, gw->enemyRangerArchId),
      WorldGetArchetype(world, gw->enemyMeleeArchId),
      WorldGetArchetype(world, gw->enemyDroneArchId),
  };
  for (int t = 0; t < 4; t++) {
    archetype_t *a = archs[t];
    if (!a) continue;
    for (uint32_t i = 0; i < a->count; i++) {
      Active *act = ECS_GET(world, a->entities[i], Active, COMP_ACTIVE);
      if (act && act->value) count++;
    }
  }
  return count;
}

static void SpawnFromSpawners(world_t *world, GameWorld *gw, int enemyType, int count) {
  if (count <= 0) return;

  archetype_t *arch = WorldGetArchetype(world, gw->spawnerArchId);

  Vector3 positions[128];
  int nPos = 0;

  // Prefer matching-type spawners
  for (uint32_t i = 0; i < arch->count && nPos < 128; i++) {
    EnemySpawner *sp = ECS_GET(world, arch->entities[i], EnemySpawner, COMP_ENEMY_SPAWNER);
    if (sp && sp->enemyType == enemyType) {
      Position *p = ECS_GET(world, arch->entities[i], Position, COMP_POSITION);
      if (p) positions[nPos++] = p->value;
    }
  }
  // Fallback: any spawner
  if (nPos == 0) {
    for (uint32_t i = 0; i < arch->count && nPos < 128; i++) {
      Position *p = ECS_GET(world, arch->entities[i], Position, COMP_POSITION);
      if (p) positions[nPos++] = p->value;
    }
  }
  // Fallback: hardcoded arena position
  if (nPos == 0) {
    positions[nPos++] = (Vector3){0, 0, 30};
  }

  for (int i = 0; i < count; i++) {
    Vector3 p = positions[i % nPos];
    p.x += (float)GetRandomValue(-300, 300) * 0.01f;
    p.z += (float)GetRandomValue(-300, 300) * 0.01f;
    if      (enemyType == 0) SpawnEnemyGrunt(world,  gw, p);
    else if (enemyType == 1) SpawnEnemyRanger(world, gw, p);
    else if (enemyType == 2) SpawnEnemyMelee(world,  gw, p);
    else                     SpawnEnemyDrone(world,  gw, p);
  }
}

static void StartWave(world_t *world, GameWorld *gw) {
  WaveState *ws = &gw->waveState;
  int idx = ws->currentWave;
  if (idx >= WAVE_COUNT) {
    ws->allWavesComplete = true;
    return;
  }
  ws->currentWave++;
  ws->waveActive = true;
  SpawnFromSpawners(world, gw, 0, kWaves[idx].grunts);
  SpawnFromSpawners(world, gw, 1, kWaves[idx].rangers);
  SpawnFromSpawners(world, gw, 2, kWaves[idx].melee);
  SpawnFromSpawners(world, gw, 3, kWaves[idx].drones);
}

void WaveSystem_Update(world_t *world, GameWorld *gw, float dt) {
  WaveState *ws = &gw->waveState;

  if (ws->missionType == MISSION_EXPLORATION) return;
  if (ws->allWavesComplete) return;

  ws->enemiesAlive = CountLiveEnemies(world, gw);

  if (ws->waveActive) {
    if (ws->enemiesAlive == 0) {
      ws->waveActive    = false;
      ws->nextWaveTimer = NEXT_WAVE_DELAY;
    }
  } else {
    ws->nextWaveTimer -= dt;
    if (ws->nextWaveTimer <= 0.0f)
      StartWave(world, gw);
  }
}
