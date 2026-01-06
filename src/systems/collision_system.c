
#include "systems.h"

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

bool CheckOBBOverlap(Vector3 aPos, ModelCollection_t *aCC, Vector3 bPos,
                     ModelCollection_t *bCC) {
  if (aCC->countModels == 0 || bCC->countModels == 0)
    return false;

  // Only model 0 used
  Model aCube = aCC->models[0];
  Model bCube = bCC->models[0];

  Vector3 aOffset = aCC->offsets[0];
  Vector3 bOffset = bCC->offsets[0];

  BoundingBox aBBox = GetMeshBoundingBox(aCube.meshes[0]);
  BoundingBox bBBox = GetMeshBoundingBox(bCube.meshes[0]);

  Vector3 aHalf = Vector3Scale(Vector3Subtract(aBBox.max, aBBox.min), 0.5f);
  Vector3 bHalf = Vector3Scale(Vector3Subtract(bBBox.max, bBBox.min), 0.5f);

  Vector3 aCenter = Vector3Add(aPos, aOffset);
  Vector3 bCenter = Vector3Add(bPos, bOffset);

  Matrix aRot = MatrixRotateXYZ((Vector3){aCC->orientations[0].pitch,
                                          aCC->orientations[0].yaw * -1,
                                          aCC->orientations[0].roll});
  Matrix bRot = MatrixRotateXYZ((Vector3){bCC->orientations[0].pitch,
                                          bCC->orientations[0].yaw * -1,
                                          bCC->orientations[0].roll});

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

  // SAT test: if any axis separates them → no overlap
  for (int i = 0; i < 15; i++) {
    Vector3 axis = axes[i];
    if (Vector3Length(axis) < 1e-6f)
      continue;

    axis = Vector3Normalize(axis);

    Projection aProj = ProjectOBB(aCorners, 8, axis, aCenter);
    Projection bProj = ProjectOBB(bCorners, 8, axis, bCenter);

    if (GetOverlap(aProj, bProj) <= 0.0f)
      return false; // separation → no collision
  }

  return true; // overlapped on all axes
}

bool SegmentIntersectsOBB(Vector3 p0, Vector3 p1, ModelCollection_t *coll,
                          int modelIndex) {
  if (!coll || modelIndex < 0 || modelIndex >= coll->countModels)
    return false;

  Model *model = &coll->models[modelIndex];
  if (model->meshCount == 0 || !model->meshes)
    return false;

  // Local-space bounding box (unrotated box)
  BoundingBox bb = GetMeshBoundingBox(model->meshes[0]);

  Vector3 center = coll->globalPositions[modelIndex];
  Orientation O = coll->globalOrientations[modelIndex];

  // Convert to rotation matrix
  Matrix rot = MatrixRotateXYZ((Vector3){O.pitch, O.yaw, O.roll});
  Matrix invRot = MatrixTranspose(rot);

  // Build ray
  Ray ray;
  ray.position = p0;
  ray.direction = Vector3Normalize(Vector3Subtract(p1, p0));
  float maxDist = Vector3Distance(p0, p1);

  // Move ray into OBB local-space
  Vector3 localOrigin = Vector3Subtract(ray.position, center);
  localOrigin = Vector3Transform(localOrigin, invRot);

  Vector3 localDir = Vector3Transform(ray.direction, invRot);

  Ray localRay = {localOrigin, localDir};

  // Ray vs AABB test in local box space
  RayCollision hit = GetRayCollisionBox(localRay, bb);

  return (hit.hit && hit.distance <= maxDist);
}

// Check if projectile intersects entity's collision OBB
bool ProjectileIntersectsEntityOBB(Engine_t *eng, int projIndex, entity_t eid) {
  EntityCategory_t cat = GetEntityCategory(eid);
  int idx = GetEntityIndex(eid);

  ModelCollection_t *col = NULL;

  // figure out which hitboxes to read
  switch (cat) {
  case ET_ACTOR:
    if (!eng->em.alive[idx])
      return false;
    col = &eng->actors->hitboxCollections[idx];
    break;
  case ET_STATIC:
    col = &eng->statics.hitboxCollections[idx];
    break;
  default:
    return false; // projectiles or unknown
  }

  if (!col || col->countModels <= 0)
    return false;

  Vector3 sphere = eng->projectiles.positions[projIndex];
  float radius = eng->projectiles.radii[projIndex];

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
