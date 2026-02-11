#include "systems.h"

void RenderSystem(world_t *world, archetype_t *arch) {
#pragma omp parallel for if (arch->count >= OMP_MIN_ITERATIONS)
  for (uint32_t i = 0; i < arch->count; ++i) {
    entity_t e = arch->entities[i];

    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
    Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
    ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);

    // Entity yaw (world-space)
    Matrix T_world = MatrixTranslate(pos->value.x, pos->value.y, pos->value.z);
    Matrix R_entityYaw = MatrixRotateY(ori->yaw);
    Matrix entityTransform = MatrixMultiply(R_entityYaw, T_world);

    for (uint32_t m = 0; m < mc->count; ++m) {
      ModelInstance_t *mi = &mc->models[m];

      // Local model transform
      Matrix local =
          MatrixMultiply(MatrixScale(mi->scale.x, mi->scale.y, mi->scale.z),
                         MatrixRotateXYZ(mi->rotation));

      local = MatrixMultiply(
          MatrixTranslate(mi->offset.x, mi->offset.y, mi->offset.z), local);

      // Final transform
      Matrix transform = MatrixMultiply(local, entityTransform);

      mi->model.transform = transform;
      DrawModel(mi->model, (Vector3){0, 0, 0}, 1.0f, WHITE);
    }
  }
}
