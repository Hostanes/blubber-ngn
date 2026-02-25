#include "collision_instance.h"

void Collision_UpdateAABB(CollisionInstance *ci, AABBCollider *aabb,
                          Vector3 position) {
  ci->worldBounds = AABB_ComputeWorld(aabb, position);
}

void Collision_UpdateSphere(CollisionInstance *ci, SphereCollider *sphere,
                            Vector3 position) {
  sphere->center = position;
  ci->worldBounds = Sphere_ComputeAABB(sphere);
}
