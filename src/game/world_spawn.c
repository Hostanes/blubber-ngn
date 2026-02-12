#include "components/components.h"
#include "game.h"
#include <raylib.h>
#include <raymath.h>

#define MAX_BULLETS 2048

GameWorld GameWorldCreate(Engine *engine, world_t *world) {
  GameWorld gw = {0};
  gw.gameState = GAMESTATE_MAINMENU;

  gw.terrainModel = LoadModel("assets/models/terrain-tutorial.glb");
  // gw.terrainModel = LoadModel("assets/models/terrain-level1.glb");
  gw.terrainHeightMap =
      HeightMap_FromMesh(gw.terrainModel.meshes[0], MatrixIdentity());

  /* ---------- Component pools ---------- */

  static componentPool_t modelPool;
  static componentPool_t timerPool;

  ComponentPoolInit(&modelPool, sizeof(ModelCollection_t));
  ComponentPoolInit(&timerPool, sizeof(Timer));

  Model cube = LoadModelFromMesh(GenMeshCube(1, 1, 1));
  Model gun = LoadModel("assets/models/gun1.glb");

  /* ---------- Player archetype ---------- */

  uint32_t playerBits[] = {COMP_POSITION, COMP_VELOCITY, COMP_ORIENTATION,
                           COMP_MODEL,    COMP_TIMER,    COMP_GRAVITY,
                           COMP_ACTIVE};

  bitset_t playerMask = MakeMask(playerBits, 6);
  gw.playerArchId = WorldCreateArchetype(world, &playerMask);
  archetype_t *playerArch = WorldGetArchetype(world, gw.playerArchId);

  ArchetypeAddInline(playerArch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(playerArch, COMP_VELOCITY, sizeof(Velocity));
  ArchetypeAddInline(playerArch, COMP_ORIENTATION, sizeof(Orientation));
  ArchetypeAddInline(playerArch, COMP_ACTIVE, sizeof(Active));
  ArchetypeAddHandle(playerArch, COMP_MODEL, &engine->modelPool);
  ArchetypeAddHandle(playerArch, COMP_TIMER, &engine->timerPool);
  ArchetypeAddHandle(playerArch, COMP_COYOTETIMER, &engine->timerPool);

  gw.player = WorldCreateEntity(world, &playerMask);

  ECS_GET(world, gw.player, Position, COMP_POSITION)->value =
      (Vector3){0, 1.8f, 0};

  Active *active = ECS_GET(world, gw.player, Active, COMP_ACTIVE);
  active->value = true;

  ModelCollection_t *mc =
      ECS_GET(world, gw.player, ModelCollection_t, COMP_MODEL);

  ModelCollectionInit(mc, 2);

  ModelCollectionAdd(mc, (ModelInstance_t){.model = cube,
                                           .offset = (Vector3){0, 2, 0},
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_YAW_ONLY});

  ModelCollectionAdd(mc, (ModelInstance_t){.model = gun,
                                           .offset = (Vector3){0, -0.9f, 0},
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_FULL});

  /* ---------- Bullet archetype ---------- */

  Model bulletModel = LoadModel("assets/models/bullet.glb");

  uint32_t bulletBits[] = {COMP_POSITION, COMP_VELOCITY,   COMP_ORIENTATION,
                           COMP_MODEL,    COMP_BULLETTYPE, COMP_TIMER,
                           COMP_ACTIVE};

  bitset_t bulletMask = MakeMask(bulletBits, 7);
  gw.bulletArchId = WorldCreateArchetype(world, &bulletMask);
  archetype_t *bulletArch = WorldGetArchetype(world, gw.bulletArchId);

  ArchetypeAddInline(bulletArch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(bulletArch, COMP_VELOCITY, sizeof(Velocity));
  ArchetypeAddInline(bulletArch, COMP_ORIENTATION, sizeof(Orientation));
  ArchetypeAddInline(bulletArch, COMP_BULLETTYPE, sizeof(int));
  ArchetypeAddInline(bulletArch, COMP_ACTIVE, sizeof(Active));

  ArchetypeAddHandle(bulletArch, COMP_MODEL, &engine->modelPool);
  ArchetypeAddHandle(bulletArch, COMP_TIMER, &engine->timerPool);

  for (int i = 0; i < MAX_BULLETS; i++) {
    entity_t b = WorldCreateEntity(world, &bulletMask);

    ECS_GET(world, b, Active, COMP_ACTIVE)->value = false;

    Timer *life = ECS_GET(world, b, Timer, COMP_TIMER);
    Active *active = ECS_GET(world, b, Active, COMP_ACTIVE);
    life->value = 0.0f;
    active->value = false;

    ModelCollection_t *mc = ECS_GET(world, b, ModelCollection_t, COMP_MODEL);
    ModelCollectionInit(mc, 1);
    ModelCollectionAdd(mc, (ModelInstance_t){.model = bulletModel,
                                             .scale = (Vector3){1, 1, 1},
                                             .offset = (Vector3){0, 0, 0},
                                             .rotationMode = MODEL_ROT_FULL});
    mc->models[0].offset = (Vector3){0, 0, 0};
    mc->models[0].rotation = (Vector3){0, 0, 0};
  }

  /* ---------- Box archetype ---------- */

  uint32_t boxBits[] = {COMP_POSITION, COMP_ORIENTATION, COMP_MODEL,
                        COMP_ACTIVE};
  bitset_t boxMask = MakeMask(boxBits, 3);

  gw.obstacleArchId = WorldCreateArchetype(world, &boxMask);
  archetype_t *obsatcleArch = WorldGetArchetype(world, gw.obstacleArchId);

  ArchetypeAddInline(obsatcleArch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(obsatcleArch, COMP_ORIENTATION, sizeof(Orientation));
  ArchetypeAddInline(obsatcleArch, COMP_ACTIVE, sizeof(Active));
  ArchetypeAddHandle(obsatcleArch, COMP_MODEL, &engine->modelPool);

  for (int i = 0; i < 5000; ++i) {
    entity_t box = WorldCreateEntity(world, &boxMask);
    ECS_GET(world, box, Position, COMP_POSITION)->value =
        (Vector3){i * 2.0f, 0.5f, 5.0f};

    Active *active = ECS_GET(world, box, Active, COMP_ACTIVE);
    active->value = true;

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
