
#include "aabb.h"
#include "capsule.h"
#include "collision_hit.h"
#include "collision_instance.h"
#include "raylib.h"
#include "sphere.h"
#include <math.h>
#include <stdio.h>

static Vector3 ClosestPointOnSegment(Vector3 a, Vector3 b, Vector3 p) {
  Vector3 ab = Vector3Subtract(b, a);
  float t =
      Vector3DotProduct(Vector3Subtract(p, a), ab) / Vector3DotProduct(ab, ab);

  t = Clamp(t, 0.0f, 1.0f);
  return Vector3Add(a, Vector3Scale(ab, t));
}

// Closest points between two line segments in XZ (Y ignored).
// Returns distance; writes closest point on each segment to pa and pb.
// Uses the parametric approach from Ericson "Real-Time Collision Detection" 5.1.9.
static float ClosestDistSegSeg2D(Vector3 A, Vector3 B, Vector3 C, Vector3 D,
                                  Vector3 *pa, Vector3 *pb) {
  float d1x = B.x - A.x, d1z = B.z - A.z;
  float d2x = D.x - C.x, d2z = D.z - C.z;
  float rx  = A.x - C.x, rz  = A.z - C.z;

  float a = d1x*d1x + d1z*d1z;
  float e = d2x*d2x + d2z*d2z;
  float f = d2x*rx  + d2z*rz;
  float s, t;

  if (a < 1e-6f && e < 1e-6f) {
    s = t = 0.0f;
  } else if (a < 1e-6f) {
    s = 0.0f;
    t = Clamp(f / e, 0.0f, 1.0f);
  } else {
    float c = d1x*rx + d1z*rz;
    if (e < 1e-6f) {
      t = 0.0f;
      s = Clamp(-c / a, 0.0f, 1.0f);
    } else {
      float b     = d1x*d2x + d1z*d2z;
      float denom = a*e - b*b;
      s = (denom > 1e-6f) ? Clamp((b*f - c*e) / denom, 0.0f, 1.0f) : 0.0f;
      t = (b*s + f) / e;
      if (t < 0.0f) {
        t = 0.0f;
        s = Clamp(-c / a, 0.0f, 1.0f);
      } else if (t > 1.0f) {
        t = 1.0f;
        s = Clamp((b - c) / a, 0.0f, 1.0f);
      }
    }
  }

  *pa = (Vector3){A.x + s*d1x, 0.0f, A.z + s*d1z};
  *pb = (Vector3){C.x + t*d2x, 0.0f, C.z + t*d2z};
  float dx = pa->x - pb->x, dz = pa->z - pb->z;
  return sqrtf(dx*dx + dz*dz);
}

bool SphereVsSphere(const CollisionInstance *a, const SphereCollider *sa,
                    const CollisionInstance *b, const SphereCollider *sb,
                    CollisionHit *outHit) {
  Vector3 delta = Vector3Subtract(sb->center, sa->center);
  float dist2 = Vector3LengthSqr(delta);
  float r = sa->radius + sb->radius;

  if (dist2 > r * r)
    return false;

  float dist = sqrtf(dist2);

  if (outHit) {
    outHit->a = a->owner;
    outHit->b = b->owner;
    outHit->normal =
        (dist > 0.0f) ? Vector3Scale(delta, 1.0f / dist) : (Vector3){0, 1, 0};
    outHit->penetration = r - dist;
    outHit->point =
        Vector3Add(sa->center, Vector3Scale(outHit->normal, sa->radius));
  }

  return true;
}

bool SphereVsCapsule(const CollisionInstance *a, const SphereCollider *s,
                     const CollisionInstance *b, const CapsuleCollider *cap,
                     CollisionHit *outHit) {
  Vector3 closest = ClosestPointOnSegment(cap->worldA, cap->worldB, s->center);

  Vector3 delta = Vector3Subtract(s->center, closest);
  float radiusSum = s->radius + cap->radius;
  float dist2 = Vector3LengthSqr(delta);

  if (dist2 > radiusSum * radiusSum)
    return false;

  float dist = sqrtf(dist2);

  if (outHit) {
    outHit->a = a->owner;
    outHit->b = b->owner;
    outHit->normal =
        (dist > 0.0f) ? Vector3Scale(delta, 1.0f / dist) : (Vector3){0, 1, 0};
    outHit->penetration = radiusSum - dist;
    outHit->point =
        Vector3Subtract(s->center, Vector3Scale(outHit->normal, s->radius));
  }

  return true;
}

bool SphereVsAABB(const CollisionInstance *a, const SphereCollider *s,
                  const CollisionInstance *b, CollisionHit *outHit) {
  const BoundingBox *box = &b->worldBounds;

  Vector3 closest = {
      Clamp(s->center.x, box->min.x, box->max.x),
      Clamp(s->center.y, box->min.y, box->max.y),
      Clamp(s->center.z, box->min.z, box->max.z),
  };

  Vector3 delta = Vector3Subtract(s->center, closest);
  float dist2 = Vector3LengthSqr(delta);

  if (dist2 > s->radius * s->radius)
    return false;

  float dist = sqrtf(dist2);

  if (outHit) {
    outHit->a = a->owner;
    outHit->b = b->owner;
    outHit->normal =
        (dist > 0.0f) ? Vector3Scale(delta, 1.0f / dist) : (Vector3){0, 1, 0};
    outHit->penetration = s->radius - dist;
    outHit->point = closest;
  }

  return true;
}

bool AABBVsAABB(const CollisionInstance *a, const CollisionInstance *b,
                CollisionHit *outHit) {
  const BoundingBox *A = &a->worldBounds;
  const BoundingBox *B = &b->worldBounds;

  if (A->max.x < B->min.x || A->min.x > B->max.x)
    return false;
  if (A->max.y < B->min.y || A->min.y > B->max.y)
    return false;
  if (A->max.z < B->min.z || A->min.z > B->max.z)
    return false;

  if (outHit) {
    outHit->a = a->owner;
    outHit->b = b->owner;
    outHit->normal = (Vector3){0, 1, 0};
    outHit->penetration = 0.0f;
    outHit->point = Vector3Scale(Vector3Add(A->min, A->max), 0.5f);
  }

  return true;
}

bool CapsuleVsAABB(const CollisionInstance *a, const CapsuleCollider *cap,
                   const CollisionInstance *b, CollisionHit *outHit) {
  const BoundingBox *box = &b->worldBounds;

  Vector3 boxCenter = Vector3Scale(Vector3Add(box->min, box->max), 0.5f);

  Vector3 closestSeg =
      ClosestPointOnSegment(cap->worldA, cap->worldB, boxCenter);

  Vector3 closestBox = {
      Clamp(closestSeg.x, box->min.x, box->max.x),
      Clamp(closestSeg.y, box->min.y, box->max.y),
      Clamp(closestSeg.z, box->min.z, box->max.z),
  };

  Vector3 delta = Vector3Subtract(closestSeg, closestBox);
  float dist2 = Vector3LengthSqr(delta);

  if (dist2 > cap->radius * cap->radius)
    return false;

  float dist = sqrtf(dist2);

  if (outHit) {
    outHit->a = a->owner;
    outHit->b = b->owner;
    outHit->normal =
        (dist > 0.0f) ? Vector3Scale(delta, 1.0f / dist) : (Vector3){0, 1, 0};
    outHit->penetration = cap->radius - dist;
    outHit->point = closestBox;
  }

  return true;
}

bool SphereVsWallSegment(const CollisionInstance *a, const SphereCollider *s,
                         const CollisionInstance *b, const WallSegmentCollider *wall,
                         CollisionHit *outHit) {
  if (s->center.y + s->radius < wall->yBottom || s->center.y - s->radius > wall->yTop)
    return false;

  Vector3 p       = {s->center.x, 0.0f, s->center.z};
  Vector3 closest = ClosestPointOnSegment(wall->worldA, wall->worldB, p);

  float dx    = s->center.x - closest.x;
  float dz    = s->center.z - closest.z;
  float dist2 = dx*dx + dz*dz;
  float r     = s->radius + wall->radius;

  if (dist2 > r * r)
    return false;

  float dist = sqrtf(dist2);

  if (outHit) {
    outHit->a           = a->owner;
    outHit->b           = b->owner;
    outHit->normal      = (dist > 0.0f) ? (Vector3){dx/dist, 0.0f, dz/dist}
                                        : (Vector3){0.0f, 0.0f, 1.0f};
    outHit->penetration = r - dist;
    outHit->point       = (Vector3){closest.x, s->center.y, closest.z};
  }

  return true;
}

bool CapsuleVsWallSegment(const CollisionInstance *a, const CapsuleCollider *cap,
                           const CollisionInstance *b, const WallSegmentCollider *wall,
                           CollisionHit *outHit) {
  float capYMin = fminf(cap->worldA.y, cap->worldB.y) - cap->radius;
  float capYMax = fmaxf(cap->worldA.y, cap->worldB.y) + cap->radius;
  if (capYMax < wall->yBottom || capYMin > wall->yTop)
    return false;

  Vector3 capA2 = {cap->worldA.x, 0.0f, cap->worldA.z};
  Vector3 capB2 = {cap->worldB.x, 0.0f, cap->worldB.z};

  Vector3 closestCap, closestWall;
  float dist = ClosestDistSegSeg2D(capA2, capB2, wall->worldA, wall->worldB,
                                   &closestCap, &closestWall);

  float r = cap->radius + wall->radius;
  if (dist > r)
    return false;

  if (outHit) {
    float dx        = closestCap.x - closestWall.x;
    float dz        = closestCap.z - closestWall.z;
    float midY      = 0.5f * (fminf(cap->worldA.y, cap->worldB.y) +
                               fmaxf(cap->worldA.y, cap->worldB.y));
    outHit->a           = a->owner;
    outHit->b           = b->owner;
    outHit->normal      = (dist > 0.0f) ? (Vector3){dx/dist, 0.0f, dz/dist}
                                        : (Vector3){0.0f, 0.0f, 1.0f};
    outHit->penetration = r - dist;
    outHit->point       = (Vector3){closestWall.x,
                                    Clamp(midY, wall->yBottom, wall->yTop),
                                    closestWall.z};
  }

  return true;
}

bool CollisionTest(const CollisionInstance *a, const void *shapeA,
                   const CollisionInstance *b, const void *shapeB,
                   CollisionHit *outHit) {
  // printf("A layer: %u mask: %u | B layer: %u mask: %u\n", a->layerMask,
  //        a->collideMask, b->layerMask, b->collideMask);
  if ((a->collideMask & b->layerMask) == 0)
    return false;

  switch (a->type) {
  case COLLIDER_SPHERE: {
    const SphereCollider *sa = shapeA;

    if (b->type == COLLIDER_SPHERE)
      return SphereVsSphere(a, sa, b, shapeB, outHit);

    if (b->type == COLLIDER_AABB)
      return SphereVsAABB(a, sa, b, outHit);

    if (b->type == COLLIDER_CAPSULE)
      return SphereVsCapsule(a, sa, b, shapeB, outHit);

    if (b->type == COLLIDER_WALL_SEGMENT)
      return SphereVsWallSegment(a, sa, b, shapeB, outHit);
  } break;

  case COLLIDER_CAPSULE: {
    const CapsuleCollider *capA = shapeA;

    if (b->type == COLLIDER_AABB)
      return CapsuleVsAABB(a, capA, b, outHit);

    if (b->type == COLLIDER_SPHERE)
      return SphereVsCapsule(b, shapeB, a, capA, outHit);

    if (b->type == COLLIDER_WALL_SEGMENT)
      return CapsuleVsWallSegment(a, capA, b, shapeB, outHit);
  } break;

  case COLLIDER_AABB: {
    if (b->type == COLLIDER_AABB)
      return AABBVsAABB(a, b, outHit);

    if (b->type == COLLIDER_SPHERE)
      return SphereVsAABB(b, shapeB, a, outHit);

    if (b->type == COLLIDER_CAPSULE)
      return CapsuleVsAABB(b, shapeB, a, outHit);
  } break;

  case COLLIDER_WALL_SEGMENT: {
    const WallSegmentCollider *wall = shapeA;

    if (b->type == COLLIDER_SPHERE)
      return SphereVsWallSegment(b, shapeB, a, wall, outHit);

    if (b->type == COLLIDER_CAPSULE)
      return CapsuleVsWallSegment(b, shapeB, a, wall, outHit);
  } break;
  }

  return false;
}
