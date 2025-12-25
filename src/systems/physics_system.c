#include "systems.h"

#define GRAVITY 20.0f // units per second2
#define TERMINAL_VELOCITY 50.0f

#define ENTITY_FEET_OFFSET 10.0f // how high the entity "stands" above terrain

//----------------------------------------
// Terrain Collision: Keep entity on ground
//----------------------------------------
// Bilinear intrepolates between the 4 nearest cells of the heightmap
float GetTerrainHeightAtXZ(Terrain_t *terrain, float wx, float wz) {
  // terrain origin in world space
  float minX = terrain->minX;
  float minZ = terrain->minZ;

  // size of one cell in world units
  float dx = terrain->cellSizeX;
  float dz = terrain->cellSizeZ;

  // map world coordinates to floating-point heightmap coordinates
  // fx and fz indicate **exact position inside the grid**, not just integers
  float fx = (wx - minX) / dx;
  float fz = (wz - minZ) / dz;

  // integer indices of the top-left corner of the cell containing (wx, wz)
  int ix = (int)floorf(fx);
  int iz = (int)floorf(fz);

  // clamp indices to avoid reading outside the heightmap
  // we subtract 1 because we'll access (ix+1, iz+1) later
  if (ix < 0)
    ix = 0;
  if (iz < 0)
    iz = 0;
  if (ix >= terrain->hmWidth - 1)
    ix = terrain->hmWidth - 2;
  if (iz >= terrain->hmHeight - 1)
    iz = terrain->hmHeight - 2;

  // fractional distance inside the cell along X and Z axes
  // ranges from 0 to 1
  float tx = fx - ix;
  float tz = fz - iz;

  // sample the four corners of the cell
  float h00 = terrain->height[iz * terrain->hmWidth + ix];       // top-left
  float h10 = terrain->height[iz * terrain->hmWidth + (ix + 1)]; // top-right
  float h01 = terrain->height[(iz + 1) * terrain->hmWidth + ix]; // bottom-left
  float h11 =
      terrain->height[(iz + 1) * terrain->hmWidth + (ix + 1)]; // bottom-right

  // linear interpolation along X for top and bottom rows
  float h0 = h00 * (1.0f - tx) + h10 * tx; // top row
  float h1 = h01 * (1.0f - tx) + h11 * tx; // bottom row

  // linear interpolation along Z between top and bottom
  float h = h0 * (1.0f - tz) + h1 * tz;

  return h; // interpolated height at (wx, wz)
}

static float GetTerrainHeightAtEntity(Engine_t *eng, GameState_t *gs,
                                      Terrain_t *terrain, entity_t entity) {
  Vector3 pos =
      *(Vector3 *)getComponent(&eng->actors, entity, gs->compReg.cid_Positions);
  return GetTerrainHeightAtXZ(terrain, pos.x, pos.z);
}

void ApplyTerrainCollision(Engine_t *eng, GameState_t *gs, Terrain_t *terrain,
                           int entityId, float dt) {

  Vector3 *pos = (Vector3 *)getComponent(&eng->actors, entityId,
                                         gs->compReg.cid_Positions);
  Vector3 *vel = (Vector3 *)getComponent(&eng->actors, entityId,
                                         gs->compReg.cid_velocities);

  // Apply gravity
  vel->y -= GRAVITY * dt;
  if (vel->y < -TERMINAL_VELOCITY)
    vel->y = -TERMINAL_VELOCITY;

  // Move entity
  pos->y += vel->y * dt;

  // Compute terrain height
  float terrainY = GetTerrainHeightAtEntity(eng, gs, terrain, entityId);

  // Clamp above terrain + offset
  float desiredY = terrainY + ENTITY_FEET_OFFSET;
  if (pos->y < desiredY) {
    pos->y = desiredY;
    vel->y = 0; // stop falling
  }
}

//----------------------------------------
// Update actor position with gravity, damping
//----------------------------------------
static void UpdateActorPosition(Engine_t *eng, GameState_t *gs, int entityId,
                                float dt) {
  // --- Lookup dynamic components ---
  Vector3 *pos = (Vector3 *)getComponent(&eng->actors, entityId,
                                         gs->compReg.cid_Positions);
  Vector3 *vel = (Vector3 *)getComponent(&eng->actors, entityId,
                                         gs->compReg.cid_velocities);
  Vector3 *prev = (Vector3 *)getComponent(&eng->actors, entityId,
                                          gs->compReg.cid_prevPositions);

  // Safety (during migration) — skip if not yet migrated
  if (!pos || !vel || !prev)
    return;

  // --- Store previous position ---
  *prev = *pos;

  // --- Apply gravity if the entity has the C_GRAVITY flag ---
  if (eng->em.masks[entityId] & C_GRAVITY) {
    // pos += vel * dt
    *pos = Vector3Add(*pos, Vector3Scale(*vel, dt));

    // Clamp to terrain and apply vertical collision
    ApplyTerrainCollision(eng, gs, &gs->terrain, entityId, dt);
  }

  // --- Horizontal damping ---
  vel->x *= 0.65f;
  vel->z *= 0.65f;
}

//----------------------------------------
// Actor vs Actor collisions (solid + trigger)
//----------------------------------------

static void ResolveActorCollisions(GameState_t *gs, Engine_t *eng) {
  int emCount = eng->em.count;
  Vector3 *pos =
      (Vector3 *)GetComponentArray(&eng->actors, gs->compReg.cid_Positions);
  Vector3 *vel =
      (Vector3 *)GetComponentArray(&eng->actors, gs->compReg.cid_velocities);

  for (int i = 0; i < emCount; i++) {
    if (!eng->em.alive[i])
      continue;

    EntityType_t typeI = eng->actors.types[i];
    if (!(typeI == ENTITY_PLAYER || typeI == ENTITY_MECH ||
          typeI == ENTITY_TANK))
      continue;

    // grid cell coordinates
    int cx = (int)((pos[i].x - gs->grid.minX) / gs->grid.cellSize);
    int cz = (int)((pos[i].z - gs->grid.minZ) / gs->grid.cellSize);

    for (int dx = -1; dx <= 1; dx++) {
      for (int dz = -1; dz <= 1; dz++) {
        int nx = cx + dx;
        int nz = cz + dz;
        if (!IsCellValid(&gs->grid, nx, nz))
          continue;

        GridNode_t *node = &gs->grid.nodes[nx][nz];
        for (int n = 0; n < node->count; n++) {
          entity_t e = node->entities[n];
          if (GetEntityCategory(e) != ET_ACTOR)
            continue;

          int j = GetEntityIndex(e);
          if (i == j || !eng->em.alive[j])
            continue;
          if ((eng->em.masks[j] & C_TRIGGER)) {
            continue;
          }
          // Check and resolve collision
          if (CheckAndResolveOBBCollision(
                  &pos[i], &eng->actors.collisionCollections[i], &pos[j],
                  &eng->actors.collisionCollections[j])) {

            Vector3 mtvDir = Vector3Normalize(Vector3Subtract(pos[i], pos[j]));
            float velDot = Vector3DotProduct(vel[i], mtvDir);
            if (velDot > 0.f)
              vel[i] = Vector3Subtract(vel[i], Vector3Scale(mtvDir, velDot));
          }
        }
      }
    }
  }
}

//----------------------------------------
// Actor vs Static collisions using grid
//----------------------------------------
static void ResolveActorStaticCollisions(GameState_t *gs, Engine_t *eng) {
  int emCount = eng->em.count;
  Vector3 *pos =
      (Vector3 *)GetComponentArray(&eng->actors, gs->compReg.cid_Positions);
  Vector3 *vel =
      (Vector3 *)GetComponentArray(&eng->actors, gs->compReg.cid_velocities);

  for (int i = 0; i < emCount; i++) {
    if (!eng->em.alive[i])
      continue;

    EntityType_t type = eng->actors.types[i];
    if (!(type == ENTITY_PLAYER || type == ENTITY_MECH || type == ENTITY_TANK))
      continue;

    // grid cell coordinates
    int cx = (int)((pos[i].x - gs->grid.minX) / gs->grid.cellSize);
    int cz = (int)((pos[i].z - gs->grid.minZ) / gs->grid.cellSize);

    for (int dx = -1; dx <= 1; dx++) {
      for (int dz = -1; dz <= 1; dz++) {
        int nx = cx + dx;
        int nz = cz + dz;
        if (!IsCellValid(&gs->grid, nx, nz))
          continue;

        GridNode_t *node = &gs->grid.nodes[nx][nz];
        for (int n = 0; n < node->count; n++) {
          entity_t e = node->entities[n];
          if (GetEntityCategory(e) != ET_STATIC)
            continue;

          int s = GetEntityIndex(e);

          if (!eng->statics.modelCollections[s].countModels)
            continue;

          // Check and resolve collision
          if (CheckAndResolveOBBCollision(
                  &pos[i], &eng->actors.collisionCollections[i],
                  &eng->statics.positions[s],
                  &eng->statics.collisionCollections[s])) {

            Vector3 mtvDir = Vector3Normalize(
                Vector3Subtract(pos[i], eng->statics.positions[s]));
            float velDot = Vector3DotProduct(vel[i], mtvDir);
            if (velDot > 0.f)
              vel[i] = Vector3Subtract(vel[i], Vector3Scale(mtvDir, velDot));
          }
        }
      }
    }
  }
}

static void ResolveTriggerEvents(GameState_t *gs, Engine_t *eng) {
  int emCount = eng->em.count;
  Vector3 *pos = GetComponentArray(&eng->actors, gs->compReg.cid_Positions);

  for (int i = 0; i < emCount; i++) {

    if (!eng->em.alive[i])
      continue;
    if (!(eng->em.masks[i] & C_TRIGGER))
      continue;

    // Trigger entity
    BehaviorCallBacks_t *cb =
        getComponent(&eng->actors, i, gs->compReg.cid_behavior);
    if (!cb)
      continue;

    bool someoneOverlapping = false;

    // Check if any actor overlaps the trigger
    for (int j = 0; j < emCount; j++) {

      if (i == j)
        continue;
      if (!eng->em.alive[j])
        continue;

      // Only react to things with a collision box
      if (!(eng->em.masks[j] & C_COLLISION))
        continue;

      bool overlap =
          CheckOBBOverlap(pos[i], &eng->actors.collisionCollections[i], pos[j],
                          &eng->actors.collisionCollections[j]);

      if (overlap) {
        someoneOverlapping = true;

        if (!cb->isColliding) {
          if (cb->onCollision)
            cb->onCollision(eng, gs, MakeEntityID(ET_ACTOR, i),
                            MakeEntityID(ET_ACTOR, j));
          cb->isColliding = true;
        }
      }
    }

    // If no one is overlapping but last frame was colliding → EXIT
    if (!someoneOverlapping && cb->isColliding) {
      if (cb->onCollisionExit)
        cb->onCollisionExit(eng, gs, MakeEntityID(ET_ACTOR, i),
                            0); // "no one"
      cb->isColliding = false;
    }
  }
}

//----------------------------------------
// Main physics system
//----------------------------------------
void PhysicsSystem(GameState_t *gs, Engine_t *eng, SoundSystem_t *soundSys,
                   float dt) {
  int emCount = eng->em.count;

  // ===== Update projectiles =====
  UpdateProjectiles(gs, eng, soundSys, dt);

  // ===== Update actors =====
  for (int i = 0; i < emCount; i++) {
    if (!eng->em.alive[i])
      continue;

    EntityType_t type = eng->actors.types[i];
    if (!(type == ENTITY_PLAYER || type == ENTITY_MECH || type == ENTITY_TANK))
      continue;

    UpdateActorPosition(eng, gs, i, dt);

    // Reinsert into grid if moved
    if (!Vector3Equals(*(Vector3 *)getComponent(&eng->actors, i,
                                                gs->compReg.cid_prevPositions),
                       (*(Vector3 *)getComponent(&eng->actors, i,
                                                 gs->compReg.cid_Positions)))) {
      // printf("updating entity position\n");
      UpdateEntityInGrid(gs, eng, MakeEntityID(ET_ACTOR, i));
    }
  }

  // ===== Resolve collisions =====
  ResolveActorCollisions(gs, eng);
  ResolveActorStaticCollisions(gs, eng);

  ResolveTriggerEvents(gs, eng);
}
