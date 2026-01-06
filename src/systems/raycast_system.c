
#include "systems.h"
#include <float.h>
#include <stdio.h>

void UpdateRayCast(Raycast_t *raycast, Vector3 position,
                   Orientation orientation) {
  raycast->ray.position = position;
  raycast->ray.direction = ConvertOrientationToVector3(orientation);
}

void UpdateRayCastToModel(GameState_t *gs, Engine_t *eng, Raycast_t *raycast,
                          int entityId, int modelId) {
  ModelCollection_t *collection = &eng->actors->modelCollections[entityId];

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
// TODO should later return the entity ID/index or its type

bool CheckRaycastCollision(GameState_t *gs, Engine_t *eng, Raycast_t *raycast,
                           entity_t self) {
  bool hit = false;
  RayCollision collision;

  // Track the closest hit (optional)
  float closestDist = FLT_MAX;
  int hitEntity = -1;

  for (int i = 0; i < eng->em.count; i++) {
    if (!eng->em.alive[i])
      continue;

    // Only check entities that have a hitbox collection
    if (!(eng->em.masks[i] & C_HITBOX))
      continue;

    if (i == self) {
      continue;
    }

    ModelCollection_t *hitboxes = &eng->actors->hitboxCollections[i];

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

void UpdateEntityRaycasts(Engine_t *eng, entity_t e) {
  if (e < 0 || e >= eng->em.count)
    return;

  ModelCollection_t *mc = &eng->actors->modelCollections[e];
  int rayCount = eng->actors->rayCounts[e];
  if (rayCount <= 0)
    return;

  // --- Update primary raycast (index 0) to follow torso ---
  Raycast_t *primary = &eng->actors->raycasts[e][0];
  UpdateRaycastFromTorso(mc, primary);

  // Compute the target point this ray points at
  Vector3 targetPoint =
      Vector3Add(primary->ray.position,
                 Vector3Scale(primary->ray.direction, primary->distance));

  // --- Update secondary raycasts to point at the same target ---
  for (int i = 1; i < rayCount; i++) {
    Raycast_t *rc = &eng->actors->raycasts[e][i];

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
