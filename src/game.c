
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
void InitTerrain(GameState_t *gs, Texture2D sandTex) {
  Terrain_t *terrain = &gs->terrain;

  const int width = TERRAIN_SIZE;
  const int depth = TERRAIN_SIZE;

  // --- Generate heightmap ---
  for (int z = 0; z < depth; z++) {
    for (int x = 0; x < width; x++) {
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

  // --- Build vertices ---
  for (int z = 0; z < depth; z++) {
    for (int x = 0; x < width; x++) {
      int idx = z * width + x;
      float worldX = (x - width / 2.0f) * TERRAIN_SCALE;
      float worldZ = (z - depth / 2.0f) * TERRAIN_SCALE;
      float height = terrain->heights[idx];
      verts[idx] = (Vector3){worldX, height, worldZ};

      texcoords[idx] = (Vector2){(float)x / width, (float)z / depth};
      normals[idx] = (Vector3){0, 1, 0};
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

  // --- Build mesh ---
  memset(&terrain->mesh, 0, sizeof(Mesh));
  terrain->mesh.vertexCount = vertexCount;
  terrain->mesh.triangleCount = triCount;

  terrain->mesh.vertices = MemAlloc(sizeof(float) * vertexCount * 3);
  terrain->mesh.texcoords = MemAlloc(sizeof(float) * vertexCount * 2);
  terrain->mesh.normals = MemAlloc(sizeof(float) * vertexCount * 3);
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

  terrain->model = LoadModelFromMesh(terrain->mesh);
  terrain->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = sandTex;
  terrain->model.materials[0].maps[MATERIAL_MAP_DIFFUSE].color = WHITE;
  terrain->model.materials[0].shader = LoadShader(NULL, NULL);

  MemFree(verts);
  MemFree(normals);
  MemFree(texcoords);
}

//----------------------------------------
// Game Initialization (ECS-style)
//----------------------------------------
GameState_t InitGame(void) {
  GameState_t gs = {0};

  //----------------------------------------
  // Entity Manager setup
  //----------------------------------------
  gs.em.count = 0;
  memset(gs.em.alive, 0, sizeof(gs.em.alive));
  memset(gs.em.masks, 0, sizeof(gs.em.masks));

  //----------------------------------------
  // Player ID
  //----------------------------------------
  gs.playerId = 0;
  gs.state = STATE_INLEVEL;
  gs.pHeadbobTimer = 0.0f;

  //----------------------------------------
  // Load terrain texture & generate terrain
  //----------------------------------------
  Texture2D sandTex = LoadTexture("assets/textures/xtSand.png");
  InitTerrain(&gs, sandTex);

  //----------------------------------------
  // Add PLAYER ENTITY
  //----------------------------------------
  entity_t player = gs.em.count++;
  gs.em.alive[player] = 1;
  gs.em.masks[player] =
      C_POSITION | C_VELOCITY | C_MODEL | C_COLLISION | C_HITBOX | C_PLAYER_TAG;

  gs.components.positions[player] = (Vector3){0, 10, 0};
  gs.components.velocities[player] = (Vector3){0};
  gs.components.stepCycle[player] = 0;
  gs.components.prevStepCycle[player] = 0;
  gs.components.stepRate[player] = 2.0f;
  gs.components.types[player] = ENTITY_PLAYER;

  // Model collection
  ModelCollection_t *pmc = &gs.components.modelCollections[player];
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

  // Collision
  ModelCollection_t *col = &gs.components.collisionCollections[player];
  *col = InitModelCollection(1);
  Mesh moveBox = GenMeshCube(4, 8, 4);
  col->models[0] = LoadModelFromMesh(moveBox);
  col->offsets[0] = (Vector3){0, 5, 0};

  // Hitbox
  ModelCollection_t *hit = &gs.components.hitboxCollections[player];
  *hit = InitModelCollection(1);
  Mesh hitbox1 = GenMeshCube(4, 4, 7);
  hit->models[0] = LoadModelFromMesh(hitbox1);
  hit->offsets[0] = (Vector3){0, 2, 0};

  //----------------------------------------
  // Add WALL / MECH ENTITY
  //----------------------------------------
  entity_t wall = gs.em.count++;
  gs.em.alive[wall] = 1;
  gs.em.masks[wall] = C_POSITION | C_MODEL | C_COLLISION;

  gs.components.positions[wall] = (Vector3){0, 10, 40};
  gs.components.types[wall] = ENTITY_WALL;

  ModelCollection_t *wmc = &gs.components.modelCollections[wall];
  *wmc = InitModelCollection(2);
  Mesh cubeMesh = GenMeshCube(50, 20, 20);
  wmc->models[0] = LoadModelFromMesh(cubeMesh);
  wmc->models[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = BLUE;
  wmc->offsets[0] = (Vector3){0, -6, 0};
  wmc->orientations[0] = (Orientation){PI / 4.0f, 0, 0};

  Mesh cubeTurret = GenMeshCube(10, 5, 10);
  wmc->models[1] = LoadModelFromMesh(cubeTurret);
  wmc->models[1].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = GREEN;
  wmc->offsets[1] = (Vector3){0, 15, 0};

  wmc->parentIds[1] = 0;

  ModelCollection_t *wCol = &gs.components.collisionCollections[wall];
  *wCol = InitModelCollection(1);
  Mesh cubeMoveBox = GenMeshCube(50, 20, 20);
  wCol->models[0] = LoadModelFromMesh(cubeMoveBox);
  wCol->offsets[0] = (Vector3){0, -6, 0};
  wCol->orientations[0] = (Orientation){PI / 4.0f, 0, 0};

  //----------------------------------------
  // Add TANK ENTITY
  //----------------------------------------
  entity_t tank = gs.em.count++;
  gs.em.alive[tank] = 1;
  gs.em.masks[tank] = C_POSITION | C_MODEL;

  gs.components.positions[tank] = (Vector3){0, 10, -30};
  gs.components.types[tank] = ENTITY_TANK;

  ModelCollection_t *tmc = &gs.components.modelCollections[tank];
  *tmc = InitModelCollection(1);
  Mesh sphereMesh = GenMeshSphere(8.0f, 16, 16);
  tmc->models[0] = LoadModelFromMesh(sphereMesh);
  tmc->models[0].materials[0].maps[MATERIAL_MAP_DIFFUSE].color = RED;

  return gs;
}
