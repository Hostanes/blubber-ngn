#include "collision_instance.h"

void Collision_UpdateInstance(CollisionInstance *ci, Vector3 position,
                              Quaternion rotation) {
  (void)rotation; // AABB ignores rotation for now

  switch (ci->type) {
  case COLLIDER_AABB: {
    AABBCollider *aabb = (AABBCollider *)ci->shape;
    ci->worldBounds = AABB_ComputeWorld(aabb, position);
  } break;

  case COLLIDER_CAPSULE: {
    CapsuleCollider *cap = (CapsuleCollider *)ci->shape;

    cap->a = position;
    cap->b = Vector3Add(position, (Vector3){0, 1.8f, 0});

    ci->worldBounds = Capsule_ComputeAABB(cap);
  } break;

  case COLLIDER_SPHERE: {
    SphereCollider *s = (SphereCollider *)ci->shape;
    s->center = position;
    ci->worldBounds = Sphere_ComputeAABB(s);
  } break;
  }
}
