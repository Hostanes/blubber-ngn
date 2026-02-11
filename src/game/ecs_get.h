#define ECS_GET(world, entity, Type, ID)                                       \
  ((Type *)WorldGetComponent(world, entity, ID))

#define OMP_MIN_ITERATIONS 1024
