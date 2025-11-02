// systems.c
// Implements player input, physics, and rendering

#include "systems.h"
#include "game.h"
#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include "sound.h"
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define BOB_AMOUNT 0.5f
#define WORLD_SIZE_X 10
#define WORLD_SIZE_Z 10

#define GRAVITY 20.0f // units per second²
#define TERMINAL_VELOCITY 50.0f

#define ENTITY_FEET_OFFSET 10.0f // how high the entity "stands" above terrain

Vector3 recoilOffset = {0};

void LoadAssets() {
  Texture2D sandTex = LoadTexture("assets/textures/xtSand.png");
}

// ---------------- Player Control ----------------

void UpdateRayCast(Raycast_t *raycast, Vector3 position,
                   Orientation orientation) {
  raycast->ray.position = position;
  raycast->ray.direction = ConvertOrientationToVector3(orientation);
}

void UpdateRayCastToModel(GameState_t *gs, Raycast_t *raycast, int entityId,
                          int modelId) {
  ModelCollection_t *collection = &gs->components.modelCollections[entityId];

  // --- Handle per-axis inversion ---
  float yawInvert = collection->rotInverts[modelId][0] ? -1.0f : 1.0f;
  float pitchInvert = collection->rotInverts[modelId][1] ? -1.0f : 1.0f;
  float rollInvert = collection->rotInverts[modelId][2] ? -1.0f : 1.0f;

  // --- Get model world position and orientation ---
  Vector3 position = collection->globalPositions[modelId];
  Orientation orientation = collection->globalOrientations[modelId];

  // Apply axis inversions
  orientation.yaw *= yawInvert;
  orientation.pitch *= pitchInvert;
  orientation.roll *= rollInvert;

  orientation.yaw += raycast->oriOffset.yaw;
  orientation.pitch += raycast->oriOffset.pitch;
  orientation.roll += raycast->oriOffset.roll;

  // --- Compute final world-space ray ---
  raycast->ray.position = Vector3Add(position, raycast->localOffset);
  raycast->ray.direction = ConvertOrientationToVector3(orientation);
}

void UpdateRaycastFromTorso(ModelCollection_t *mc, Raycast_t *rc) {
  int parent = rc->parentModelIndex;
  if (parent < 0 || parent >= mc->countModels)
    return;

  // Get world position and orientation of the torso
  Vector3 pos = mc->globalPositions[parent];
  Orientation ori = mc->globalOrientations[parent];

  // Apply per-axis inversion if needed
  float yawInvert = mc->rotInverts[parent][0] ? -1.0f : 1.0f;
  float pitchInvert = mc->rotInverts[parent][1] ? -1.0f : 1.0f;
  float rollInvert = mc->rotInverts[parent][2] ? -1.0f : 1.0f;

  ori.yaw *= yawInvert;
  ori.pitch *= pitchInvert;
  ori.roll *= rollInvert;

  ori.yaw += rc->oriOffset.yaw;

  // Update raycast
  rc->ray.position = pos;
  rc->ray.direction = ConvertOrientationToVector3(ori);
}

// check raycast collision
// currently just returns true or false for any collision
// with an entity with C_HITBOX flag
// should later return the entity ID/index or its type

bool CheckRaycastCollision(GameState_t *gs, Raycast_t *raycast, entity_t self) {
  bool hit = false;
  RayCollision collision;

  // Track the closest hit (optional)
  float closestDist = FLT_MAX;
  int hitEntity = -1;

  for (int i = 0; i < gs->em.count; i++) {
    if (!gs->em.alive[i])
      continue;

    // Only check entities that have a hitbox collection
    if (!(gs->em.masks[i] & C_HITBOX))
      continue;

    if (i == self) {
      continue;
    }

    ModelCollection_t *hitboxes = &gs->components.hitboxCollections[i];

    // Iterate over each model in the entity's hitbox collection
    for (int m = 0; m < hitboxes->countModels; m++) {
      Model model = hitboxes->models[m];
      Vector3 modelPos = hitboxes->globalPositions[m];
      Orientation modelOrient = hitboxes->globalOrientations[m];

      // Build model's world transform matrix
      Matrix transform = MatrixIdentity();
      transform = MatrixMultiply(transform, MatrixRotateX(modelOrient.pitch));
      transform = MatrixMultiply(transform, MatrixRotateY(modelOrient.yaw));
      transform = MatrixMultiply(transform, MatrixRotateZ(modelOrient.roll));
      transform = MatrixMultiply(
          transform, MatrixTranslate(modelPos.x, modelPos.y, modelPos.z));

      // Perform the ray–mesh collision test
      collision = GetRayCollisionMesh(raycast->ray, model.meshes[0], transform);
      // TODO currently relying just on this, need to use a quad tree instead
      if (collision.hit && collision.distance < closestDist) {
        closestDist = collision.distance;
        hitEntity = i;
        hit = true;
      }
    }
  }

  if (hit) {
    printf("Ray hit entity %d at distance %.2f\n", hitEntity, closestDist);
  }

  return hit;
}

void UpdateEntityRaycasts(GameState_t *gs, entity_t e) {
  if (e < 0 || e >= gs->em.count)
    return;

  ModelCollection_t *mc = &gs->components.modelCollections[e];
  int rayCount = gs->components.rayCounts[e];
  if (rayCount <= 0)
    return;

  // --- Update primary raycast (index 0) to follow torso ---
  Raycast_t *primary = &gs->components.raycasts[e][0];
  UpdateRaycastFromTorso(mc, primary);

  // Compute the target point this ray points at
  Vector3 targetPoint =
      Vector3Add(primary->ray.position,
                 Vector3Scale(primary->ray.direction, primary->distance));

  // --- Update secondary raycasts to point at the same target ---
  for (int i = 1; i < rayCount; i++) {
    Raycast_t *rc = &gs->components.raycasts[e][i];

    // Keep the ray origin at its local offset from the model
    Vector3 origin =
        Vector3Add(mc->globalPositions[rc->parentModelIndex], rc->localOffset);

    // Compute direction vector pointing at the primary ray's target
    Vector3 dir = Vector3Subtract(targetPoint, origin);
    dir = Vector3Normalize(dir);

    rc->ray.position = origin;
    rc->ray.direction = dir;
  }
}

// ========== GUN ==========

void ApplyTorsoRecoil(ModelCollection_t *mc, int torsoIndex, float intensity,
                      Vector3 direction) {
  if (Vector3Length(direction) > 0.0001f)
    direction = Vector3Normalize(direction);

  Orientation *torso = &mc->orientations[torsoIndex];

  // Add randomness
  float randYaw = ((float)rand() / RAND_MAX * 2.0f - 1.0f);
  float randPitch = ((float)rand() / RAND_MAX * 2.0f - 1.0f);

  float recoilYaw = (direction.x + randYaw * 0.3f) * intensity;
  float recoilPitch = (direction.y + randPitch * 0.3f) * intensity / 1.5;

  torso->yaw += recoilYaw;
  torso->pitch += recoilPitch;

  // Optional clamp (prevent over-rotation)
  if (torso->pitch > PI / 3.0f)
    torso->pitch = PI / 3.0f;
  if (torso->pitch < -PI / 3.0f)
    torso->pitch = -PI / 3.0f;
}

void UpdateTorsoRecoil(ModelCollection_t *mc, int torsoIndex, float dt) {
  Orientation *torso = &mc->orientations[torsoIndex];

  float stiffness = 8.0f;
  float damping = 6.0f;
}

void spawnProjectile(GameState_t *gs, Vector3 pos, Vector3 velocity,
                     float lifetime, float radius, float dropRate, int owner,
                     int type) {
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    if (gs->projectiles.active[i])
      continue;

    gs->projectiles.active[i] = true;
    gs->projectiles.positions[i] = pos;
    gs->projectiles.velocities[i] = velocity;
    gs->projectiles.lifetimes[i] = lifetime;
    gs->projectiles.radii[i] = radius;
    gs->projectiles.owners[i] = owner;
    gs->projectiles.types[i] = type;
    gs->projectiles.dropRates[i] = dropRate;
    break;
  }
}

void FireProjectile(GameState_t *gs, entity_t shooter, int rayIndex) {
  if (!gs->components.raycasts[shooter][rayIndex].active)
    return;

  int gunId = 0;

  Ray *ray = &gs->components.raycasts[shooter][rayIndex].ray;

  Vector3 origin = ray->position;
  Vector3 dir = Vector3Normalize(ray->direction);

  // Load weapon stats from components
  float muzzleVel = gs->components.muzzleVelocities[shooter][gunId]
                        ? gs->components.muzzleVelocities[shooter][gunId]
                        : 10.0f; // default fallback

  float drop = gs->components.dropRates[shooter][gunId]
                   ? gs->components.dropRates[shooter][gunId]
                   : 1.0f;

  Vector3 vel = Vector3Scale(dir, muzzleVel);
  printf("spawning projectile\n");
  spawnProjectile(gs, origin, vel,
                  10.0f,   // lifetime sec
                  0.5f,    // radius
                  drop,    // drop rate
                  shooter, // owner
                  1);      // type
}

// ========== END GUN ==========

void UpdateRayDistance(GameState_t *gs, entity_t e, float dt) {
  if (e < 0 || e >= gs->em.count)
    return;

  int rayIndex = 0; // the main torso ray
  Raycast_t *rc = &gs->components.raycasts[e][rayIndex];

  // Get mouse wheel movement
  int wheelMove = GetMouseWheelMove(); // +1 scroll up, -1 scroll down
  if (wheelMove != 0) {
    float sensitivity = 50.0f; // units per wheel step
    rc->distance += wheelMove * sensitivity;

    // Clamp distance
    if (rc->distance < 100.0f)
      rc->distance = 100.0f;
    if (rc->distance > 2500.0f)
      rc->distance = 2500.0f;

    printf("Ray distance: %.2f\n", rc->distance);
  }
}

void PlayerControlSystem(GameState_t *gs, SoundSystem_t *soundSys, float dt,
                         Camera3D *camera) {
  int pid = gs->playerId;
  Vector3 *pos = gs->components.positions;
  Vector3 *vel = gs->components.velocities;

  // player leg and torso orientations
  Orientation *leg = &gs->components.modelCollections[pid].orientations[0];
  Orientation *torso = &gs->components.modelCollections[pid].orientations[1];

  bool isSprinting = IsKeyDown(KEY_LEFT_SHIFT);

  float baseFOV = 60.0f;
  float sprintFOV = 80.0f;
  float fovSpeed = 12.0f; // how fast FOV interpolates

  float targetFOV = isSprinting ? sprintFOV : baseFOV;
  camera->fovy = camera->fovy + (targetFOV - camera->fovy) * dt * fovSpeed;

  float totalSpeedMult = isSprinting ? 1.5f : 1.0f;
  float forwardSpeedMult = 5.0f * totalSpeedMult;
  float backwardSpeedMult = 5.0f * totalSpeedMult;
  float strafeSpeedMult = 2.0f * totalSpeedMult;

  float turnRate = isSprinting ? 0.2f : 1.0f;

  // Rotate legs with A/D
  if (IsKeyDown(KEY_A))
    leg[pid].yaw -= 1.5f * dt * turnRate;
  if (IsKeyDown(KEY_D))
    leg[pid].yaw += 1.5f * dt * turnRate;

  // Torso yaw/pitch from mouse
  Vector2 mouse = GetMouseDelta();
  float sensitivity = 0.0007f;
  torso[pid].yaw += mouse.x * sensitivity;
  torso[pid].pitch += -mouse.y * sensitivity;
  // gs->entities.collisionCollections[pid].orientations[pid].yaw =
  // torso[pid].yaw;

  UpdateRayDistance(gs, gs->playerId, dt);

  // Clamp torso pitch between -89° and +89°
  if (torso[pid].pitch > 1.2f)
    torso[pid].pitch = 1.2f;
  if (torso[pid].pitch < -1.0f)
    torso[pid].pitch = -1.0f;

  // Movement is based on leg orientation
  float c = cosf(leg[pid].yaw);
  float s = sinf(leg[pid].yaw);
  Vector3 forward = {c, 0, s};
  Vector3 right = {-s, 0, c};

  // TODO fix later ZOOM basic
  if(IsKeyDown(KEY_B)){
    camera->fovy = 10;
  }else{
    camera->fovy = 60;
  }


  // Movement keys
  if (IsKeyDown(KEY_SPACE)) {
    vel[pid].y += forward.z * 100.0f * dt;
  }

  if (IsKeyDown(KEY_W)) {
    vel[pid].x += forward.x * 100.0f * dt * forwardSpeedMult;
    vel[pid].z += forward.z * 100.0f * dt * forwardSpeedMult;
  }
  if (IsKeyDown(KEY_S)) {
    vel[pid].x -= forward.x * 100.0f * dt * backwardSpeedMult;
    vel[pid].z -= forward.z * 100.0f * dt * backwardSpeedMult;
  }
  if (IsKeyDown(KEY_Q)) {
    vel[pid].x += right.x * -100.0f * dt * strafeSpeedMult;
    vel[pid].z += right.z * -100.0f * dt * strafeSpeedMult;
  }
  if (IsKeyDown(KEY_E)) {
    vel[pid].x += right.x * 100.0f * dt * strafeSpeedMult;
    vel[pid].z += right.z * 100.0f * dt * strafeSpeedMult;
  }

  if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) &&
      gs->components.cooldowns[pid][0] <= 0) {
    printf("firing\n");
    gs->components.cooldowns[pid][0] = gs->components.firerate[pid][0];
    QueueSound(soundSys, SOUND_WEAPON_FIRE, pos[pid], 0.4f, 1.0f);

    ApplyTorsoRecoil(&gs->components.modelCollections[gs->playerId], 1, 0.05f,
                     (Vector3){-0.2f, 1.0f, 0});

    FireProjectile(gs, gs->playerId, 1);
  }

  // Step cycle update
  Vector3 velocity = vel[pid];
  float speed = sqrtf(velocity.x * velocity.x + velocity.z * velocity.z);

  if (speed > 1.0f) {
    gs->pHeadbobTimer += dt * 8.0f;
    gs->components.stepRate[pid] = speed * 0.07;

    float prev = gs->components.prevStepCycle[pid];
    float curr =
        gs->components.stepCycle[pid] + gs->components.stepRate[pid] * dt;

    if (curr >= 1.0f)
      curr -= 1.0f; // wrap cycle

    if (prev > curr) { // wrapped around -> stomp
      QueueSound(soundSys, SOUND_FOOTSTEP, pos[pid], 0.2f, 1.0f);
    }

    gs->components.stepCycle[pid] = curr;
    gs->components.prevStepCycle[pid] = curr;
  } else {
    gs->pHeadbobTimer = 0;
    gs->components.stepCycle[pid] = 0.0f;
    gs->components.prevStepCycle[pid] = 0.0f;
  }
}

// ------------- Weapon Cooldowns -------------

void DecrementCooldowns(GameState_t *gs, float dt) {
  for (int i = 0; i < gs->em.count; i++) {
    // Skip dead entities
    if (!gs->em.alive[i])
      continue;

    // Only process entities that have the cooldown component
    if (gs->em.masks[i] & C_COOLDOWN_TAG) {
      gs->components.cooldowns[i][0] -= dt;

      // Clamp to zero
      if (gs->components.cooldowns[i][0] < 0.0f)
        gs->components.cooldowns[i][0] = 0.0f;
    }
  }
}

// ---------------- Physics ----------------

//----------------------------------------
// Terrain Collision: Keep entity on ground
//----------------------------------------

float GetTerrainHeightAtPoint(GameState_t *gs, Vector3 point) {
  // Cast a ray downward from above the point
  Vector3 rayStart = (Vector3){point.x, point.y + 1000.0f, point.z};
  Ray ray = {rayStart, (Vector3){0, -1, 0}};

  RayCollision col =
      GetRayCollisionMesh(ray, gs->terrain.mesh, MatrixIdentity());

  if (col.hit) {
    // terrain collision point: start + direction * distance
    return rayStart.y + ray.direction.y * col.distance;
  }

  // fallback if nothing below
  return 0.0f;
}

static float GetTerrainHeightAtEntity(GameState_t *gs, entity_t entity) {

  Vector3 pos = gs->components.positions[entity];
  return GetTerrainHeightAtPoint(gs, pos);
}

void ApplyTerrainCollision(GameState_t *gs, int entityId, float dt) {
  Vector3 *pos = &gs->components.positions[entityId];
  Vector3 *vel = &gs->components.velocities[entityId];

  // Apply gravity
  vel->y -= GRAVITY * dt;
  if (vel->y < -TERMINAL_VELOCITY)
    vel->y = -TERMINAL_VELOCITY;

  // Move entity
  pos->y += vel->y * dt;

  // Compute terrain height
  float terrainY = GetTerrainHeightAtEntity(gs, entityId);

  // Clamp above terrain + offset
  float desiredY = terrainY + ENTITY_FEET_OFFSET;
  if (pos->y < desiredY) {
    pos->y = desiredY;
    vel->y = 0; // stop falling
  }
}

//----------------------------------------
// Helpers
//----------------------------------------

// Project OBB corners onto an axis
typedef struct {
  float min, max;
} Projection;

static Projection ProjectOBB(const Vector3 *corners, int numCorners,
                             Vector3 axis, Vector3 center) {
  Projection p = {FLT_MAX, -FLT_MAX};
  for (int i = 0; i < numCorners; i++) {
    Vector3 world = Vector3Add(corners[i], center);
    float dot = Vector3DotProduct(world, axis);
    if (dot < p.min)
      p.min = dot;
    if (dot > p.max)
      p.max = dot;
  }
  return p;
}

// Compute overlap along axis
static float GetOverlap(Projection a, Projection b) {
  return fminf(a.max, b.max) - fmaxf(a.min, b.min);
}

// Get local axis from rotation matrix
static Vector3 OBBGetAxis(const Matrix *rot, int index) {
  switch (index) {
  case 0:
    return (Vector3){rot->m0, rot->m1, rot->m2};
  case 1:
    return (Vector3){rot->m4, rot->m5, rot->m6};
  case 2:
    return (Vector3){rot->m8, rot->m9, rot->m10};
  default:
    return (Vector3){0, 0, 0};
  }
}

//----------------------------------------
// OBB Collision + Resolution
//----------------------------------------

bool CheckAndResolveOBBCollision(Vector3 *aPos, ModelCollection_t *aCC,
                                 Vector3 *bPos, ModelCollection_t *bCC) {
  if (aCC->countModels == 0 || bCC->countModels == 0)
    return false;

  Model aCube = aCC->models[0];
  Model bCube = bCC->models[0];
  Vector3 aOffset = aCC->offsets[0];
  Vector3 bOffset = bCC->offsets[0];

  BoundingBox aBBox = GetMeshBoundingBox(aCube.meshes[0]);
  BoundingBox bBBox = GetMeshBoundingBox(bCube.meshes[0]);

  Vector3 aHalf = Vector3Scale(Vector3Subtract(aBBox.max, aBBox.min), 0.5f);
  Vector3 bHalf = Vector3Scale(Vector3Subtract(bBBox.max, bBBox.min), 0.5f);

  Vector3 aCenter = Vector3Add(*aPos, aOffset);
  Vector3 bCenter = Vector3Add(*bPos, bOffset);

  Matrix aRot = MatrixRotateXYZ((Vector3){aCC->orientations[0].pitch,
                                          aCC->orientations[0].yaw * -1,
                                          aCC->orientations[0].roll});
  Matrix bRot = MatrixRotateXYZ((Vector3){bCC->orientations[0].pitch,
                                          bCC->orientations[0].yaw * -1,
                                          bCC->orientations[0].roll});

  // Generate corners
  Vector3 aCorners[8], bCorners[8];
  for (int i = 0; i < 8; i++) {
    aCorners[i] =
        (Vector3){(i & 1 ? aHalf.x : -aHalf.x), (i & 2 ? aHalf.y : -aHalf.y),
                  (i & 4 ? aHalf.z : -aHalf.z)};
    bCorners[i] =
        (Vector3){(i & 1 ? bHalf.x : -bHalf.x), (i & 2 ? bHalf.y : -bHalf.y),
                  (i & 4 ? bHalf.z : -bHalf.z)};

    aCorners[i] = Vector3Transform(aCorners[i], aRot);
    bCorners[i] = Vector3Transform(bCorners[i], bRot);
  }

  // 15 SAT axes
  Vector3 axes[15];
  int idx = 0;
  for (int i = 0; i < 3; i++)
    axes[idx++] = OBBGetAxis(&aRot, i);
  for (int i = 0; i < 3; i++)
    axes[idx++] = OBBGetAxis(&bRot, i);
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      axes[idx++] =
          Vector3CrossProduct(OBBGetAxis(&aRot, i), OBBGetAxis(&bRot, j));

  float minOverlap = FLT_MAX;
  Vector3 mtvAxis = {0, 0, 0};

  for (int i = 0; i < 15; i++) {
    Vector3 axis = axes[i];
    if (Vector3Length(axis) < 1e-6f)
      continue; // skip degenerate
    axis = Vector3Normalize(axis);

    Projection aProj = ProjectOBB(aCorners, 8, axis, aCenter);
    Projection bProj = ProjectOBB(bCorners, 8, axis, bCenter);

    float overlap = GetOverlap(aProj, bProj);
    if (overlap <= 0.0f)
      return false; // separating axis found

    if (overlap < minOverlap) {
      minOverlap = overlap;
      mtvAxis = axis;
      Vector3 d = Vector3Subtract(bCenter, aCenter);
      if (Vector3DotProduct(d, mtvAxis) < 0)
        mtvAxis = Vector3Scale(mtvAxis, -1.0f);
    }
  }

  // Move A out along MTV
  *aPos = Vector3Subtract(*aPos, Vector3Scale(mtvAxis, minOverlap));
  return true;
}

//----------------------------------------
// Physics System
//----------------------------------------

// Sphere-OBB collision test
bool SphereIntersectsOBB(Vector3 sphereCenter, float radius, Vector3 boxCenter,
                         Vector3 boxHalfExtents, Matrix boxRotation) {
  // Transform sphere center to OBB local space
  Matrix invRot = MatrixTranspose(boxRotation); // inverse rotation
  Vector3 local = Vector3Subtract(sphereCenter, boxCenter);
  local = Vector3Transform(local, invRot);

  // Clamp to box extents
  Vector3 closest = {
      fmaxf(-boxHalfExtents.x, fminf(local.x, boxHalfExtents.x)),
      fmaxf(-boxHalfExtents.y, fminf(local.y, boxHalfExtents.y)),
      fmaxf(-boxHalfExtents.z, fminf(local.z, boxHalfExtents.z))};

  // Distance from sphere center to closest point
  Vector3 delta = Vector3Subtract(local, closest);
  return Vector3LengthSqr(delta) <= radius * radius;
}

// Check if projectile intersects entity's collision OBB
bool ProjectileIntersectsEntityOBB(GameState_t *gs, int projIndex, int eid) {
  ModelCollection_t *col = &gs->components.hitboxCollections[eid];
  if (col->countModels <= 0)
    return false;

  Vector3 sphere = gs->projectiles.positions[projIndex];
  float radius = gs->projectiles.radii[projIndex];

  for (int m = 0; m < col->countModels; m++) {
    Model model = col->models[m];
    BoundingBox bbox = GetMeshBoundingBox(model.meshes[0]);

    Vector3 halfExtents =
        Vector3Scale(Vector3Subtract(bbox.max, bbox.min), 0.5f);
    Vector3 center = col->globalPositions[m];

    Matrix rot = MatrixRotateXYZ((Vector3){col->globalOrientations[m].pitch,
                                           col->globalOrientations[m].yaw,
                                           col->globalOrientations[m].roll});

    if (SphereIntersectsOBB(sphere, radius, center, halfExtents, rot))
      return true;
  }

  return false;
}

void UpdateProjectiles(GameState_t *gs, float dt) {
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    if (!gs->projectiles.active[i])
      continue;

    // Decrease lifetime
    gs->projectiles.lifetimes[i] -= dt;
    if (gs->projectiles.lifetimes[i] <= 0.0f) {
      gs->projectiles.active[i] = false;
      continue;
    }

    // Apply drop/gravity
    gs->projectiles.velocities[i].y -= gs->projectiles.dropRates[i] * dt;

    // Update position
    gs->projectiles.positions[i] =
        Vector3Add(gs->projectiles.positions[i],
                   Vector3Scale(gs->projectiles.velocities[i], dt));

    if (GetTerrainHeightAtPoint(gs, gs->projectiles.positions[i]) >=
        gs->projectiles.positions[i].y) {
      printf("PROJECTILE: terrain collision\n");
      gs->projectiles.active[i] = false;
    }

    // -------- ENTITY COLLISION --------
    for (int e = 0; e < MAX_ENTITIES; e++) { // TODO use quad tree

      if (!(gs->em.masks[e] & C_HITBOX))
        continue;
      if (e == gs->projectiles.owners[i])
        continue;

      if (ProjectileIntersectsEntityOBB(gs, i, e)) {
        gs->projectiles.active[i] = false;
        printf("PROJECTILE: entity collision detected\n Entity_t %d\n", e);
        if (gs->em.masks[e] & C_HITPOINT_TAG) {
          gs->components.hitPoints[e] -= 50.0f;
          if (gs->components.hitPoints[e] <= 0) {
            gs->em.alive[e] = 0;
            printf("Entity killed \n");
          }
        }
        break;
      }
    }
  }
}

void PhysicsSystem(GameState_t *gs, float dt) {
  Vector3 *pos = gs->components.positions;
  Vector3 *vel = gs->components.velocities;
  int playerId = gs->playerId;

  // Floor clamping
  for (int i = 0; i < gs->em.count; i++) {
    if (gs->em.masks[i] & C_GRAVITY) {
      pos[i] = Vector3Add(pos[i], Vector3Scale(vel[i], dt));
      ApplyTerrainCollision(gs, i, dt);
    }
  }

  // Damping
  for (int i = 0; i < gs->em.count; i++) {
    vel[i].x *= 0.65f;
    vel[i].z *= 0.65f;
  }

  UpdateProjectiles(gs, dt);

  // Collision
  for (int i = 0; i < gs->em.count; i++) {
    if (i == playerId)
      continue;

    bool collided = CheckAndResolveOBBCollision(
        &pos[playerId], &gs->components.collisionCollections[playerId], &pos[i],
        &gs->components.collisionCollections[i]);

    if (collided) {
      // slide along collision
      Vector3 mtvDir = Vector3Normalize(Vector3Subtract(pos[playerId], pos[i]));
      float velDot = Vector3DotProduct(vel[playerId], mtvDir);
      if (velDot > 0.0f) {
        vel[playerId] =
            Vector3Subtract(vel[playerId], Vector3Scale(mtvDir, velDot));
      }
    }
  }
}

// ---------------- TURRET AI SYSTEM ----------------

void TurretAISystem(GameState_t *gs, SoundSystem_t *soundSys, float dt) {
  int playerId = gs->playerId;
  Vector3 playerPos = gs->components.positions[playerId];

  const float engageRange = 500.0f; // turrets become idle beyond this range
  const float minAccuracy = 0.6f;   // worst accuracy
  const float maxAccuracy = 1.0f;   // best accuracy at close range

  for (int i = 0; i < gs->em.count; i++) {
    if (!gs->em.alive[i])
      continue;
    if (!(gs->em.masks[i] & C_TURRET_BEHAVIOUR_1))
      continue;

    Vector3 turretPos = gs->components.positions[i];
    ModelCollection_t *turretModels = &gs->components.modelCollections[i];

    Orientation *base = &turretModels->orientations[0];
    Orientation *barrel = &turretModels->orientations[1];

    // --- Distance and accuracy falloff ---
    Vector3 toPlayer = Vector3Subtract(playerPos, turretPos);
    float distanceToPlayer = Vector3Length(toPlayer);

    // If far away, idle (look straight up)
    if (distanceToPlayer > engageRange) {
      base->yaw =
          Lerp(base->yaw, 0.0f, dt * 1.0f); // slowly return to neutral yaw
      barrel->pitch =
          Lerp(barrel->pitch, -PI / 2.0f, dt * 1.0f); // look straight up
      continue;
    }

    // Scale accuracy with distance
    float distRatio = distanceToPlayer / engageRange; // 0 near, 1 at max
    float accuracy = maxAccuracy - (maxAccuracy - minAccuracy) * distRatio;
    if (accuracy < minAccuracy)
      accuracy = minAccuracy;

    // --- Aim at player ---
    float maxOffsetAngle =
        (1.0f - accuracy) * (PI / 4.0f); // less accurate = larger offset
    float yawOffset = ((float)rand() / RAND_MAX * 2.0f - 1.0f) * maxOffsetAngle;
    float pitchOffset =
        ((float)rand() / RAND_MAX * 2.0f - 1.0f) * maxOffsetAngle;

    float targetYaw = atan2f(toPlayer.z, toPlayer.x) - PI / 2.0f + yawOffset;
    float targetPitch =
        atan2f(toPlayer.y,
               Vector3Length((Vector3){toPlayer.x, 0, toPlayer.z})) +
        pitchOffset;

    // --- Smooth rotate ---
    float yawDiff = targetYaw - base->yaw;
    if (yawDiff > PI)
      yawDiff -= 2 * PI;
    if (yawDiff < -PI)
      yawDiff += 2 * PI;
    base->yaw += yawDiff * dt * 4.0f;

    barrel->pitch = Lerp(barrel->pitch, targetPitch, dt * 2.0f);

    // --- Check if aimed ---
    float maxAllowedError = (1.0f - accuracy) * (PI / 12.0f);
    bool yawAligned = fabsf(yawDiff) < maxAllowedError;
    bool pitchAligned = fabsf(barrel->pitch - targetPitch) < maxAllowedError;

    // --- Optional random misses ---
    if (accuracy < 1.0f) {
      float missChance = 1.0f - accuracy;
      if (((float)rand() / RAND_MAX) < missChance) {
        yawAligned = false;
        pitchAligned = false;
      }
    }

    // --- Fire if aligned ---
    if (yawAligned && pitchAligned && gs->components.cooldowns[i][0] <= 0.0f) {
      gs->components.cooldowns[i][0] = gs->components.firerate[i][0];
      UpdateRayCastToModel(gs, &gs->components.raycasts[i][0], i, 1);
      QueueSound(soundSys, SOUND_WEAPON_FIRE, turretPos, 0.3f, 1.0f);

      bool hit = CheckRaycastCollision(gs, &gs->components.raycasts[i][0], i);
      if (hit)
        printf("Turret %d hit something!\n", i);
    }
  }
}

// ---------------- Rendering ----------------

// --- Update world-space transforms for a ModelCollection ---
static void UpdateModelCollectionWorldTransforms(ModelCollection_t *mc,
                                                 Vector3 entityPos,
                                                 Vector3 cameraTarget,
                                                 int entityType) {
  for (int m = 0; m < mc->countModels; m++) {
    Vector3 localOffset = mc->offsets[m];
    Orientation localRot = mc->orientations[m];
    int parentId = mc->parentIds[m];

    Vector3 parentWorldPos;
    float yaw = localRot.yaw;
    float pitch = localRot.pitch;
    float roll = localRot.roll;

    if (parentId != -1 && parentId < m) {
      Orientation parentRot = mc->globalOrientations[parentId];
      float parentYaw = parentRot.yaw * 1.0f;
      float parentPitch = parentRot.pitch * 1.0f;
      float parentRoll = parentRot.roll * 1.0f;

      float yawLock = mc->rotLocks[m][0] ? 1.0f : 0.0f;
      float pitchLock = mc->rotLocks[m][1] ? 1.0f : 0.0f;
      float rollLock = mc->rotLocks[m][2] ? 1.0f : 0.0f;

      yaw = parentYaw * yawLock + localRot.yaw * (1.0f - yawLock);
      pitch = parentPitch * pitchLock + localRot.pitch * (1.0f - pitchLock);
      roll = parentPitch * rollLock + localRot.roll * (1.0f - rollLock);

      parentWorldPos = mc->globalPositions[parentId];
      localOffset = Vector3Transform(localOffset, MatrixRotateY(parentYaw));
    } else {
      parentWorldPos = entityPos;
      yaw *= -1.0f;
    }
    // --- Apply local rotation offset normally ---
    yaw += mc->localRotationOffset[m].yaw;
    pitch += mc->localRotationOffset[m].pitch;
    roll += mc->localRotationOffset[m].roll;

    // --- Apply per-axis inversion ---
    if (mc->rotInverts[m][0])
      yaw *= -1.0f;
    if (mc->rotInverts[m][1])
      pitch *= -1.0f;
    if (mc->rotInverts[m][2])
      roll *= -1.0f;

    mc->globalPositions[m] = Vector3Add(parentWorldPos, localOffset);
    mc->globalOrientations[m] = (Orientation){yaw, pitch, roll};
  }
}

// --- Helper to draw a ModelCollection (solid or wireframe) ---
// Uses precomputed globalPositions/globalOrientations if available.
// Otherwise, computes the same local->world transform as before.
static void DrawModelCollection(ModelCollection_t *mc, Vector3 entityPos,
                                Color tint, bool wireframe) {
  int numModels = mc->countModels;
  for (int m = 0; m < numModels; m++) {
    Vector3 drawPos;
    float yaw, pitch, roll;

    // If the caller has provided precomputed global transforms, use them.
    if (mc->globalPositions != NULL && mc->globalOrientations != NULL) {
      drawPos = mc->globalPositions[m];
      Orientation g = mc->globalOrientations[m];
      yaw = g.yaw;
      pitch = -g.pitch;
      roll = g.roll;
    } else {
      // Fallback: compute world transform exactly like previous code.
      Vector3 localOffset = mc->offsets[m];
      Orientation localRot = mc->orientations[m];
      int parentId = mc->parentIds[m];

      Vector3 parentWorldPos;
      yaw = localRot.yaw;
      pitch = localRot.pitch;
      roll = localRot.roll;

      // --- Parent transform inheritance (same rules as before) ---
      if (parentId != -1 && parentId < m) {
        Orientation parentRot = mc->orientations[parentId];
        float parentYaw = parentRot.yaw * -1.0f;
        float parentPitch = parentRot.pitch * 1.0f;
        float parentRoll = parentRot.roll * 1.0f;

        float yawLock = mc->rotLocks[m][0] ? 1.0f : 0.0f;
        float pitchLock = mc->rotLocks[m][1] ? 1.0f : 0.0f;
        float rollLock = mc->rotLocks[m][2] ? 1.0f : 0.0f;

        yaw = parentYaw * yawLock + localRot.yaw * (1.0f - yawLock);
        pitch = parentPitch * pitchLock + localRot.pitch * (1.0f - pitchLock);
        roll = parentPitch * rollLock + localRot.roll * (1.0f - rollLock);

        parentWorldPos = Vector3Add(entityPos, mc->offsets[parentId]);
        localOffset = Vector3Transform(localOffset, MatrixRotateY(parentYaw));
      } else {
        parentWorldPos = entityPos;
        // yaw *= -1.0f;
      }

      // --- Final world position ---
      drawPos = Vector3Add(parentWorldPos, localOffset);
    }

    // --- Build rotation matrix (same order as before) ---
    Matrix rotMat = MatrixRotateY(yaw);
    rotMat = MatrixMultiply(MatrixRotateX(pitch), rotMat);
    rotMat = MatrixMultiply(MatrixRotateZ(roll), rotMat);

    // --- Draw model using world transform ---
    rlPushMatrix();
    rlTranslatef(drawPos.x, drawPos.y, drawPos.z);
    rlMultMatrixf(MatrixToFloat(rotMat));

    if (wireframe) {
      rlPushMatrix();
      rlSetLineWidth(1.0f); // make lines 3px thick
      DrawModelWires(mc->models[m], (Vector3){0, 0, 0}, 1.0f, tint);
      rlSetLineWidth(1.0f); // reset after drawing
      rlPopMatrix();
    } else {
      DrawModel(mc->models[m], (Vector3){0, 0, 0}, 1.0f, tint);
    }
    rlPopMatrix();
  }
}

void DrawProjectiles(GameState_t *gs) {
  for (int i = 0; i < MAX_PROJECTILES; i++) {
    if (!gs->projectiles.active[i])
      continue;

    DrawSphere(gs->projectiles.positions[i], gs->projectiles.radii[i], YELLOW);
  }
}

void DrawRaycasts(GameState_t *gs) {
  for (int i = 0; i < gs->em.count; i++) {
    if (!gs->em.alive[i])
      continue; // skip dead entities
    if (!(gs->em.masks[i] & C_RAYCAST))
      continue; // skip entities without raycast

    for (int j = 0; j < gs->components.rayCounts[i]; j++) {

      Raycast_t *raycast = &gs->components.raycasts[i][j];

      // Color for debug: player = RED, others = BLUE
      Color c = (i == gs->playerId) ? RED : BLUE;

      DrawRay(raycast->ray, c);
    }
  }
}

// --- Main Render Function ---
void RenderSystem(GameState_t *gs, Camera3D camera) {

  const float bobAmount = BOB_AMOUNT; // height in meters, visual only

  int pid = gs->playerId;
  Vector3 playerPos = gs->components.positions[pid];
  ModelCollection_t *mc = &gs->components.modelCollections[pid];

  // --- Compute headbob ---
  float t = gs->components.stepCycle[pid];
  float bobTri = (t < 0.5f) ? (t * 2.0f) : (2.0f - t * 2.0f); // 0->1->0
  bobTri = 1.0f - bobTri;                                     // flip to drop
  float torsoBobY = bobTri * bobAmount;

  // --- Update torso position with bob ---
  Vector3 torsoPos = playerPos;
  torsoPos.y += 10.0f + torsoBobY; // base height + bob

  // Update torso model collection transforms
  UpdateModelCollectionWorldTransforms(mc, torsoPos, camera.target,
                                       gs->components.types[pid]);

  // --- Compute forward from torso orientation ---
  Orientation torsoOri = mc->globalOrientations[1]; // torso model index

  // Rotate yaw 90° left relative to torso
  float camYaw = torsoOri.yaw;
  float camPitch = torsoOri.pitch;

  // Convert yaw/pitch to forward vector correctly
  Vector3 forward;
  forward.x = sinf(camYaw) * cosf(camPitch);
  forward.y = sinf(camPitch);
  forward.z = cosf(camYaw) * cosf(camPitch);

  // --- Small camera offset/jitter around torso ---
  float camBobX = 0;
  // ((float)GetRandomValue(-100, 100) / 1000.0f); // small X jitter
  float camBobZ = 0;
  // ((float)GetRandomValue(-100, 100) / 1000.0f); // small Z jitter
  float camBobY = torsoBobY * 0.0f; // reduced vertical bob

  // Apply small camera bob
  Vector3 eye = {torsoPos.x + camBobX, torsoPos.y + camBobY,
                 torsoPos.z + camBobZ};

  camera.position = eye;
  camera.target = Vector3Add(eye, forward);

  BeginDrawing();
  ClearBackground((Color){20, 20, 30, 255});

  BeginMode3D(camera);

  DrawModel(gs->terrain.model, (Vector3){0, 0, 0}, 1.0f, BROWN);

  // --- Draw world terrain/chunks ---
  // for (int z = 0; z < WORLD_SIZE_Z; z++) {
  //   for (int x = 0; x < WORLD_SIZE_X; x++) {
  //     Chunk c = world[x][z];
  //     DrawModel(level->levelChunks[c.type], c.worldPos, 1.0f, WHITE);
  //   }
  // }

  // --- Draw all entities ---
  DrawProjectiles(gs);

  for (int i = 0; i < gs->em.count; i++) {
    Vector3 entityPos = gs->components.positions[i];

    // Update world transforms
    UpdateModelCollectionWorldTransforms(&gs->components.modelCollections[i],
                                         entityPos, camera.target,
                                         gs->components.types[i]);
    UpdateModelCollectionWorldTransforms(
        &gs->components.collisionCollections[i], entityPos, camera.target,
        gs->components.types[i]);
    UpdateModelCollectionWorldTransforms(&gs->components.hitboxCollections[i],
                                         entityPos, camera.target,
                                         gs->components.types[i]);

    DrawRaycasts(gs);

    // Visual models (solid white)
    DrawModelCollection(&gs->components.modelCollections[i], entityPos, WHITE,
                        false);

    // Movement collision boxes (green wireframe)
    DrawModelCollection(&gs->components.collisionCollections[i], entityPos,
                        GREEN, true);

    Color hitboxColor = RED;
    if (!gs->em.alive[i]) {
      hitboxColor = BLACK;
    }
    // Hitboxes (red wireframe)
    DrawModelCollection(&gs->components.hitboxCollections[i], entityPos,
                        hitboxColor, true);
  }

  EndMode3D();

  // draw UI segment

  DrawFPS(10, 10);

  // Prepare debug string
  char debugOri[128];
  snprintf(debugOri, sizeof(debugOri),
           "Torso Yaw: %.2f  Pitch: %.2f  Roll: %.2f\n"
           "Camera Yaw: %.2f  Pitch: %.2f\n Convergence distance %f",
           torsoOri.yaw, torsoOri.pitch, torsoOri.roll, camYaw, camPitch,
           gs->components.raycasts[pid][0].distance);

  // Draw text at top-left
  DrawText(debugOri, 10, 40, 20, RAYWHITE);

  // Draw player position
  char posText[64];
  snprintf(posText, sizeof(posText), "Player Pos: X: %.2f  Y: %.2f  Z: %.2f",
           playerPos.x, playerPos.y, playerPos.z);

  int textWidth = MeasureText(posText, 20);
  DrawText(posText, GetScreenWidth() - textWidth - 10, 10, 20, RAYWHITE);

  // draw torso leg orientation
  Orientation legs_orientation =
      gs->components.modelCollections[gs->playerId].orientations[0];

  Orientation torso_orientation =
      gs->components.modelCollections[gs->playerId].orientations[1];

  float legYaw = fmod(legs_orientation.yaw, 2 * PI);
  if (legYaw < 0)
    legYaw += 2 * PI;

  float torsoYaw = fmod(torso_orientation.yaw, 2 * PI);
  if (torsoYaw < 0)
    torsoYaw += 2 * PI;

  // difference
  float diff = fmod(torsoYaw - legYaw + PI, 2 * PI);
  if (diff < 0)
    diff += 2 * PI;

  char rotText[64];
  snprintf(rotText, sizeof(rotText),
           "legs yaw: %f \ntorso yaw: %f \ndiff: %f\n", legYaw, torsoYaw, diff);

  int rotTextWidth = MeasureText(rotText, 20);
  DrawText(rotText, GetScreenWidth() - rotTextWidth - 10, 30, 20, RAYWHITE);

  float length = 50.0f;
  Vector2 arrowStart =
      (Vector2){GetScreenWidth() * 0.8, GetScreenHeight() * 0.8};

  float endX = arrowStart.x + cosf(-diff) * length;
  float endY = arrowStart.y + sinf(-diff) * length;

  float endXTorso = arrowStart.x;
  float endYTorso = arrowStart.y - length;

  // torso arrow
  DrawLineEx(arrowStart, (Vector2){endXTorso, endYTorso}, 3.0f, RED);
  // leg arrow
  DrawLineEx(arrowStart, (Vector2){endX, endY}, 3.0f, GREEN);

  // Draw other UI shapes
  DrawCircleV(arrowStart, 10, DARKBLUE);

  DrawCircleLines(GetScreenWidth() / 2, GetScreenHeight() / 2, 10, RED);

  EndDrawing();
}

void MainMenuSystem(GameState_t *gs) {
  BeginDrawing();
  ClearBackground(BLACK);

  // Simple button rectangle
  Rectangle startButton = {GetScreenWidth() / 2.0f - 100,
                           GetScreenHeight() / 2.0f - 25, 200, 50};

  // Check if mouse is over the button
  Vector2 mousePos = GetMousePosition();
  bool hovering = CheckCollisionPointRec(mousePos, startButton);

  // Draw the button
  DrawRectangleRec(startButton, hovering ? DARKGRAY : GRAY);
  DrawText("START", startButton.x + 50, startButton.y + 12, 24, WHITE);

  // Check for click
  if (hovering && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
    gs->state = STATE_INLEVEL;

    DisableCursor();
  }

  EndDrawing();
}

void UpdateGame(GameState_t *gs, SoundSystem_t *soundSys, Camera3D *camera,
                float dt) {

  if (gs->state == STATE_INLEVEL) {

    PlayerControlSystem(gs, soundSys, dt, camera);

    int entity = 0; // player
    int rayIndex = 0;
    Raycast_t *rc = &gs->components.raycasts[entity][rayIndex];
    ModelCollection_t *mc = &gs->components.modelCollections[entity];

    UpdateRayCastToModel(gs, rc, entity, 1);
    UpdateEntityRaycasts(gs, entity);

    TurretAISystem(gs, soundSys, dt);

    DecrementCooldowns(gs, dt);

    UpdateTorsoRecoil(&gs->components.modelCollections[gs->playerId], 1, dt);

    PhysicsSystem(gs, dt);

    RenderSystem(gs, *camera);

    int pid = gs->playerId;
    Vector3 playerPos = gs->components.positions[pid];

    ProcessSoundSystem(soundSys, playerPos);
  } else if (gs->state == STATE_MAINMENU) {
    MainMenuSystem(gs);
  }
}
