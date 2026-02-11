#include "game.h"

static bitset_t MakeMask(uint32_t *bits, uint32_t count) {
  bitset_t mask;
  BitsetInit(&mask, 64);
  for (uint32_t i = 0; i < count; ++i)
    BitsetSet(&mask, bits[i]);
  return mask;
}

GameWorld GameWorldCreate(Engine *engine, world_t *world) {
  GameWorld gw = {0};
  gw.gameState = GAMESTATE_MAINMENU;
  /* ---------- Component pools ---------- */

  static componentPool_t modelPool;
  static componentPool_t timerPool;

  ComponentPoolInit(&modelPool, sizeof(ModelCollection_t));
  ComponentPoolInit(&timerPool, sizeof(Timer));

  Model cube = LoadModelFromMesh(GenMeshCube(1, 1, 1));
  Model gun = LoadModel("assets/models/gun1.glb");

  /* ---------- Player archetype ---------- */

  uint32_t playerBits[] = {COMP_POSITION, COMP_VELOCITY, COMP_ORIENTATION,
                           COMP_MODEL, COMP_TIMER};

  bitset_t playerMask = MakeMask(playerBits, 5);
  gw.playerArchId = WorldCreateArchetype(world, &playerMask);
  archetype_t *playerArch = WorldGetArchetype(world, gw.playerArchId);

  ArchetypeAddInline(playerArch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(playerArch, COMP_VELOCITY, sizeof(Velocity));
  ArchetypeAddInline(playerArch, COMP_ORIENTATION, sizeof(Orientation));
  ArchetypeAddHandle(playerArch, COMP_MODEL, &engine->modelPool);
  ArchetypeAddHandle(playerArch, COMP_TIMER, &engine->timerPool);

  gw.player = WorldCreateEntity(world, &playerMask);

  ECS_GET(world, gw.player, Position, COMP_POSITION)->value =
      (Vector3){0, 1.8f, 0};

  ModelCollection_t *mc =
      ECS_GET(world, gw.player, ModelCollection_t, COMP_MODEL);

  ModelCollectionInit(mc, 2);

  ModelCollectionAdd(mc, (ModelInstance_t){.model = cube,
                                           .offset = (Vector3){0, -2, 0},
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_YAW_ONLY});

  ModelCollectionAdd(mc, (ModelInstance_t){.model = gun,
                                           .offset = (Vector3){0, -0.9f, 0},
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_FULL});

  /* ---------- Obstacle archetype ---------- */

  /* ---------- Box archetype ---------- */

  uint32_t boxBits[] = {COMP_POSITION, COMP_ORIENTATION, COMP_MODEL};
  bitset_t boxMask = MakeMask(boxBits, 3);

  gw.obstacleArchId = WorldCreateArchetype(world, &boxMask);
  archetype_t *obsatcleArch = WorldGetArchetype(world, gw.obstacleArchId);

  ArchetypeAddInline(obsatcleArch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(obsatcleArch, COMP_ORIENTATION, sizeof(Orientation));
  ArchetypeAddHandle(obsatcleArch, COMP_MODEL, &engine->modelPool);

  for (int i = 0; i < 5000; ++i) {
    entity_t box = WorldCreateEntity(world, &boxMask);
    ECS_GET(world, box, Position, COMP_POSITION)->value =
        (Vector3){i * 2.0f, 0.5f, 5.0f};
    ModelCollection_t *mc = ECS_GET(world, box, ModelCollection_t, COMP_MODEL);
    ModelCollectionInit(mc, 2);
    ModelCollectionAdd(mc, (ModelInstance_t){.model = cube,
                                             .offset = (Vector3){0, 0, 0},
                                             .rotation = (Vector3){0, 0, 0},
                                             .scale = (Vector3){1, 1, 1},
                                             .rotationMode = MODEL_ROT_FULL});
    ModelCollectionAdd(mc, (ModelInstance_t){.model = gun,
                                             .offset = (Vector3){0, 0, 0},
                                             .rotation = (Vector3){0, 0, 0},
                                             .scale = (Vector3){1, 1, 1},
                                             .rotationMode = MODEL_ROT_FULL});
  }

  return gw;
}
