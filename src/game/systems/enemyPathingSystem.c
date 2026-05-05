#include "../game.h"
#include "enemy_behaviour.h"
#include "systems.h"
#include <math.h>

/* ------------------------------------------------------------------ */
/*  Path request queue                                                 */
/*  Spreads A* calls across frames to avoid wave-start spikes.        */
/* ------------------------------------------------------------------ */

#define PATH_QUEUE_CAP 64

typedef struct {
  NavGrid       *grid;
  Vector3        start;
  Vector3        goal;
  NavPath       *outPath;
  bool          *pendingFlag;
  CombatState_t *combat;
  entity_t       owner;
} PathRequest;

static PathRequest s_pathQueue[PATH_QUEUE_CAP];
static int         s_pathHead  = 0;
static int         s_pathTail  = 0;
static int         s_pathCount = 0;

bool EnemyPathQueue_Submit(NavGrid *grid, Vector3 start, Vector3 goal,
                           NavPath *outPath, bool *pendingFlag,
                           CombatState_t *combat, entity_t owner) {
  if (s_pathCount >= PATH_QUEUE_CAP) return false;

  s_pathQueue[s_pathTail] = (PathRequest){
      .grid        = grid,
      .start       = start,
      .goal        = goal,
      .outPath     = outPath,
      .pendingFlag = pendingFlag,
      .combat      = combat,
      .owner       = owner,
  };
  s_pathTail = (s_pathTail + 1) % PATH_QUEUE_CAP;
  s_pathCount++;
  *pendingFlag = true;
  return true;
}

void EnemyPathQueue_Reset(void) {
  s_pathHead  = 0;
  s_pathTail  = 0;
  s_pathCount = 0;
}

void EnemyPathQueue_Flush(world_t *world, int maxPerFrame) {
  int processed = 0;
  while (s_pathCount > 0 && processed < maxPerFrame) {
    PathRequest *req = &s_pathQueue[s_pathHead];
    s_pathHead = (s_pathHead + 1) % PATH_QUEUE_CAP;
    s_pathCount--;
    processed++;

    // Skip if the requesting entity died since the request was submitted
    if (!EntityIsAlive(&world->entityManager, req->owner)) {
      *req->pendingFlag = false;
      continue;
    }

    bool ok = NavGrid_FindPath(req->grid, req->start, req->goal, req->outPath);
    *req->pendingFlag = false;
    if (ok && req->combat)
      req->combat->state = ENEMY_STATE_MOVING;
  }
}

/* ------------------------------------------------------------------ */
/*  Generic path follower with smooth acceleration / deceleration     */
/* ------------------------------------------------------------------ */

static const float ARRIVE_THRESHOLD = 0.5f;
static const float FACE_THRESHOLD   = 0.5f;

static float PathRemainingLength(NavPath *path, Position *pos) {
  if (path->currentIndex >= path->count) return 0.0f;
  float total = Vector3Distance(pos->value, path->points[path->currentIndex]);
  for (int i = path->currentIndex; i < path->count - 1; i++)
    total += Vector3Distance(path->points[i], path->points[i + 1]);
  return total;
}

// Returns true when the entity has fully arrived at the end of its path.
bool EnemyFollowPath(world_t *world, GameWorld *game, entity_t e,
                     float maxSpeed, float rotateSpeed, float dt) {
  Position    *pos  = ECS_GET(world, e, Position,    COMP_POSITION);
  Velocity    *vel  = ECS_GET(world, e, Velocity,    COMP_VELOCITY);
  Orientation *ori  = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
  NavPath     *path = ECS_GET(world, e, NavPath,     COMP_NAVPATH);

  if (!pos || !vel || !ori || !path) return true;

  if (path->count == 0) {
    vel->value.x = 0.0f;
    vel->value.z = 0.0f;
    return true;
  }

  if (path->currentIndex >= path->count) {
    vel->value.x    = 0.0f;
    vel->value.z    = 0.0f;
    path->count     = 0;
    path->currentIndex = 0;
    return true;
  }

  // Snap to terrain
  pos->value.y = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap,
                                               pos->value.x, pos->value.z);

  Vector3 target   = path->points[path->currentIndex];
  Vector3 toTarget = Vector3Subtract(target, pos->value);
  toTarget.y       = 0.0f;
  float distance   = Vector3Length(toTarget);

  if (distance < ARRIVE_THRESHOLD) {
    path->currentIndex++;
    if (path->currentIndex >= path->count) {
      vel->value.x    = 0.0f;
      vel->value.z    = 0.0f;
      path->count     = 0;
      path->currentIndex = 0;
      return true;
    }
    return false;
  }

  // Rotate body toward current waypoint
  Vector3 dir      = Vector3Normalize(toTarget);
  float targetYaw  = atan2f(dir.x, dir.z);
  float delta      = targetYaw - ori->yaw;
  while (delta >  PI) delta -= 2.0f * PI;
  while (delta < -PI) delta += 2.0f * PI;

  float maxStep = rotateSpeed * dt;
  if (fabsf(delta) <= maxStep)
    ori->yaw = targetYaw;
  else
    ori->yaw += (delta > 0.0f ? 1.0f : -1.0f) * maxStep;

  // Speed: decelerate approaching the final waypoint, accelerate from rest
  float remaining   = PathRemainingLength(path, pos);
  float speedFactor = (remaining < ENEMY_DECEL_DIST)
                          ? fmaxf(ENEMY_MIN_SPEED_FACTOR, remaining / ENEMY_DECEL_DIST)
                          : 1.0f;
  float targetSpeed = maxSpeed * speedFactor;

  float curSpeedSq = vel->value.x * vel->value.x + vel->value.z * vel->value.z;
  float curSpeed   = sqrtf(curSpeedSq);
  float accelRate  = maxSpeed / 0.5f; // reach full speed in ~0.5s
  float newSpeed   = (curSpeed < targetSpeed)
                         ? fminf(curSpeed + accelRate * dt, targetSpeed)
                         : targetSpeed;

  if (fabsf(delta) < FACE_THRESHOLD) {
    vel->value.x = dir.x * newSpeed;
    vel->value.z = dir.z * newSpeed;
  } else {
    vel->value.x = 0.0f;
    vel->value.z = 0.0f;
  }

  return false;
}

/* ------------------------------------------------------------------ */
/*  Grunt state machine                                                */
/* ------------------------------------------------------------------ */

void EnemyGruntAISystem(world_t *world, GameWorld *game,
                        archetype_t *enemyArch, float dt) {
  const float minDist        = 5.0f;
  const float maxDist        = 100.0f;
  const float repathInterval = 5.0f;

  Position *playerPos = ECS_GET(world, game->player, Position, COMP_POSITION);
  if (!playerPos) return;

  for (uint32_t i = 0; i < enemyArch->count; i++) {
    entity_t e = enemyArch->entities[i];

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value) continue;

    Position      *pos         = ECS_GET(world, e, Position,      COMP_POSITION);
    NavPath       *path        = ECS_GET(world, e, NavPath,       COMP_NAVPATH);
    Timer         *repathTimer = ECS_GET(world, e, Timer,         COMP_MOVE_TIMER);
    CombatState_t *combat      = ECS_GET(world, e, CombatState_t, COMP_COMBAT_STATE);
    if (!pos || !path || !repathTimer || !combat) continue;

    if (repathTimer->value > 0.0f) repathTimer->value -= dt;
    if (combat->state == ENEMY_STATE_COMBAT && combat->settleTimer > 0.0f)
      combat->settleTimer -= dt;

    Vector3 toPlayer = Vector3Subtract(playerPos->value, pos->value);
    toPlayer.y = 0.0f;
    float distToPlayer = Vector3Length(toPlayer);

    switch (combat->state) {

    case ENEMY_STATE_MOVING:
      if (!combat->pathPending) {
        bool done = EnemyFollowPath(world, game, e,
                                    moveSpeeds[0], rotateSpeeds[0], dt);
        if (done) {
          combat->state       = ENEMY_STATE_COMBAT;
          combat->settleTimer = ENEMY_SETTLE_TIME;
        }
      }
      // while pathPending: stand still, wait for queue to deliver the path
      break;

    case ENEMY_STATE_COMBAT:
      if ((distToPlayer < minDist || distToPlayer > maxDist) &&
          repathTimer->value <= 0.0f && !combat->pathPending) {
        for (int attempt = 0; attempt < 10; attempt++) {
          float angle  = GetRandomValue(0, 360) * DEG2RAD;
          float radius = (float)GetRandomValue((int)minDist, (int)maxDist);
          Vector3 cand = {playerPos->value.x + cosf(angle) * radius, 0.0f,
                          playerPos->value.z + sinf(angle) * radius};
          cand.y = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap,
                                                  cand.x, cand.z);
          int cx, cy;
          if (!NavGrid_WorldToCell(&game->navGrid, cand, &cx, &cy)) continue;
          if (game->navGrid.cells[NavGrid_Index(&game->navGrid, cx, cy)].type
              == NAV_CELL_WALL) continue;

          if (EnemyPathQueue_Submit(&game->navGrid, pos->value, cand, path,
                                    &combat->pathPending, combat, e)) {
            repathTimer->value = repathInterval;
            break;
          }
        }
      }
      break;

    default:
      combat->state = ENEMY_STATE_COMBAT;
      break;
    }
  }
}

/* ------------------------------------------------------------------ */
/*  Ranger state machine                                               */
/* ------------------------------------------------------------------ */

void EnemyRangerAISystem(world_t *world, GameWorld *game,
                         archetype_t *enemyArch, float dt) {
  const float minDist        = 10.0f;
  const float maxDist        = 250.0f;
  const float repathInterval = 8.0f;

  Position *playerPos = ECS_GET(world, game->player, Position, COMP_POSITION);
  if (!playerPos) return;

  for (uint32_t i = 0; i < enemyArch->count; i++) {
    entity_t e = enemyArch->entities[i];

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value) continue;

    Position      *pos         = ECS_GET(world, e, Position,      COMP_POSITION);
    NavPath       *path        = ECS_GET(world, e, NavPath,       COMP_NAVPATH);
    Timer         *repathTimer = ECS_GET(world, e, Timer,         COMP_MOVE_TIMER);
    CombatState_t *combat      = ECS_GET(world, e, CombatState_t, COMP_COMBAT_STATE);
    if (!pos || !path || !repathTimer || !combat) continue;

    if (repathTimer->value > 0.0f) repathTimer->value -= dt;
    if (combat->state == ENEMY_STATE_COMBAT && combat->settleTimer > 0.0f)
      combat->settleTimer -= dt;

    Vector3 toPlayer = Vector3Subtract(playerPos->value, pos->value);
    toPlayer.y = 0.0f;
    float distToPlayer = Vector3Length(toPlayer);

    switch (combat->state) {

    case ENEMY_STATE_MOVING:
      if (!combat->pathPending) {
        bool done = EnemyFollowPath(world, game, e,
                                    moveSpeeds[1], rotateSpeeds[1], dt);
        if (done) {
          combat->state       = ENEMY_STATE_COMBAT;
          combat->settleTimer = ENEMY_SETTLE_TIME;
        }
      }
      break;

    case ENEMY_STATE_COMBAT:
      if ((distToPlayer < minDist || distToPlayer > maxDist) &&
          repathTimer->value <= 0.0f && !combat->pathPending) {
        Vector3 dir         = Vector3Normalize(toPlayer);
        float desiredDist   = (minDist + maxDist) * 0.5f;
        Vector3 ringTarget  = Vector3Subtract(playerPos->value,
                                              Vector3Scale(dir, desiredDist));
        ringTarget.y = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap,
                                                      ringTarget.x, ringTarget.z);
        if (EnemyPathQueue_Submit(&game->navGrid, pos->value, ringTarget, path,
                                  &combat->pathPending, combat, e)) {
          repathTimer->value = repathInterval;
        }
      }
      break;

    default:
      combat->state = ENEMY_STATE_COMBAT;
      break;
    }
  }
}
