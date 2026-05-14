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
  // Waypoints have Y=0 (from NavGrid_CellCenter); use XZ-only distance
  // so terrain height doesn't inflate the remaining length and break decel.
  Vector3 posXZ = {pos->value.x, 0.0f, pos->value.z};
  Vector3 wpXZ  = {path->points[path->currentIndex].x, 0.0f,
                   path->points[path->currentIndex].z};
  float total = Vector3Distance(posXZ, wpXZ);
  for (int i = path->currentIndex; i < path->count - 1; i++) {
    Vector3 a = {path->points[i].x,   0.0f, path->points[i].z};
    Vector3 b = {path->points[i+1].x, 0.0f, path->points[i+1].z};
    total += Vector3Distance(a, b);
  }
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
/*  Claim table — soft spread-out (enemies avoid each other's spots)  */
/* ------------------------------------------------------------------ */

#define CLAIM_CAP 64
typedef struct { entity_t entity; int cx, cy; } Claim;
static Claim s_claims[CLAIM_CAP];
static int   s_claimCount = 0;

static void ClaimPurgeDeadEntities(world_t *world) {
  int i = 0;
  while (i < s_claimCount) {
    Active *a = ECS_GET(world, s_claims[i].entity, Active, COMP_ACTIVE);
    if (!a || !a->value) { s_claims[i] = s_claims[--s_claimCount]; }
    else                 { i++; }
  }
}

static void ClaimRelease(uint32_t id) {
  for (int i = 0; i < s_claimCount; i++) {
    if (s_claims[i].entity.id == id) { s_claims[i] = s_claims[--s_claimCount]; return; }
  }
}

static void ClaimAcquire(entity_t e, int cx, int cy) {
  ClaimRelease(e.id);
  if (s_claimCount < CLAIM_CAP)
    s_claims[s_claimCount++] = (Claim){e, cx, cy};
}

static float ClaimNearestDist(uint32_t selfId, int cx, int cy) {
  float minD = 9999.0f;
  for (int i = 0; i < s_claimCount; i++) {
    if (s_claims[i].entity.id == selfId) continue;
    float dx = (float)(s_claims[i].cx - cx);
    float dy = (float)(s_claims[i].cy - cy);
    float d = sqrtf(dx*dx + dy*dy);
    if (d < minD) minD = d;
  }
  return minD;
}

/* ------------------------------------------------------------------ */
/*  Position scoring                                                   */
/* ------------------------------------------------------------------ */

static float ScorePosition(NavGrid *grid, int cx, int cy,
                            float distToPlayer, float selfDistToPlayer,
                            float minDist, float maxDist,
                            uint32_t selfId, bool isRanger) {
  NavCellType type = grid->cells[NavGrid_Index(grid, cx, cy)].type;
  if (type == NAV_CELL_WALL || type == NAV_CELL_BLOCKED ||
      type == NAV_CELL_FENCE) return -9999.0f;

  /* Hard exclusion: refuse any cell that overlaps another claim */
  float nearDist = ClaimNearestDist(selfId, cx, cy);
  if (nearDist < 3.5f) return -9999.0f;

  float score = 0.0f;
  if (!isRanger) {
    if (type == NAV_CELL_COVER_LOW)  score += 35.0f;
    if (type == NAV_CELL_COVER_HIGH) score += 25.0f;
    if (type == NAV_CELL_FLANK)      score += 30.0f;
    if (type == NAV_CELL_SNIPE)      score +=  5.0f;
  } else {
    if (type == NAV_CELL_SNIPE)      score += 50.0f;
    if (type == NAV_CELL_COVER_HIGH) score += 10.0f;
    if (type == NAV_CELL_COVER_LOW)  score +=  5.0f;
    if (type == NAV_CELL_FLANK)      score -= 10.0f;
  }
  /* Combat range bonus */
  if (distToPlayer >= minDist && distToPlayer <= maxDist) score += 20.0f;
  /* Soft spread bonus */
  if (nearDist > 8.0f) score += 10.0f;
  if (nearDist < 6.0f) score -= 15.0f;
  /* Drift: reward positions that bring us a bit closer to the player */
  if (distToPlayer < selfDistToPlayer - 5.0f) score += 15.0f;
  return score;
}

/* ------------------------------------------------------------------ */
/*  Tactical position selector                                         */
/* ------------------------------------------------------------------ */

static bool SelectTacticalPosition(world_t *world, GameWorld *game,
                                    Vector3 from, Vector3 playerPos,
                                    float minDist, float maxDist,
                                    float maxMoveRadius,
                                    uint32_t selfId, bool isRanger,
                                    Vector3 *outPos) {
  NavGrid *grid = &game->navGrid;
  /* Search centered on THIS ENEMY so moves are local — prevents cross-map walks */
  int eCX, eCY;
  if (!NavGrid_WorldToCell(grid, from, &eCX, &eCY)) return false;

  float selfDistToPlayer = sqrtf((from.x-playerPos.x)*(from.x-playerPos.x) +
                                  (from.z-playerPos.z)*(from.z-playerPos.z));
  int searchR = (int)(maxMoveRadius / grid->cellSize) + 1;
  if (searchR > TACTICAL_SEARCH_RADIUS) searchR = TACTICAL_SEARCH_RADIUS;

  float bestScore = -9998.0f;
  Vector3 bestPos = {0};
  bool found = false;

  for (int dy = -searchR; dy <= searchR; dy++) {
    for (int dx = -searchR; dx <= searchR; dx++) {
      int cx = eCX + dx, cy = eCY + dy;
      if (!NavGrid_InBounds(grid, cx, cy)) continue;

      Vector3 cPos = NavGrid_CellCenter(grid, cx, cy);
      /* Limit to move radius (circular) */
      float moveDist = sqrtf((cPos.x-from.x)*(cPos.x-from.x)+(cPos.z-from.z)*(cPos.z-from.z));
      if (moveDist > maxMoveRadius) continue;
      /* Arena bounds */
      if (sqrtf(cPos.x*cPos.x + cPos.z*cPos.z) > game->arenaRadius) continue;

      float dist = sqrtf((cPos.x-playerPos.x)*(cPos.x-playerPos.x) +
                         (cPos.z-playerPos.z)*(cPos.z-playerPos.z));
      float score = ScorePosition(grid, cx, cy, dist, selfDistToPlayer,
                                   minDist, maxDist, selfId, isRanger);
      if (score > bestScore) {
        bestScore = score;
        cPos.y = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap, cPos.x, cPos.z);
        bestPos = cPos;
        found = true;
      }
    }
  }

  /* Fallback: random candidates near THIS ENEMY */
  if (!found) {
    for (int attempt = 0; attempt < 12; attempt++) {
      float angle  = GetRandomValue(0, 360) * DEG2RAD;
      float radius = (float)GetRandomValue(5, (int)maxMoveRadius);
      Vector3 cand = {from.x + cosf(angle)*radius, 0.0f, from.z + sinf(angle)*radius};
      if (sqrtf(cand.x*cand.x + cand.z*cand.z) > game->arenaRadius) continue;
      cand.y = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap, cand.x, cand.z);
      int cx, cy;
      if (!NavGrid_WorldToCell(grid, cand, &cx, &cy)) continue;
      float dist  = sqrtf((cand.x-playerPos.x)*(cand.x-playerPos.x) +
                          (cand.z-playerPos.z)*(cand.z-playerPos.z));
      float score = ScorePosition(grid, cx, cy, dist, selfDistToPlayer,
                                   minDist, maxDist, selfId, isRanger);
      if (score > bestScore) { bestScore = score; bestPos = cand; found = true; }
    }
  }

  if (found) *outPos = bestPos;
  return found;
}

static bool SelectRetreatPosition(world_t *world, GameWorld *game,
                                   Vector3 from, Vector3 playerPos,
                                   bool isRanger, Vector3 *outPos) {
  NavGrid *grid = &game->navGrid;
  /* Search centered on THIS ENEMY — retreat is relative to where we are */
  int eCX, eCY;
  if (!NavGrid_WorldToCell(grid, from, &eCX, &eCY)) return false;

  float selfDist   = sqrtf((from.x-playerPos.x)*(from.x-playerPos.x)+
                            (from.z-playerPos.z)*(from.z-playerPos.z));
  float maxMoveR   = isRanger ? RANGER_MAX_MOVE_RADIUS : GRUNT_MAX_MOVE_RADIUS;
  int   searchR    = (int)(maxMoveR / grid->cellSize) + 1;
  if (searchR > TACTICAL_SEARCH_RADIUS) searchR = TACTICAL_SEARCH_RADIUS;

  float bestScore = -9999.0f;
  Vector3 bestPos = {0};
  bool found = false;

  for (int dy = -searchR; dy <= searchR; dy++) {
    for (int dx = -searchR; dx <= searchR; dx++) {
      int cx = eCX + dx, cy = eCY + dy;
      if (!NavGrid_InBounds(grid, cx, cy)) continue;
      NavCellType t = grid->cells[NavGrid_Index(grid, cx, cy)].type;
      if (t == NAV_CELL_WALL || t == NAV_CELL_BLOCKED || t == NAV_CELL_FENCE) continue;

      Vector3 cPos = NavGrid_CellCenter(grid, cx, cy);
      if (sqrtf(cPos.x*cPos.x + cPos.z*cPos.z) > game->arenaRadius) continue;

      /* Within move radius */
      float moveDist = sqrtf((cPos.x-from.x)*(cPos.x-from.x)+(cPos.z-from.z)*(cPos.z-from.z));
      if (moveDist > maxMoveR) continue;

      float dist = sqrtf((cPos.x-playerPos.x)*(cPos.x-playerPos.x)+
                         (cPos.z-playerPos.z)*(cPos.z-playerPos.z));
      if (dist < selfDist - 2.0f) continue; /* must move away from player */

      float score = 0.0f;
      if (!isRanger) {
        if (t == NAV_CELL_COVER_HIGH) score += 60.0f;
        else if (t == NAV_CELL_COVER_LOW) score += 30.0f;
      } else {
        if (t == NAV_CELL_SNIPE)      score += 50.0f;
      }
      score += (dist - selfDist) * 0.8f;
      if (ClaimNearestDist(0xFFFFFFFFu, cx, cy) < 3.5f) score -= 30.0f;

      if (score > bestScore) {
        bestScore = score;
        cPos.y = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap, cPos.x, cPos.z);
        bestPos = cPos;
        found = true;
      }
    }
  }

  if (!found) {
    /* Fallback: step directly away, trying progressively shorter distances */
    float fdx = from.x - playerPos.x, fdz = from.z - playerPos.z;
    float len  = sqrtf(fdx*fdx + fdz*fdz);
    if (len < 0.001f) return false;
    fdx /= len; fdz /= len;
    for (float step = maxMoveR; step >= 5.0f; step -= 5.0f) {
      Vector3 cand = {from.x + fdx*step, 0.0f, from.z + fdz*step};
      float flatR  = sqrtf(cand.x*cand.x + cand.z*cand.z);
      if (flatR > game->arenaRadius * 0.97f) {
        float s = game->arenaRadius * 0.97f / flatR;
        cand.x *= s; cand.z *= s;
      }
      cand.y = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap, cand.x, cand.z);
      int cx2, cy2;
      if (!NavGrid_WorldToCell(grid, cand, &cx2, &cy2)) continue;
      NavCellType t2 = grid->cells[NavGrid_Index(grid, cx2, cy2)].type;
      if (t2 == NAV_CELL_WALL || t2 == NAV_CELL_BLOCKED || t2 == NAV_CELL_FENCE) continue;
      bestPos = cand; found = true; break;
    }
  }

  if (found) *outPos = bestPos;
  return found;
}

/* Helper: set state after path arrival based on current cell type */
static void ArriveAtPosition(world_t *world, GameWorld *game, entity_t e,
                              Position *pos, CombatState_t *combat,
                              float repBase, float repJitter) {
  int cx, cy;
  NavCellType type = NAV_CELL_EMPTY;
  if (NavGrid_WorldToCell(&game->navGrid, pos->value, &cx, &cy)) {
    type = game->navGrid.cells[NavGrid_Index(&game->navGrid, cx, cy)].type;
    ClaimAcquire(e, cx, cy);
    combat->claimedCX = (int16_t)cx;
    combat->claimedCY = (int16_t)cy;
  }
  combat->state = (type == NAV_CELL_COVER_LOW || type == NAV_CELL_COVER_HIGH)
                  ? ENEMY_AI_COVER : ENEMY_AI_SUPPRESS;
  combat->settleTimer = ENEMY_SETTLE_TIME;
  float jitter = GetRandomValue(0, (int)(repJitter * 10)) * 0.1f;
  combat->repositionTimer = repBase + jitter;
}

/* ------------------------------------------------------------------ */
/*  Grunt state machine                                                */
/* ------------------------------------------------------------------ */

void EnemyGruntAISystem(world_t *world, GameWorld *game,
                        archetype_t *enemyArch, float dt) {
  Position *playerPos = ECS_GET(world, game->player, Position, COMP_POSITION);
  if (!playerPos) return;

  ClaimPurgeDeadEntities(world);

  for (uint32_t i = 0; i < enemyArch->count; i++) {
    entity_t e = enemyArch->entities[i];

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value) continue;

    Position      *pos    = ECS_GET(world, e, Position,      COMP_POSITION);
    NavPath       *path   = ECS_GET(world, e, NavPath,       COMP_NAVPATH);
    CombatState_t *combat = ECS_GET(world, e, CombatState_t, COMP_COMBAT_STATE);
    Health        *health = ECS_GET(world, e, Health,        COMP_HEALTH);
    if (!pos || !path || !combat) continue;

    /* Tick timers */
    if (combat->settleTimer > 0.0f)    combat->settleTimer    -= dt;
    combat->repositionTimer            -= dt;
    combat->losCheckTimer              -= dt;

    /* LOS check (throttled) */
    if (combat->losCheckTimer <= 0.0f) {
      combat->hasLOS       = NavGrid_HasLOS(&game->navGrid, pos->value, playerPos->value);
      combat->losCheckTimer = GRUNT_LOS_CHECK_INTERVAL;
    }

    float dx = playerPos->value.x - pos->value.x;
    float dz = playerPos->value.z - pos->value.z;
    float distToPlayer = sqrtf(dx*dx + dz*dz);
    float hpFrac = (health && health->max > 0.0f) ? health->current / health->max : 1.0f;
    bool  shouldRetreat = (distToPlayer < GRUNT_PANIC_DIST) || (hpFrac < GRUNT_LOW_HP_FRAC);

    switch (combat->state) {

    /* ---- Pathing toward a tactical position ---- */
    case ENEMY_AI_ADVANCE:
    case ENEMY_AI_REPOSITION: {
      if (shouldRetreat && !combat->pathPending) {
        NavPath_Clear(path);
        ClaimRelease(e.id);
        Vector3 dest;
        if (SelectRetreatPosition(world, game, pos->value, playerPos->value, false, &dest))
          EnemyPathQueue_Submit(&game->navGrid, pos->value, dest, path, &combat->pathPending, NULL, e);
        combat->state = ENEMY_AI_RETREAT;
        break;
      }
      if (combat->pathPending) {
        pos->value.y = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap,
                                                     pos->value.x, pos->value.z);
        Velocity *vel = ECS_GET(world, e, Velocity, COMP_VELOCITY);
        if (vel) { vel->value.x = 0.0f; vel->value.z = 0.0f; }
        break;
      }
      bool arrived = EnemyFollowPath(world, game, e, moveSpeeds[0], rotateSpeeds[0], dt);
      if (arrived)
        ArriveAtPosition(world, game, e, pos, combat, GRUNT_REPOSITION_BASE, GRUNT_REPOSITION_JITTER);
      break;
    }

    /* ---- Holding position and firing ---- */
    case ENEMY_AI_SUPPRESS:
    case ENEMY_AI_COVER: {
      if (shouldRetreat) {
        NavPath_Clear(path);
        ClaimRelease(e.id);
        Vector3 dest;
        if (SelectRetreatPosition(world, game, pos->value, playerPos->value, false, &dest))
          EnemyPathQueue_Submit(&game->navGrid, pos->value, dest, path, &combat->pathPending, NULL, e);
        combat->state = ENEMY_AI_RETREAT;
        break;
      }
      /* Shorten reposition timer if LOS is blocked */
      if (!combat->hasLOS && combat->repositionTimer > GRUNT_LOS_REPOSITION)
        combat->repositionTimer = GRUNT_LOS_REPOSITION;
      if (combat->repositionTimer <= 0.0f && !combat->pathPending) {
        ClaimRelease(e.id);
        Vector3 dest;
        if (SelectTacticalPosition(world, game, pos->value, playerPos->value,
                                   GRUNT_MIN_DIST, GRUNT_MAX_DIST, GRUNT_MAX_MOVE_RADIUS, e.id, false, &dest)) {
          EnemyPathQueue_Submit(&game->navGrid, pos->value, dest, path, &combat->pathPending, NULL, e);
          combat->state = ENEMY_AI_REPOSITION;
        } else {
          combat->repositionTimer = GRUNT_REPOSITION_BASE;
        }
      }
      break;
    }

    /* ---- Retreating to cover ---- */
    case ENEMY_AI_RETREAT: {
      pos->value.y = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap,
                                                   pos->value.x, pos->value.z);
      if (combat->pathPending) {
        Velocity *vel = ECS_GET(world, e, Velocity, COMP_VELOCITY);
        if (vel) { vel->value.x = 0.0f; vel->value.z = 0.0f; }
        break;
      }
      /* If we're safe again, stop retreating */
      if (distToPlayer > GRUNT_PANIC_DIST * 2.5f && hpFrac > GRUNT_LOW_HP_FRAC + 0.05f) {
        ArriveAtPosition(world, game, e, pos, combat, GRUNT_REPOSITION_BASE, GRUNT_REPOSITION_JITTER);
        break;
      }
      bool arrived = EnemyFollowPath(world, game, e, moveSpeeds[0] * 1.25f, rotateSpeeds[0], dt);
      if (arrived)
        ArriveAtPosition(world, game, e, pos, combat, GRUNT_REPOSITION_BASE, GRUNT_REPOSITION_JITTER);
      break;
    }

    /* ---- Default / legacy ---- */
    default: {
      Vector3 dest;
      if (SelectTacticalPosition(world, game, pos->value, playerPos->value,
                                 GRUNT_MIN_DIST, GRUNT_MAX_DIST, GRUNT_MAX_MOVE_RADIUS, e.id, false, &dest)) {
        EnemyPathQueue_Submit(&game->navGrid, pos->value, dest, path, &combat->pathPending, NULL, e);
      }
      combat->state           = ENEMY_AI_ADVANCE;
      combat->repositionTimer = GRUNT_REPOSITION_BASE;
      break;
    }
    }
  }
}

/* ------------------------------------------------------------------ */
/*  Ranger state machine                                               */
/* ------------------------------------------------------------------ */

void EnemyRangerAISystem(world_t *world, GameWorld *game,
                         archetype_t *enemyArch, float dt) {
  Position *playerPos = ECS_GET(world, game->player, Position, COMP_POSITION);
  if (!playerPos) return;

  /* Claims are shared — no second purge needed if grunt ran first */

  for (uint32_t i = 0; i < enemyArch->count; i++) {
    entity_t e = enemyArch->entities[i];

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value) continue;

    Position      *pos    = ECS_GET(world, e, Position,      COMP_POSITION);
    NavPath       *path   = ECS_GET(world, e, NavPath,       COMP_NAVPATH);
    CombatState_t *combat = ECS_GET(world, e, CombatState_t, COMP_COMBAT_STATE);
    Health        *health = ECS_GET(world, e, Health,        COMP_HEALTH);
    if (!pos || !path || !combat) continue;

    if (combat->settleTimer > 0.0f)    combat->settleTimer    -= dt;
    combat->repositionTimer            -= dt;
    combat->losCheckTimer              -= dt;

    if (combat->losCheckTimer <= 0.0f) {
      combat->hasLOS        = NavGrid_HasLOS(&game->navGrid, pos->value, playerPos->value);
      combat->losCheckTimer = RANGER_LOS_CHECK_INTERVAL;
    }

    float dx = playerPos->value.x - pos->value.x;
    float dz = playerPos->value.z - pos->value.z;
    float distToPlayer = sqrtf(dx*dx + dz*dz);
    float hpFrac = (health && health->max > 0.0f) ? health->current / health->max : 1.0f;
    bool  shouldRetreat = (distToPlayer < RANGER_PANIC_DIST) || (hpFrac < RANGER_LOW_HP_FRAC);

    switch (combat->state) {

    case ENEMY_AI_ADVANCE:
    case ENEMY_AI_REPOSITION: {
      if (shouldRetreat && !combat->pathPending) {
        NavPath_Clear(path);
        ClaimRelease(e.id);
        Vector3 dest;
        if (SelectRetreatPosition(world, game, pos->value, playerPos->value, true, &dest))
          EnemyPathQueue_Submit(&game->navGrid, pos->value, dest, path, &combat->pathPending, NULL, e);
        combat->state = ENEMY_AI_RETREAT;
        break;
      }
      if (combat->pathPending) {
        pos->value.y = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap,
                                                     pos->value.x, pos->value.z);
        Velocity *vel = ECS_GET(world, e, Velocity, COMP_VELOCITY);
        if (vel) { vel->value.x = 0.0f; vel->value.z = 0.0f; }
        break;
      }
      bool arrived = EnemyFollowPath(world, game, e, moveSpeeds[1], rotateSpeeds[1], dt);
      if (arrived)
        ArriveAtPosition(world, game, e, pos, combat, RANGER_REPOSITION_BASE, RANGER_REPOSITION_JITTER);
      break;
    }

    case ENEMY_AI_SUPPRESS:
    case ENEMY_AI_COVER: {
      if (shouldRetreat) {
        NavPath_Clear(path);
        ClaimRelease(e.id);
        Vector3 dest;
        if (SelectRetreatPosition(world, game, pos->value, playerPos->value, true, &dest))
          EnemyPathQueue_Submit(&game->navGrid, pos->value, dest, path, &combat->pathPending, NULL, e);
        combat->state = ENEMY_AI_RETREAT;
        break;
      }
      if (!combat->hasLOS && combat->repositionTimer > RANGER_LOS_REPOSITION)
        combat->repositionTimer = RANGER_LOS_REPOSITION;
      if (combat->repositionTimer <= 0.0f && !combat->pathPending) {
        /* Also reposition if player too close or too far */
        bool outOfRange = distToPlayer < RANGER_MIN_DIST || distToPlayer > RANGER_MAX_DIST;
        if (outOfRange || combat->repositionTimer <= 0.0f) {
          ClaimRelease(e.id);
          Vector3 dest;
          if (SelectTacticalPosition(world, game, pos->value, playerPos->value,
                                     RANGER_MIN_DIST, RANGER_MAX_DIST, RANGER_MAX_MOVE_RADIUS, e.id, true, &dest)) {
            EnemyPathQueue_Submit(&game->navGrid, pos->value, dest, path, &combat->pathPending, NULL, e);
            combat->state = ENEMY_AI_REPOSITION;
          } else {
            combat->repositionTimer = RANGER_REPOSITION_BASE;
          }
        }
      }
      break;
    }

    case ENEMY_AI_RETREAT: {
      pos->value.y = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap,
                                                   pos->value.x, pos->value.z);
      if (combat->pathPending) {
        Velocity *vel = ECS_GET(world, e, Velocity, COMP_VELOCITY);
        if (vel) { vel->value.x = 0.0f; vel->value.z = 0.0f; }
        break;
      }
      if (distToPlayer > RANGER_PANIC_DIST * 2.0f && hpFrac > RANGER_LOW_HP_FRAC + 0.05f) {
        ArriveAtPosition(world, game, e, pos, combat, RANGER_REPOSITION_BASE, RANGER_REPOSITION_JITTER);
        break;
      }
      bool arrived = EnemyFollowPath(world, game, e, moveSpeeds[1] * 1.3f, rotateSpeeds[1], dt);
      if (arrived)
        ArriveAtPosition(world, game, e, pos, combat, RANGER_REPOSITION_BASE, RANGER_REPOSITION_JITTER);
      break;
    }

    default: {
      Vector3 dest;
      if (SelectTacticalPosition(world, game, pos->value, playerPos->value,
                                 RANGER_MIN_DIST, RANGER_MAX_DIST, RANGER_MAX_MOVE_RADIUS, e.id, true, &dest)) {
        EnemyPathQueue_Submit(&game->navGrid, pos->value, dest, path, &combat->pathPending, NULL, e);
      }
      combat->state           = ENEMY_AI_ADVANCE;
      combat->repositionTimer = RANGER_REPOSITION_BASE;
      break;
    }
    }
  }
}

/* ------------------------------------------------------------------ */
/*  Out-of-bounds damage — kills enemies that escape the 200-unit     */
/*  XZ radius so stray units bleed out instead of roaming forever.    */
/* ------------------------------------------------------------------ */

#define OOB_RADIUS       200.0f
#define OOB_DAMAGE_RATE   20.0f   // HP per second

void OutOfBoundsSystem(world_t *world, GameWorld *game, float dt) {
  uint32_t archIds[] = {
    game->enemyGruntArchId,
    game->enemyRangerArchId,
    game->enemyMeleeArchId,
    game->enemyDroneArchId,
  };
  float r2 = OOB_RADIUS * OOB_RADIUS;
  for (int ai = 0; ai < 4; ai++) {
    archetype_t *arch = WorldGetArchetype(world, archIds[ai]);
    if (!arch) continue;
    for (uint32_t i = 0; i < arch->count; i++) {
      entity_t e = arch->entities[i];
      Active   *act = ECS_GET(world, e, Active,   COMP_ACTIVE);
      if (!act || !act->value) continue;
      Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
      Health   *hp  = ECS_GET(world, e, Health,   COMP_HEALTH);
      if (!pos || !hp) continue;
      if (pos->value.x * pos->value.x + pos->value.z * pos->value.z > r2) {
        hp->current -= OOB_DAMAGE_RATE * dt;
        if (hp->current <= 0.0f)
          TryKillEntity(world, e);
      }
    }
  }
}
