
#include "aabb.h"
#include "capsule.h"
#include "collision_hit.h"
#include "collision_instance.h"
#include "raylib.h"
#include "sphere.h"
#include <math.h>

static Vector3 ClosestPointOnSegment(Vector3 a, Vector3 b, Vector3 p) {
  Vector3 ab = Vector3Subtract(b, a);
  float t =
      Vector3DotProduct(Vector3Subtract(p, a), ab) / Vector3DotProduct(ab, ab);

  t = Clamp(t, 0.0f, 1.0f);
  return Vector3Add(a, Vector3Scale(ab, t));
}

bool SphereVsSphere(const CollisionInstance *a, const CollisionInstance *b,
                    CollisionHit *outHit) {
  const SphereCollider *sa = a->shape;
  const SphereCollider *sb = b->shape;

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

bool SphereVsAABB(const CollisionInstance *a, const CollisionInstance *b,
                  CollisionHit *outHit) {
  const SphereCollider *s = a->shape;
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
    outHit->normal = (Vector3){0, 1, 0}; // refine later
    outHit->penetration = 0.0f;
    outHit->point = Vector3Scale(Vector3Add(A->min, A->max), 0.5f);
  }

  return true;
}

bool CapsuleVsAABB(const CollisionInstance *a, const CollisionInstance *b,
                   CollisionHit *outHit) {
  const CapsuleCollider *cap = a->shape;
  const BoundingBox *box = &b->worldBounds;

  /* --------------------------------------------------
     1. Closest point on capsule segment to box center
  -------------------------------------------------- */

  Vector3 boxCenter = Vector3Scale(Vector3Add(box->min, box->max), 0.5f);

  Vector3 closestSeg = ClosestPointOnSegment(cap->a, cap->b, boxCenter);

  /* --------------------------------------------------
     2. Closest point on AABB to that point
  -------------------------------------------------- */

  Vector3 closestBox = {
      Clamp(closestSeg.x, box->min.x, box->max.x),
      Clamp(closestSeg.y, box->min.y, box->max.y),
      Clamp(closestSeg.z, box->min.z, box->max.z),
  };

  /* --------------------------------------------------
     3. Sphere test (radius = capsule radius)
  -------------------------------------------------- */

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

bool CollisionTest(const CollisionInstance *a, const CollisionInstance *b,
                   CollisionHit *outHit) {
  // Layer filtering
  if ((a->collideMask & b->layerMask) == 0)
    return false;

  switch (a->type) {

  case COLLIDER_SPHERE:
    if (b->type == COLLIDER_SPHERE)
      return SphereVsSphere(a, b, outHit);
    if (b->type == COLLIDER_AABB)
      return SphereVsAABB(a, b, outHit);
    break;

  case COLLIDER_CAPSULE:
    if (b->type == COLLIDER_AABB)
      return CapsuleVsAABB(a, b, outHit);
    break;

  case COLLIDER_AABB:
    if (b->type == COLLIDER_AABB)
      return AABBVsAABB(a, b, outHit);
    if (b->type == COLLIDER_SPHERE)
      return SphereVsAABB(b, a, outHit); // flip
    if (b->type == COLLIDER_CAPSULE)
      return CapsuleVsAABB(b, a, outHit); // flip
    break;

  default:
    break;
  }

  return false;
}
