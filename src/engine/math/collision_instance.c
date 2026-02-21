#include "collision_instance.h"

void Collision_UpdateAABB(CollisionInstance *ci, AABBCollider *aabb,
                          Vector3 position) {
  ci->worldBounds = AABB_ComputeWorld(aabb, position);
}

void Collision_UpdateCapsule(CollisionInstance *ci, CapsuleCollider *cap,
                             Vector3 position) {
  cap->a = position;
  cap->b = Vector3Add(position, (Vector3){0, cap->b.y, 0});
  ci->worldBounds = Capsule_ComputeAABB(cap);
}

void Collision_UpdateSphere(CollisionInstance *ci, SphereCollider *sphere,
                            Vector3 position) {
  sphere->center = position;
  ci->worldBounds = Sphere_ComputeAABB(sphere);
}
