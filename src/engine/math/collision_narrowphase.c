
#include "aabb.h"
#include "capsule.h"
#include "collision_instance.h"
#include "raylib.h"
#include "sphere.h"

bool CollisionInstance_Test(const CollisionInstance_t *a,
                            const CollisionInstance_t *b) {
  if (!CheckCollisionBoxes(a->worldBounds, b->worldBounds))
    return false; // broadphase reject

  switch (a->type) {
  case COLLIDER_AABB:
    if (b->type == COLLIDER_AABB)
      return CheckCollisionBoxes(((AABBCollider *)a->shape)->world,
                                 ((AABBCollider *)b->shape)->world);

    if (b->type == COLLIDER_SPHERE)
      return CheckCollisionBoxSphere(((AABBCollider *)a->shape)->world,
                                     ((SphereCollider *)b->shape)->worldCenter,
                                     ((SphereCollider *)b->shape)->radius);
    break;

  case COLLIDER_CAPSULE:
    // custom implementation later
    break;

  default:
    break;
  }

  return false;
}
