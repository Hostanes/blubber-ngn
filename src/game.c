#include "game.h"
#include "raylib.h"
#include "raymath.h"
#include <stdlib.h>
#include <string.h>

//----------------------------------------
// Helper: initialize an empty ModelCollection
//----------------------------------------
static ModelCollection_t InitModelCollection(int countModels) {
  ModelCollection_t mc = {0};
  mc.countModels = countModels;

  mc.models = MemAlloc(sizeof(Model) * countModels);
  mc.offsets = MemAlloc(sizeof(Vector3) * countModels);
  mc.orientations = MemAlloc(sizeof(Orientation) * countModels);
  mc.parentIds = MemAlloc(sizeof(int) * countModels);
  mc.rotLocks = MemAlloc(sizeof(bool *) * countModels);

  for (int i = 0; i < countModels; i++) {
    mc.rotLocks[i] = MemAlloc(sizeof(bool) * 3); // yaw, pitch, roll
    for (int j = 0; j < 3; j++)
      mc.rotLocks[i][j] = true;

    mc.parentIds[i] = -1;
    mc.offsets[i] = (Vector3){0};
    mc.orientations[i] = (Orientation){0};
  }

  return mc;
}

//----------------------------------------
// Terrain Initialization
//----------------------------------------

//----------------------------------------
// Terrain Initialization
//----------------------------------------
void InitTerrain(GameState_t *gs, Texture2D sandTex) {
  Terrain_t *terrain = &gs->terrain;

  const int width = TERRAIN_SIZE;
  const int depth = TERRAIN_SIZE;

  // --- Generate height data ---
  for (int z = 0; z < depth; z++) {
    for (int x = 0; x < width; x++) {
      // You can make more complex patterns here (Perlin noise, etc.)
      terrain->heights[z * width + x] =
          sinf(x * 0.3f) * 3.0f + cosf(z * 0.2f) * 2.0f;
    }
  }

  // --- Allocate vertex data ---
  int vertexCount = width * depth;
  int quadCount = (width - 1) * (depth - 1);
  int triCount = quadCount * 2;

  Vector3 *verts = MemAlloc(sizeof(Vector3) * vertexCount);
  Vector2 *texcoords = MemAlloc(sizeof(Vector2) * vertexCount);
  Vector3 *normals = MemAlloc(sizeof(Vector3) * vertexCount);
  unsigned short *indices = MemAlloc(sizeof(unsigned short) * triCount * 3);

  // --- Build vertex positions, normals, and texcoords ---
  for (int z = 0; z < depth; z++) {
    for (int x = 0; x < width; x++) {
      int idx = z * width + x;
      float worldX = (x - width / 2.0f) * TERRAIN_SCALE;
      float worldZ = (z - depth / 2.0f) * TERRAIN_SCALE;
      float height = terrain->heights[idx];
      verts[idx] = (Vector3){worldX, height, worldZ};

      texcoords[idx] = (Vector2){(float)x / width, (float)z / depth};
      normals[idx] = (Vector3){0, 1, 0}; // TODO crude normal
    }
  }

  // --- Build triangle indices ---
  int i = 0;
  for (int z = 0; z < depth - 1; z++) {
    for (int x = 0; x < width - 1; x++) {
      int topLeft = z * width + x;
      int topRight = topLeft + 1;
      int bottomLeft = topLeft + width;
      int bottomRight = bottomLeft + 1;

      indices[i++] = topLeft;
      indices[i++] = bottomLeft;
      indices[i++] = topRight;

      indices[i++] = topRight;
      indices[i++] = bottomLeft;
      indices[i++] = bottomRight;
    }
  }

  // --- Build Raylib mesh ---
  memset(&terrain->mesh, 0, sizeof(Mesh));
  terrain->mesh.vertexCount = vertexCount;
  terrain->mesh.triangleCount = triCount;

  terrain->mesh.vertices = (float *)MemAlloc(vertexCount * 3 * sizeof(float));
  terrain->mesh.texcoords = (float *)MemAlloc(vertexCount * 2 * sizeof(float));
  terrain->mesh.normals = (float *)MemAlloc(vertexCount * 3 * sizeof(float));
  terrain->mesh.indices = indices;

  for (int v = 0; v < vertexCount; v++) {
    terrain->mesh.vertices[v * 3 + 0] = verts[v].x;
    terrain->mesh.vertices[v * 3 + 1] = verts[v].y;
    terrain->mesh.vertices[v * 3 + 2] = verts[v].z;

    terrain->mesh.texcoords[v * 2 + 0] = texcoords[v].x;
    terrain->mesh.texcoords[v * 2 + 1] = texcoords[v].y;

    terrain->mesh.normals[v * 3 + 0] = normals[v].x;
    terrain->mesh.normals[v * 3 + 1] = normals[v].y;
    terrain->mesh.normals[v * 3 + 2] = normals[v].z;
  }

  UploadMesh(&terrain->mesh, true);

  // --- Create model and apply texture ---
  terrain->model = LoadModelFromMesh(terrain->mesh);
  terrain->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = sandTex;
  terrain->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
  terrain->model.materials[0].shader = LoadShader(NULL, NULL); // force unlit

  // Cleanup CPU-side memory
  MemFree(verts);
  MemFree(normals);
  MemFree(texcoords);
}

//----------------------------------------
// Game initialization
//----------------------------------------
GameState_t InitGame(void) {
  GameState_t gs = {0};
  gs.entities.count = MAX_ENTITIES;
  gs.playerId = 0;
  gs.pHeadbobTimer = 0.0f;

  // Allocate all entity arrays
  gs.entities.types = MemAlloc(sizeof(EntityType_t) * MAX_ENTITIES);
  gs.entities.positions = MemAlloc(sizeof(Vector3) * MAX_ENTITIES);
  gs.entities.velocities = MemAlloc(sizeof(Vector3) * MAX_ENTITIES);
  gs.entities.stepCycle = MemAlloc(sizeof(float) * MAX_ENTITIES);
  gs.entities.prevStepCycle = MemAlloc(sizeof(float) * MAX_ENTITIES);
  gs.entities.stepRate = MemAlloc(sizeof(float) * MAX_ENTITIES);
  gs.entities.modelCollections =
      MemAlloc(sizeof(ModelCollection_t) * MAX_ENTITIES);
  gs.entities.collisionCollections =
      MemAlloc(sizeof(ModelCollection_t) * MAX_ENTITIES);
  gs.entities.hitboxCollections =
      MemAlloc(sizeof(ModelCollection_t) * MAX_ENTITIES);

  // Initialize terrain properly

  Texture2D sandTex = LoadTexture("assets/textures/xtSand.png");

  InitTerrain(&gs, sandTex);

  //----------------------------------------
  // PLAYER ENTITY
  //----------------------------------------
  int playerId = 0;
  gs.entities.types[playerId] = ENTITY_PLAYER;
  gs.entities.positions[playerId] = (Vector3){0, 10, 0};
  gs.entities.velocities[playerId] = (Vector3){0};
  gs.entities.stepCycle[playerId] = 0;
  gs.entities.prevStepCycle[playerId] = 0;
  gs.entities.stepRate[playerId] = 2.0f;

  ModelCollection_t *pmc = &gs.entities.modelCollections[playerId];
  *pmc = InitModelCollection(3);

  pmc->models[0] = LoadModel("assets/models/raptor1-legs.glb");
  Texture2D mechTex = LoadTexture("assets/textures/legs.png");
  pmc->models[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = mechTex;
  pmc->offsets[0] = (Vector3){0, 0, 0};

  Mesh legChild = GenMeshCube(10.0f, 2.0f, 2.0f);
  pmc->models[2] = LoadModelFromMesh(legChild);
  pmc->models[2].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = PURPLE;
  pmc->offsets[2] = (Vector3){2, 8, -4};

  pmc->parentIds[0] = -1;
  pmc->parentIds[1] = -1;
  pmc->parentIds[2] = 1;

  pmc->rotLocks[2][0] = true;
  pmc->rotLocks[2][1] = false;
  pmc->rotLocks[2][2] = true;

  // Movement collision shape
  gs.entities.collisionCollections[playerId] = InitModelCollection(1);
  Mesh moveBox = GenMeshCube(4, 4, 15);
  gs.entities.collisionCollections[playerId].offsets[0] = (Vector3){0, 5, 0};
  gs.entities.collisionCollections[playerId].models[0] =
      LoadModelFromMesh(moveBox);

  // Hitbox
  gs.entities.hitboxCollections[playerId] = InitModelCollection(1);
  Mesh hitbox1 = GenMeshCube(4, 4, 7);
  gs.entities.hitboxCollections[playerId].models[0] =
      LoadModelFromMesh(hitbox1);
  gs.entities.hitboxCollections[playerId].offsets[0] = (Vector3){0, 2, 0};

  //----------------------------------------
  // MECH ENTITY
  //----------------------------------------
  int cubeId = 1;
  gs.entities.types[cubeId] = ENTITY_MECH;
  gs.entities.positions[cubeId] = (Vector3){0, 10, 40};
  gs.entities.velocities[cubeId] = (Vector3){0};

  ModelCollection_t *cmc = &gs.entities.modelCollections[cubeId];
  *cmc = InitModelCollection(2);
  Mesh cubeMesh = GenMeshCube(50, 20, 20);
  cmc->models[0] = LoadModelFromMesh(cubeMesh);
  cmc->models[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = BLUE;
  cmc->offsets[0] = (Vector3){0, 0, 0};

  Mesh cubeTurret = GenMeshCube(10, 5, 10);
  cmc->models[1] = LoadModelFromMesh(cubeTurret);
  cmc->models[1].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = GREEN;
  cmc->offsets[1] = (Vector3){0, 15, 0};

  gs.entities.collisionCollections[cubeId] = InitModelCollection(1);
  Mesh cubeMoveBox = GenMeshCube(50, 20, 20);
  gs.entities.collisionCollections[cubeId].models[0] =
      LoadModelFromMesh(cubeMoveBox);
  gs.entities.collisionCollections[cubeId].offsets[0] = (Vector3){0, 0, 0};

  //----------------------------------------
  // TANK ENTITY
  //----------------------------------------
  int tankId = 2;
  gs.entities.types[tankId] = ENTITY_TANK;
  gs.entities.positions[tankId] = (Vector3){0, 10, -30};
  gs.entities.velocities[tankId] = (Vector3){0};

  ModelCollection_t *smc = &gs.entities.modelCollections[tankId];
  *smc = InitModelCollection(1);
  Mesh sphereMesh = GenMeshSphere(8.0f, 16, 16);
  smc->models[0] = LoadModelFromMesh(sphereMesh);
  smc->models[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = RED;

  return gs;
}
