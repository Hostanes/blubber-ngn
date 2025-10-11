
// game.c
// Implements game state initialization

#include "game.h"
#include "raylib.h"

GameState_t InitGame(void) {
  GameState_t gs = {0};

  gs.entities.count = MAX_ENTITIES;

  gs.entities.types =
      (EntityType_t *)MemAlloc(sizeof(EntityType_t) * MAX_ENTITIES);

  gs.entities.positions = (Vector3 *)MemAlloc(sizeof(Vector3) * MAX_ENTITIES);
  gs.entities.velocities = (Vector3 *)MemAlloc(sizeof(Vector3) * MAX_ENTITIES);
  gs.entities.stepCycle = (float *)MemAlloc(sizeof(float) * MAX_ENTITIES);
  gs.entities.prevStepCycle = (float *)MemAlloc(sizeof(float) * MAX_ENTITIES);
  gs.entities.stepRate = (float *)MemAlloc(sizeof(float) * MAX_ENTITIES);
  gs.entities.modelCollections =
      MemAlloc(sizeof(ModelCollection_t) * MAX_ENTITIES);

  gs.playerId = 0;
  gs.pHeadbobTimer = 0.0f;

  // ========= PLAYER ENTITY =========
  gs.entities.types[0] = ENTITY_PLAYER;
  gs.entities.positions[0] = (Vector3){0, 1, 0};
  gs.entities.velocities[0] = (Vector3){0, 0, 0};
  gs.entities.stepCycle[0] = 0.0f;
  gs.entities.prevStepCycle[0] = 0.0f;
  gs.entities.stepRate[0] = 2.0f;

  ModelCollection_t *pmc = &gs.entities.modelCollections[0];
  *pmc = (ModelCollection_t){0};

  pmc->countModels = 2;
  pmc->models = MemAlloc(sizeof(Model) * pmc->countModels);
  pmc->offsets = MemAlloc(sizeof(Vector3) * pmc->countModels);
  pmc->orientations = MemAlloc(sizeof(Orientation) * pmc->countModels);
  pmc->parentIds = MemAlloc(sizeof(int) * pmc->countModels);

  pmc->parentIds[0] = -1;
  pmc->parentIds[1] = -1;

  pmc->models[0] = LoadModel("assets/models/raptor1-legs.glb");
  Texture2D mechTex = LoadTexture("assets/textures/legs.png");
  pmc->models[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = mechTex;

  pmc->offsets[0] = (Vector3){0, 0, 0};
  pmc->orientations[0] = (Orientation){0, 0, 0};

  // ========= CUBE ENTITY =========
  int cubeId = 1;
  gs.entities.types[cubeId] = ENTITY_MECH;
  gs.entities.positions[cubeId] = (Vector3){0, 10, 40}; // right of player
  gs.entities.velocities[cubeId] = (Vector3){0, 0, 0};

  ModelCollection_t *cmc = &gs.entities.modelCollections[cubeId];
  *cmc = (ModelCollection_t){0};

  cmc->countModels = 3;
  cmc->models = MemAlloc(sizeof(Model) * cmc->countModels);
  cmc->offsets = MemAlloc(sizeof(Vector3) * cmc->countModels);
  cmc->orientations = MemAlloc(sizeof(Orientation) * cmc->countModels);
  cmc->parentIds = MemAlloc(sizeof(int) * cmc->countModels);
  cmc->parentIds[0] = -1;
  cmc->parentIds[1] = -1;
  cmc->parentIds[2] = 1;

  Mesh cubeMesh = GenMeshCube(50.0f, 20.0f, 20.0f);
  cmc->models[0] = LoadModelFromMesh(cubeMesh);
  cmc->models[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = BLUE;

  cmc->offsets[0] = (Vector3){0, 0, 0};
  cmc->orientations[0] = (Orientation){0, 0, 0};

  Mesh cubeMesh2 = GenMeshCube(10.0f, 10.0f, 10.0f);
  cmc->models[1] = LoadModelFromMesh(cubeMesh2);
  cmc->models[1].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = GREEN;

  cmc->offsets[1] = (Vector3){0, 25, 0};
  cmc->orientations[1] = (Orientation){0, 0, 0};

  Mesh cubeMesh3 = GenMeshCube(2.0f, 2.0f, 10.0f);
  cmc->models[2] = LoadModelFromMesh(cubeMesh3);
  cmc->models[2].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = GREEN;

  cmc->offsets[2] = (Vector3){10, 25, 0};
  cmc->orientations[2] = (Orientation){0, 0, 0};

  // ========= SPHERE ENTITY =========
  int sphereId = 2;
  gs.entities.types[sphereId] = ENTITY_TANK;
  gs.entities.positions[sphereId] = (Vector3){0, 10, -30}; // left of player
  gs.entities.velocities[sphereId] = (Vector3){0, 0, 0};

  ModelCollection_t *smc = &gs.entities.modelCollections[sphereId];
  *smc = (ModelCollection_t){0};

  smc->models = MemAlloc(sizeof(Model) * 1);
  smc->offsets = MemAlloc(sizeof(Vector3) * 1);
  smc->orientations = MemAlloc(sizeof(Orientation) * 1);

  smc->parentIds = MemAlloc(sizeof(int) * smc->countModels);
  smc->parentIds[0] = -1;

  Mesh sphereMesh = GenMeshSphere(8.0f, 16, 16); // smoother sphere
  smc->models[0] = LoadModelFromMesh(sphereMesh);
  smc->models[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = RED;

  smc->offsets[0] = (Vector3){0, 0, 0};
  smc->orientations[0] = (Orientation){0, 0, 0};

  return gs;
}
