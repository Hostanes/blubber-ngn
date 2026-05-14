#include "editor.h"
#include "../engine/util/json_reader.h"
#include "../engine/math/capsule.h"
#include "rlgl.h"
#include "components/renderable.h"
#include "game.h"
#include "level_creater_helper.h"
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static float s_navHeightCache[180][180];
static bool  s_navHeightCacheValid = false;

static void NavmapPathFromLevel(const char *levelPath, char *out, int maxLen) {
  strncpy(out, levelPath, maxLen - 1);
  out[maxLen - 1] = '\0';
  char *dot = strrchr(out, '.');
  if (dot) strcpy(dot, ".navmap.png");
  else strncat(out, ".navmap.png", maxLen - (int)strlen(out) - 1);
}

/* ---- Editor-side model cache (load-on-demand, persists across editor sessions) ---- */
#define ED_MODEL_CACHE_CAP 32
typedef struct { char path[256]; Model model; bool loaded; } EdModelEntry;
static EdModelEntry s_edModels[ED_MODEL_CACHE_CAP];
static int          s_edModelCount = 0;

static Model EditorGetModel(const char *path) {
  for (int i = 0; i < s_edModelCount; i++)
    if (strcmp(s_edModels[i].path, path) == 0) return s_edModels[i].model;
  if (s_edModelCount < ED_MODEL_CACHE_CAP) {
    strncpy(s_edModels[s_edModelCount].path, path, 255);
    s_edModels[s_edModelCount].model = LoadModel(path);
    s_edModels[s_edModelCount].loaded = true;
    return s_edModels[s_edModelCount++].model;
  }
  return s_edModels[0].model; // cache full
}

#define PICKER_IH   34   // item height
#define PICKER_GAP   3   // gap between items
#define PICKER_HDR  42   // header area height
#define PICKER_FTR  26   // footer area height
#define PICKER_VIS  12   // max visible items

static void ScanModelFiles(EditorState *ed) {
  ed->edModelCount = 0;
  DIR *dir = opendir("assets/models");
  if (!dir) return;
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL && ed->edModelCount < 48) {
    if (entry->d_name[0] == '.') continue;
    const char *name = entry->d_name;
    int len = (int)strlen(name);
    if (len < 5 || strcmp(name + len - 4, ".glb") != 0) continue;
    snprintf(ed->edModelPaths[ed->edModelCount], 256, "assets/models/%s", name);
    snprintf(ed->edModelNames[ed->edModelCount], 64, "%.*s", len - 4, name);
    ed->edModelCount++;
  }
  closedir(dir);
}

// Terrain picker only lists files whose names begin with "terrain"
static void ScanTerrainFiles(EditorState *ed) {
  ed->edModelCount = 0;
  DIR *dir = opendir("assets/models");
  if (!dir) return;
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL && ed->edModelCount < 48) {
    if (entry->d_name[0] == '.') continue;
    const char *name = entry->d_name;
    int len = (int)strlen(name);
    if (len < 5 || strcmp(name + len - 4, ".glb") != 0) continue;
    if (strncmp(name, "terrain", 7) != 0) continue;
    snprintf(ed->edModelPaths[ed->edModelCount], 256, "assets/models/%s", name);
    snprintf(ed->edModelNames[ed->edModelCount], 64, "%.*s", len - 4, name);
    ed->edModelCount++;
  }
  closedir(dir);
}

void EditorInit(EditorState *ed, GameWorld *gw, world_t *world) {
  WorldClear(world);

  ed->yaw   = 0.0f;
  ed->pitch = -0.5f;

  ed->camera.position  = (Vector3){0, 80, -80};
  ed->camera.up        = (Vector3){0, 1, 0};
  ed->camera.fovy      = 60.0f;
  ed->camera.projection = CAMERA_PERSPECTIVE;

  Vector3 forward = {
      cosf(ed->pitch) * sinf(ed->yaw),
      sinf(ed->pitch),
      cosf(ed->pitch) * cosf(ed->yaw),
  };
  ed->camera.target = Vector3Add(ed->camera.position, forward);

  ed->boxScale        = 5.0f;
  ed->placedCount     = 0;
  ed->hasHit          = false;
  ed->transformMode   = false;
  ed->selectedIndex   = -1;
  ed->isPaused        = false;
  ed->isSelectingLevel = false;
  ed->requestQuit     = false;
  ed->edLevelCount    = 0;
  ed->edLevelSelected = 0;

  ed->spawnerCount      = 0;
  ed->placeType         = 0;
  ed->historyTop        = 0;
  ed->propCount         = 0;
  ed->propPlaceYaw      = 0.0f;
  ed->propPlaceScale    = 3.0f;
  ed->objectPanelOpen   = false;
  ed->selectedType      = 0;
  ed->modelPickerOpen    = false;
  ed->terrainPickerOpen  = false;
  ed->modelPickerScroll  = 0;
  ed->edModelCount       = 0;
  strncpy(ed->propPlaceModel, "assets/models/obstacle.glb",
          sizeof(ed->propPlaceModel) - 1);

  HeightMap_Free(&gw->terrainHeightMap);
  gw->terrainHeightMap =
      HeightMap_FromMesh(gw->terrainModel.meshes[0], MatrixIdentity());

  ed->missionType       = MISSION_WAVES;
  ed->healthBarFade     = true;
  ed->infoBoxCount      = 0;
  ed->infoBoxHalfExtent = 2.5f;
  ed->infoBoxEditOpen   = false;
  ed->infoBoxTextLen    = 0;
  ed->infoBoxTextBuf[0] = '\0';
  ed->infoBoxDuration     = 5.0f;
  ed->infoBoxMaxTriggers  = 1;
  ed->infoBoxMarkerHeight = 0.0f;
  ed->infoBoxFontSize     = 0;

  ed->edHasSpawnPoint = false;
  ed->edSpawnPoint    = (Vector3){0, 1.8f, 0};

  ed->wallSegCount             = 0;
  ed->wallSegStep              = 0;
  ed->wallSegDialogOpen        = false;
  ed->wallSegEditExisting      = false;
  ed->wallSegHeight            = 3.0f;
  ed->wallSegRadius            = 0.3f;
  ed->wallSegBlockPlayer       = true;
  ed->wallSegBlockProjectiles  = true;

  ed->arrayDialogOpen = false;
  ed->arrayCount      = 5;
  ed->arraySpacing    = 5.0f;
  ed->arrayDir        = 2; // +Z default

  ed->targetStaticCount  = 0;
  ed->targetPatrolCount  = 0;
  ed->targetDialogOpen   = false;
  ed->targetIsPatrol     = false;
  ed->targetPatrolStep   = 0;
  ed->targetDlgHealth     = 100.0f;
  ed->targetDlgShield      = 0.0f;
  ed->targetDlgSpeed      = 5.0f;
  ed->targetDlgHealthCount = 0.0f;
  ed->targetDlgCoolantCount = 0.0f;
  ed->targetDlgYaw        = 0.0f;
  ed->targetDlgField      = 0;
  ed->targetEditExisting  = false;
  ed->targetEditIndex     = -1;

  ed->waveEditorOpen = false;
  ed->edWaveCount    = 0;

  ed->navPaintMode   = false;
  ed->navPaintType   = 0;
  ed->navPaletteOpen = false;
  ed->navBrushSize   = 1;
  if (ed->navImageLoaded) UnloadImage(ed->navImage);
  ed->navImage = GenImageColor(180, 180, WHITE);
  ImageFormat(&ed->navImage, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
  ed->navImageLoaded = true;
  s_navHeightCacheValid = false;

  ed->initialized = true;
}

static bool RaycastTerrain(EditorState *ed, GameWorld *gw, Vector3 *out) {
  Vector2 center = {(float)GetScreenWidth() / 2.0f,
                    (float)GetScreenHeight() / 2.0f};
  Ray ray = GetScreenToWorldRay(center, ed->camera);

  for (float t = 1.0f; t < 600.0f; t += 0.5f) {
    Vector3 p = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    if (p.x < -200 || p.x > 200 || p.z < -200 || p.z > 200)
      break;
    float terrainY =
        HeightMap_GetHeightCatmullRom(&gw->terrainHeightMap, p.x, p.z);
    if (p.y <= terrainY) {
      out->x = p.x;
      out->z = p.z;
      out->y = terrainY + ed->boxScale * 0.5f;
      return true;
    }
  }
  return false;
}

// Ray-picks the nearest placed box from the screen center. Returns index or -1.
static int PickBox(EditorState *ed) {
  Vector2 center = {(float)GetScreenWidth() / 2.0f,
                    (float)GetScreenHeight() / 2.0f};
  Ray ray = GetScreenToWorldRay(center, ed->camera);

  float bestT = 1e9f;
  int   best  = -1;

  for (int i = 0; i < ed->placedCount; i++) {
    EditorPlacedBox *b = &ed->placed[i];
    BoundingBox bb = {
        Vector3Subtract(b->position, Vector3Scale(b->scale, 0.5f)),
        Vector3Add(b->position,      Vector3Scale(b->scale, 0.5f)),
    };
    RayCollision col = GetRayCollisionBox(ray, bb);
    if (col.hit && col.distance < bestT) {
      bestT = col.distance;
      best  = i;
    }
  }
  return best;
}

// Ray-picks the nearest placed prop from the screen center. Returns index or -1.
static int PickProp(EditorState *ed) {
  Vector2 center = {(float)GetScreenWidth() / 2.0f,
                    (float)GetScreenHeight() / 2.0f};
  Ray ray = GetScreenToWorldRay(center, ed->camera);

  float bestT = 1e9f;
  int   best  = -1;
  for (int i = 0; i < ed->propCount; i++) {
    EditorPlacedProp *p = &ed->placedProps[i];
    BoundingBox bb = {
        Vector3Subtract(p->position, Vector3Scale(p->scale, 0.5f)),
        Vector3Add(p->position,      Vector3Scale(p->scale, 0.5f)),
    };
    RayCollision col = GetRayCollisionBox(ray, bb);
    if (col.hit && col.distance < bestT) {
      bestT = col.distance;
      best  = i;
    }
  }
  return best;
}

static int PickInfoBox(EditorState *ed) {
  Vector2 center = {(float)GetScreenWidth() / 2.0f, (float)GetScreenHeight() / 2.0f};
  Ray ray = GetScreenToWorldRay(center, ed->camera);
  float bestT = 1e9f; int best = -1;
  for (int i = 0; i < ed->infoBoxCount; i++) {
    EditorPlacedInfoBox *ib = &ed->placedInfoBoxes[i];
    float he = ib->halfExtent;
    BoundingBox bb = {
        {ib->position.x - he, ib->position.y - he, ib->position.z - he},
        {ib->position.x + he, ib->position.y + he, ib->position.z + he},
    };
    RayCollision col = GetRayCollisionBox(ray, bb);
    if (col.hit && col.distance < bestT) { bestT = col.distance; best = i; }
  }
  return best;
}

static bool PickSpawnPoint(EditorState *ed, float *outDist) {
  if (!ed->edHasSpawnPoint) return false;
  Vector2 center = {(float)GetScreenWidth() / 2.0f, (float)GetScreenHeight() / 2.0f};
  Ray ray = GetScreenToWorldRay(center, ed->camera);
  Vector3 sp = ed->edSpawnPoint;
  BoundingBox bb = {{sp.x - 1.0f, sp.y - 0.5f, sp.z - 1.0f},
                    {sp.x + 1.0f, sp.y + 4.5f,  sp.z + 1.0f}};
  RayCollision col = GetRayCollisionBox(ray, bb);
  if (col.hit) { *outDist = col.distance; return true; }
  return false;
}

static int PickSpawner(EditorState *ed) {
  Vector2 center = {(float)GetScreenWidth() / 2.0f, (float)GetScreenHeight() / 2.0f};
  Ray ray = GetScreenToWorldRay(center, ed->camera);
  float bestT = 1e9f; int best = -1;
  for (int i = 0; i < ed->spawnerCount; i++) {
    Vector3 p = ed->placedSpawners[i].position;
    BoundingBox bb = {{p.x-1.5f, p.y-1.5f, p.z-1.5f}, {p.x+1.5f, p.y+1.5f, p.z+1.5f}};
    RayCollision col = GetRayCollisionBox(ray, bb);
    if (col.hit && col.distance < bestT) { bestT = col.distance; best = i; }
  }
  return best;
}

static int PickWallSeg(EditorState *ed) {
  Vector2 center = {(float)GetScreenWidth() / 2.0f, (float)GetScreenHeight() / 2.0f};
  Ray ray = GetScreenToWorldRay(center, ed->camera);
  float bestT = 1e9f; int best = -1;
  for (int i = 0; i < ed->wallSegCount; i++) {
    EditorPlacedWallSeg *ws = &ed->placedWallSegs[i];
    BoundingBox bb = {
        {fminf(ws->ax, ws->bx) - ws->radius, ws->yBottom, fminf(ws->az, ws->bz) - ws->radius},
        {fmaxf(ws->ax, ws->bx) + ws->radius, ws->yTop,    fmaxf(ws->az, ws->bz) + ws->radius},
    };
    RayCollision col = GetRayCollisionBox(ray, bb);
    if (col.hit && col.distance < bestT) { bestT = col.distance; best = i; }
  }
  return best;
}

static int PickTargetStatic(EditorState *ed) {
  Vector2 center = {(float)GetScreenWidth()/2.0f, (float)GetScreenHeight()/2.0f};
  Ray ray = GetScreenToWorldRay(center, ed->camera);
  float bestT = 1e9f; int best = -1;
  for (int i = 0; i < ed->targetStaticCount; i++) {
    Vector3 p = ed->placedTargetsStatic[i].position;
    BoundingBox bb = {{p.x-1.2f,p.y-0.5f,p.z-1.2f},{p.x+1.2f,p.y+3.0f,p.z+1.2f}};
    RayCollision col = GetRayCollisionBox(ray, bb);
    if (col.hit && col.distance < bestT) { bestT = col.distance; best = i; }
  }
  return best;
}

static int PickTargetPatrol(EditorState *ed) {
  Vector2 center = {(float)GetScreenWidth()/2.0f, (float)GetScreenHeight()/2.0f};
  Ray ray = GetScreenToWorldRay(center, ed->camera);
  float bestT = 1e9f; int best = -1;
  for (int i = 0; i < ed->targetPatrolCount; i++) {
    Vector3 pa = ed->placedTargetsPatrol[i].pointA;
    Vector3 pb = ed->placedTargetsPatrol[i].pointB;
    BoundingBox bbA = {{pa.x-1.2f,pa.y-0.5f,pa.z-1.2f},{pa.x+1.2f,pa.y+3.0f,pa.z+1.2f}};
    BoundingBox bbB = {{pb.x-1.2f,pb.y-0.5f,pb.z-1.2f},{pb.x+1.2f,pb.y+3.0f,pb.z+1.2f}};
    RayCollision ca = GetRayCollisionBox(ray, bbA);
    RayCollision cb = GetRayCollisionBox(ray, bbB);
    float t = fminf(ca.hit ? ca.distance : 1e9f, cb.hit ? cb.distance : 1e9f);
    if (t < bestT) { bestT = t; best = i; }
  }
  return best;
}

static void SyncTargetPosition(world_t *world, entity_t e, Vector3 pos, float yaw) {
  Position    *p  = ECS_GET(world, e, Position,    COMP_POSITION);
  Orientation *o  = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
  CapsuleCollider   *cap = ECS_GET(world, e, CapsuleCollider,   COMP_CAPSULE_COLLIDER);
  CollisionInstance *ci  = ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);
  if (p)        p->value = pos;
  if (o)        o->yaw   = yaw;
  if (cap && ci) { Capsule_UpdateWorld(cap, pos); ci->worldBounds = Capsule_ComputeAABB(cap); }
}

// Pushes the placed[] entry back into its ECS entity (position, model scale, AABB).
static void SyncBoxEntity(EditorState *ed, world_t *world, int idx) {
  EditorPlacedBox *b = &ed->placed[idx];

  Position *pos = ECS_GET(world, b->entity, Position, COMP_POSITION);
  if (pos) pos->value = b->position;

  ModelCollection_t *mc =
      ECS_GET(world, b->entity, ModelCollection_t, COMP_MODEL);
  if (mc && mc->count > 0)
    mc->models[0].scale = b->scale;

  AABBCollider *aabb =
      ECS_GET(world, b->entity, AABBCollider, COMP_AABB_COLLIDER);
  CollisionInstance *ci =
      ECS_GET(world, b->entity, CollisionInstance, COMP_COLLISION_INSTANCE);
  if (aabb && ci) {
    aabb->halfExtents = Vector3Scale(b->scale, 0.5f);
    Collision_UpdateAABB(ci, aabb, b->position);
  }
}

static void ScanLevels(EditorState *ed) {
  ed->edLevelCount = 0;
  DIR *dir = opendir("assets/levels");
  if (!dir) return;
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL && ed->edLevelCount < 64) {
    const char *name = entry->d_name;
    int len = (int)strlen(name);
    if (len < 6 || strcmp(name + len - 5, ".json") != 0) continue;
    snprintf(ed->edLevelPaths[ed->edLevelCount], 256, "assets/levels/%s", name);
    snprintf(ed->edLevelNames[ed->edLevelCount], 128, "%.*s", len - 5, name);
    ed->edLevelCount++;
  }
  closedir(dir);
}

void EditorLoad(EditorState *ed, GameWorld *gw, world_t *world, const char *path) {
  for (int i = 0; i < ed->placedCount; i++) {
    entity_t e = ed->placed[i].entity;
    ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);
    if (mc) ModelCollectionFree(mc);
    WorldDestroyEntity(world, e);
  }
  ed->placedCount   = 0;
  ed->selectedIndex = -1;
  ed->spawnerCount  = 0;
  ed->propCount     = 0;
  ed->historyTop    = 0;

  char *text = LoadFileText(path);
  if (!text) return;

  // Parse terrain — load if different from current
  {
    char terrainPath[256];
    strncpy(terrainPath, gw->terrainModelPath, 255);
    JsonReadString(text, "terrain", terrainPath, sizeof(terrainPath));
    if (strcmp(terrainPath, gw->terrainModelPath) != 0) {
      UnloadModel(gw->terrainModel);
      gw->terrainModel = LoadModel(terrainPath);
      strncpy(gw->terrainModelPath, terrainPath, sizeof(gw->terrainModelPath)-1);
      HeightMap_Free(&gw->terrainHeightMap);
      gw->terrainHeightMap = HeightMap_FromMesh(gw->terrainModel.meshes[0], MatrixIdentity());
      s_navHeightCacheValid = false;
    }
  }

  // Mission type
  {
    char missionBuf[32] = "waves";
    JsonReadString(text, "mission", missionBuf, sizeof(missionBuf));
    ed->missionType =
        (strcmp(missionBuf, "exploration") == 0) ? MISSION_EXPLORATION : MISSION_WAVES;
  }

  // Health bar fade
  {
    float hpf = 1.0f;
    JsonReadFloat(text, "hpfade", &hpf);
    ed->healthBarFade = (hpf != 0.0f);
  }

  // Wave composition
  {
    ed->edWaveCount = 0;
    const char *p = strstr(text, "\"waves\":");
    if (p) {
      p = strchr(p, '[');
      if (p) { p++;
        while (*p && *p != ']' && ed->edWaveCount < MAX_WAVES) {
          while (*p && *p != '{' && *p != ']') p++;
          if (*p != '{') break;
          const char *obj = p;
          while (*p && *p != '}') p++;
          if (!*p) break; p++;
          int len = (int)(p - obj);
          if (len >= 128) continue;
          char buf[128]; memcpy(buf, obj, len); buf[len] = '\0';
          float g=0,r=0,m=0,d=0;
          JsonReadFloat(buf,"g",&g); JsonReadFloat(buf,"r",&r);
          JsonReadFloat(buf,"m",&m); JsonReadFloat(buf,"d",&d);
          int wi = ed->edWaveCount++;
          ed->edWaves[wi][0]=(int)g; ed->edWaves[wi][1]=(int)r;
          ed->edWaves[wi][2]=(int)m; ed->edWaves[wi][3]=(int)d;
        }
      }
    }
  }

  // Load navmap — derive path from level path, allow JSON override
  {
    char navPath[256];
    NavmapPathFromLevel(path, navPath, sizeof(navPath));
    JsonReadString(text, "navmap", navPath, sizeof(navPath));
    if (ed->navImageLoaded) UnloadImage(ed->navImage);
    ed->navImage = LoadImage(navPath);
    if (!ed->navImage.data || ed->navImage.width != 180 || ed->navImage.height != 180) {
      if (ed->navImage.data) UnloadImage(ed->navImage);
      ed->navImage = GenImageColor(180, 180, WHITE);
    }
    ImageFormat(&ed->navImage, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    ed->navImageLoaded = true;
    s_navHeightCacheValid = false;
  }

  // Parse boxes
  {
    const char *p = strstr(text, "\"boxes\"");
    if (p) {
      p = strchr(p, '[');
      if (p) { p++;
        while (*p && *p != ']' && ed->placedCount < EDITOR_MAX_BOXES) {
          while (*p && *p != '{' && *p != ']') p++;
          if (*p != '{') break;
          const char *obj = p;
          while (*p && *p != '}') p++;
          if (!*p) break; p++;
          int len = (int)(p - obj);
          if (len >= 512) continue;
          char buf[512]; memcpy(buf, obj, len); buf[len] = '\0';
          float x=0,y=0,z=0,sx=5,sy=5,sz=5;
          JsonReadFloat(buf,"x",&x); JsonReadFloat(buf,"y",&y); JsonReadFloat(buf,"z",&z);
          JsonReadFloat(buf,"sx",&sx); JsonReadFloat(buf,"sy",&sy); JsonReadFloat(buf,"sz",&sz);
          Vector3 pos={x,y,z}, scale={sx,sy,sz};
          entity_t e = SpawnBoxModel(world, gw, pos, scale);
          ed->placed[ed->placedCount++] = (EditorPlacedBox){.entity=e,.position=pos,.scale=scale};
        }
      }
    }
  }

  // Parse spawners
  {
    const char *p = strstr(text, "\"spawners\"");
    if (p) {
      p = strchr(p, '[');
      if (p) { p++;
        while (*p && *p != ']' && ed->spawnerCount < EDITOR_MAX_SPAWNERS) {
          while (*p && *p != '{' && *p != ']') p++;
          if (*p != '{') break;
          const char *obj = p;
          while (*p && *p != '}') p++;
          if (!*p) break; p++;
          int len = (int)(p - obj);
          if (len >= 256) continue;
          char buf[256]; memcpy(buf, obj, len); buf[len] = '\0';
          float x=0,y=0,z=0,type=0;
          JsonReadFloat(buf,"x",&x); JsonReadFloat(buf,"y",&y); JsonReadFloat(buf,"z",&z);
          JsonReadFloat(buf,"type",&type);
          ed->placedSpawners[ed->spawnerCount++] = (EditorPlacedSpawner){
              .position  = {x,y,z},
              .enemyType = (int)type,
          };
        }
      }
    }
  }

  // Parse props
  {
    const char *p = strstr(text, "\"props\"");
    if (p) {
      p = strchr(p, '[');
      if (p) { p++;
        while (*p && *p != ']' && ed->propCount < EDITOR_MAX_PROPS) {
          while (*p && *p != '{' && *p != ']') p++;
          if (*p != '{') break;
          const char *obj = p;
          while (*p && *p != '}') p++;
          if (!*p) break; p++;
          int len = (int)(p - obj);
          if (len >= 512) continue;
          char buf[512]; memcpy(buf, obj, len); buf[len] = '\0';
          float x=0,y=0,z=0,yaw=0,sx=3,sy=3,sz=3;
          char modelPath[256] = "assets/models/obstacle.glb";
          JsonReadFloat(buf,"x",&x); JsonReadFloat(buf,"y",&y); JsonReadFloat(buf,"z",&z);
          JsonReadFloat(buf,"yaw",&yaw);
          JsonReadFloat(buf,"sx",&sx); JsonReadFloat(buf,"sy",&sy); JsonReadFloat(buf,"sz",&sz);
          JsonReadString(buf,"model",modelPath,sizeof(modelPath));
          EditorPlacedProp *pp = &ed->placedProps[ed->propCount++];
          pp->position = (Vector3){x,y,z};
          pp->yaw      = yaw;
          pp->scale    = (Vector3){sx,sy,sz};
          strncpy(pp->modelPath, modelPath, 255);
        }
      }
    }
  }

  // Parse player spawn point
  {
    ed->edHasSpawnPoint = false;
    const char *sp = strstr(text, "\"spawn\"");
    if (sp) {
      sp = strchr(sp, '{');
      if (sp) {
        const char *ep = strchr(sp, '}');
        if (ep) {
          int len = (int)(ep - sp) + 1;
          if (len < 128) {
            char buf[128]; memcpy(buf, sp, len); buf[len] = '\0';
            float sx = 0, sy = 1.8f, sz = 0;
            JsonReadFloat(buf, "x", &sx);
            JsonReadFloat(buf, "y", &sy);
            JsonReadFloat(buf, "z", &sz);
            ed->edSpawnPoint    = (Vector3){sx, sy, sz};
            ed->edHasSpawnPoint = true;
          }
        }
      }
    }
  }

  // Parse info boxes
  {
    ed->infoBoxCount = 0;
    const char *p = strstr(text, "\"infoboxes\"");
    if (p) {
      p = strchr(p, '[');
      if (p) { p++;
        while (*p && *p != ']' && ed->infoBoxCount < EDITOR_MAX_INFOBOXES) {
          while (*p && *p != '{' && *p != ']') p++;
          if (*p != '{') break;
          const char *obj = p;
          while (*p && *p != '}') p++;
          if (!*p) break; p++;
          int len = (int)(p - obj);
          if (len >= 512) continue;
          char buf[512]; memcpy(buf, obj, len); buf[len] = '\0';
          EditorPlacedInfoBox *ib = &ed->placedInfoBoxes[ed->infoBoxCount++];
          ib->halfExtent = 2.5f; ib->duration = 5.0f; ib->triggerCount = 1;
          ib->fontSize = 0;
          JsonReadFloat(buf, "x",   &ib->position.x);
          JsonReadFloat(buf, "y",   &ib->position.y);
          JsonReadFloat(buf, "z",   &ib->position.z);
          JsonReadFloat(buf, "ext", &ib->halfExtent);
          JsonReadFloat(buf, "dur", &ib->duration);
          JsonReadString(buf, "msg", ib->message, sizeof(ib->message));
          { float tf = 1.0f; if (JsonReadFloat(buf, "trig", &tf)) ib->triggerCount = (int)tf; }
          JsonReadFloat(buf, "mh", &ib->markerHeight);
          { float fsf = 0; if (JsonReadFloat(buf, "fs", &fsf)) ib->fontSize = (int)fsf; }
          // Convert | back to \n
          for (int j = 0; ib->message[j]; j++)
            if (ib->message[j] == '|') ib->message[j] = '\n';
        }
      }
    }
  }

  // Parse wall segments
  {
    ed->wallSegCount = 0;
    const char *p = strstr(text, "\"wallsegs\"");
    if (p) {
      p = strchr(p, '[');
      if (p) { p++;
        while (*p && *p != ']' && ed->wallSegCount < EDITOR_MAX_WALLSEGS) {
          while (*p && *p != '{' && *p != ']') p++;
          if (*p != '{') break;
          const char *obj = p;
          while (*p && *p != '}') p++;
          if (!*p) break; p++;
          int len = (int)(p - obj);
          if (len >= 256) continue;
          char buf[256]; memcpy(buf, obj, len); buf[len] = '\0';
          EditorPlacedWallSeg *ws = &ed->placedWallSegs[ed->wallSegCount++];
          ws->ax = 0; ws->az = 0; ws->bx = 0; ws->bz = 0;
          ws->yBottom = 0; ws->yTop = 3; ws->radius = 0.3f;
          ws->blockPlayer = true; ws->blockProjectiles = true;
          JsonReadFloat(buf, "ax", &ws->ax);
          JsonReadFloat(buf, "az", &ws->az);
          JsonReadFloat(buf, "bx", &ws->bx);
          JsonReadFloat(buf, "bz", &ws->bz);
          JsonReadFloat(buf, "yb", &ws->yBottom);
          JsonReadFloat(buf, "yt", &ws->yTop);
          JsonReadFloat(buf, "r",  &ws->radius);
          { float v = 1; if (JsonReadFloat(buf, "bplay", &v)) ws->blockPlayer      = (v != 0); }
          { float v = 1; if (JsonReadFloat(buf, "bproj", &v)) ws->blockProjectiles = (v != 0); }
        }
      }
    }
  }

  // Parse targets
  {
    ed->targetStaticCount = 0;
    ed->targetPatrolCount = 0;
    const char *p = strstr(text, "\"targets\"");
    if (p) {
      p = strchr(p, '[');
      if (p) { p++;
        while (*p && *p != ']') {
          while (*p && *p != '{' && *p != ']') p++;
          if (*p != '{') break;
          const char *obj = p;
          while (*p && *p != '}') p++;
          if (!*p) break; p++;
          int len = (int)(p - obj);
          if (len >= 512) continue;
          char buf[512]; memcpy(buf, obj, len); buf[len] = '\0';
          float x=0, y=0, z=0, yaw=0, hp=100, shield=0, patrol=0, x2=0, y2=0, z2=0, speed=5, hdrop=0, cdrop=0;
          JsonReadFloat(buf, "x",      &x);
          JsonReadFloat(buf, "y",      &y);
          JsonReadFloat(buf, "z",      &z);
          JsonReadFloat(buf, "yaw",    &yaw);
          JsonReadFloat(buf, "hp",     &hp);
          JsonReadFloat(buf, "shield", &shield);
          JsonReadFloat(buf, "patrol", &patrol);
          JsonReadFloat(buf, "x2",     &x2);
          JsonReadFloat(buf, "y2",     &y2);
          JsonReadFloat(buf, "z2",     &z2);
          JsonReadFloat(buf, "speed",  &speed);
          JsonReadFloat(buf, "hdrop",  &hdrop);
          JsonReadFloat(buf, "cdrop",  &cdrop);
          if ((int)patrol && ed->targetPatrolCount < EDITOR_MAX_TARGETS) {
            entity_t e = SpawnTargetPatrol(world, gw, (Vector3){x,y,z}, (Vector3){x2,y2,z2},
                                           hp, shield, speed, yaw, (int)hdrop, (int)cdrop);
            ed->placedTargetsPatrol[ed->targetPatrolCount++] = (EditorPlacedTargetPatrol){
              .entity = e, .pointA = {x, y, z}, .pointB = {x2, y2, z2},
              .yaw = yaw, .health = hp, .shield = shield, .speed = speed,
              .healthDropCount = hdrop, .coolantDropCount = cdrop,
            };
          } else if (!(int)patrol && ed->targetStaticCount < EDITOR_MAX_TARGETS) {
            entity_t e = SpawnTargetStatic(world, gw, (Vector3){x,y,z}, hp, shield, yaw, (int)hdrop, (int)cdrop);
            ed->placedTargetsStatic[ed->targetStaticCount++] = (EditorPlacedTargetStatic){
              .entity = e, .position = {x, y, z}, .yaw = yaw, .health = hp, .shield = shield,
              .healthDropCount = hdrop, .coolantDropCount = cdrop,
            };
          }
        }
      }
    }
  }

  UnloadFileText(text);
}

void EditorUpdate(EditorState *ed, GameWorld *gw, world_t *world) {
  float dt = GetFrameTime();

  // --- Naming prompt (blocks everything else) ---
  if (ed->isNaming) {
    int ch;
    while ((ch = GetCharPressed()) != 0) {
      bool valid = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                   (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == ' ';
      if (valid && ed->nameLen < 127) {
        ed->nameBuffer[ed->nameLen++] = (char)ch;
        ed->nameBuffer[ed->nameLen]   = '\0';
      }
    }
    if (IsKeyPressed(KEY_BACKSPACE) && ed->nameLen > 0)
      ed->nameBuffer[--ed->nameLen] = '\0';
    if (IsKeyPressed(KEY_ENTER) && ed->nameLen > 0) {
      char path[256];
      snprintf(path, sizeof(path), "assets/levels/%s.json", ed->nameBuffer);
      EditorSave(ed, gw, path);
      ed->isNaming = false;
    }
    if (IsKeyPressed(KEY_ESCAPE))
      ed->isNaming = false;
    return;
  }

  // --- Info box text dialog (blocks all other input) ---
  if (ed->infoBoxEditOpen) {
    // Scroll adjusts trigger size even during dialog
    float sc = GetMouseWheelMove();
    if (sc != 0.0f) {
      ed->infoBoxHalfExtent += sc * 0.25f;
      if (ed->infoBoxHalfExtent < 0.5f)  ed->infoBoxHalfExtent = 0.5f;
      if (ed->infoBoxHalfExtent > 20.0f) ed->infoBoxHalfExtent = 20.0f;
    }
    int ch;
    while ((ch = GetCharPressed()) != 0) {
      if (ch == '"' || ch == '{' || ch == '}' || ch == '\\' || ch == '|') continue;
      if (ed->infoBoxTextLen < 254) {
        ed->infoBoxTextBuf[ed->infoBoxTextLen++] = (char)ch;
        ed->infoBoxTextBuf[ed->infoBoxTextLen]   = '\0';
      }
    }
    // Enter without Ctrl inserts a newline; Ctrl+Enter confirms
    bool ibConfirm = false;
    if (IsKeyPressed(KEY_ENTER)) {
      if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
        ibConfirm = true;
      } else if (ed->infoBoxTextLen < 254) {
        ed->infoBoxTextBuf[ed->infoBoxTextLen++] = '\n';
        ed->infoBoxTextBuf[ed->infoBoxTextLen]   = '\0';
      }
    }
    if (IsKeyPressed(KEY_BACKSPACE) && ed->infoBoxTextLen > 0)
      ed->infoBoxTextBuf[--ed->infoBoxTextLen] = '\0';
    {
      int sw2 = GetScreenWidth(), sh2 = GetScreenHeight();
      int pw2 = 520, px2 = sw2 / 2 - pw2 / 2, py2 = sh2 / 2 - 190;
      Vector2 mouse       = GetMousePosition();
      Rectangle durMinus  = {(float)(px2 + 170), (float)(py2 + 161), 44, 24};
      Rectangle durPlus   = {(float)(px2 + 224), (float)(py2 + 161), 44, 24};
      Rectangle trigMinus = {(float)(px2 + 170), (float)(py2 + 223), 30, 24};
      Rectangle trigPlus  = {(float)(px2 + 210), (float)(py2 + 223), 30, 24};
      Rectangle mhMinus   = {(float)(px2 + 170), (float)(py2 + 273), 44, 24};
      Rectangle mhPlus    = {(float)(px2 + 224), (float)(py2 + 273), 44, 24};
      Rectangle fsMinus   = {(float)(px2 + 170), (float)(py2 + 313), 44, 24};
      Rectangle fsPlus    = {(float)(px2 + 224), (float)(py2 + 313), 44, 24};
      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (CheckCollisionPointRec(mouse, durMinus)) {
          ed->infoBoxDuration -= 5.0f;
          if (ed->infoBoxDuration < 1.0f) ed->infoBoxDuration = 1.0f;
        }
        if (CheckCollisionPointRec(mouse, durPlus)) {
          ed->infoBoxDuration += 5.0f;
          if (ed->infoBoxDuration > 120.0f) ed->infoBoxDuration = 120.0f;
        }
        if (CheckCollisionPointRec(mouse, trigMinus)) {
          ed->infoBoxMaxTriggers--;
          if (ed->infoBoxMaxTriggers < 0) ed->infoBoxMaxTriggers = 0;
        }
        if (CheckCollisionPointRec(mouse, trigPlus)) {
          ed->infoBoxMaxTriggers++;
          if (ed->infoBoxMaxTriggers > 99) ed->infoBoxMaxTriggers = 99;
        }
        if (CheckCollisionPointRec(mouse, mhMinus)) {
          ed->infoBoxMarkerHeight -= 0.5f;
          if (ed->infoBoxMarkerHeight < -10.0f) ed->infoBoxMarkerHeight = -10.0f;
        }
        if (CheckCollisionPointRec(mouse, mhPlus)) {
          ed->infoBoxMarkerHeight += 0.5f;
          if (ed->infoBoxMarkerHeight > 10.0f) ed->infoBoxMarkerHeight = 10.0f;
        }
        if (CheckCollisionPointRec(mouse, fsMinus)) {
          ed->infoBoxFontSize -= 2;
          if (ed->infoBoxFontSize < 0) ed->infoBoxFontSize = 0;
        }
        if (CheckCollisionPointRec(mouse, fsPlus)) {
          ed->infoBoxFontSize += 2;
          if (ed->infoBoxFontSize > 60) ed->infoBoxFontSize = 60;
        }
      }
    }
    if (ibConfirm && ed->infoBoxTextLen > 0) {
      if (ed->infoBoxEditExisting) {
        EditorPlacedInfoBox *ib = &ed->placedInfoBoxes[ed->selectedIndex];
        ib->halfExtent   = ed->infoBoxHalfExtent;
        ib->duration     = ed->infoBoxDuration;
        ib->triggerCount = ed->infoBoxMaxTriggers;
        ib->markerHeight = ed->infoBoxMarkerHeight;
        ib->fontSize     = ed->infoBoxFontSize;
        strncpy(ib->message, ed->infoBoxTextBuf, 255);
        ib->message[255] = '\0';
        ed->infoBoxEditExisting = false;
      } else {
        int histCap = EDITOR_MAX_BOXES + EDITOR_MAX_SPAWNERS + EDITOR_MAX_PROPS +
                      EDITOR_MAX_INFOBOXES + EDITOR_MAX_WALLSEGS;
        if (ed->infoBoxCount < EDITOR_MAX_INFOBOXES && ed->historyTop < histCap) {
          EditorPlacedInfoBox *ib = &ed->placedInfoBoxes[ed->infoBoxCount++];
          ib->position     = ed->infoBoxPendingPos;
          ib->halfExtent   = ed->infoBoxHalfExtent;
          ib->duration     = ed->infoBoxDuration;
          ib->triggerCount = ed->infoBoxMaxTriggers;
          ib->markerHeight = ed->infoBoxMarkerHeight;
          ib->fontSize     = ed->infoBoxFontSize;
          strncpy(ib->message, ed->infoBoxTextBuf, 255);
          ib->message[255] = '\0';
          ed->history[ed->historyTop++] = 4;
        }
      }
      ed->infoBoxEditOpen = false;
      DisableCursor();
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
      ed->infoBoxEditExisting = false;
      ed->infoBoxEditOpen = false;
      DisableCursor();
    }
    return;
  }

  // --- Wall segment param dialog (blocks all other input) ---
  if (ed->wallSegDialogOpen) {
    int sw2 = GetScreenWidth(), sh2 = GetScreenHeight();
    int pw2 = 380, px2 = sw2 / 2 - pw2 / 2, py2 = sh2 / 2 - 130;
    Vector2 mouse = GetMousePosition();
    Rectangle hMinus = {(float)(px2 + 170), (float)(py2 + 55), 44, 24};
    Rectangle hPlus  = {(float)(px2 + 224), (float)(py2 + 55), 44, 24};
    Rectangle rMinus   = {(float)(px2 + 170), (float)(py2 + 105), 44, 24};
    Rectangle rPlus    = {(float)(px2 + 224), (float)(py2 + 105), 44, 24};
    Rectangle bPlayBtn = {(float)(px2 + 170), (float)(py2 + 155), 80, 24};
    Rectangle bProjBtn = {(float)(px2 + 170), (float)(py2 + 195), 80, 24};
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      if (CheckCollisionPointRec(mouse, hMinus)) {
        ed->wallSegHeight -= 0.5f;
        if (ed->wallSegHeight < 0.5f) ed->wallSegHeight = 0.5f;
      }
      if (CheckCollisionPointRec(mouse, hPlus)) {
        ed->wallSegHeight += 0.5f;
        if (ed->wallSegHeight > 50.0f) ed->wallSegHeight = 50.0f;
      }
      if (CheckCollisionPointRec(mouse, rMinus)) {
        ed->wallSegRadius -= 0.1f;
        if (ed->wallSegRadius < 0.1f) ed->wallSegRadius = 0.1f;
      }
      if (CheckCollisionPointRec(mouse, rPlus)) {
        ed->wallSegRadius += 0.1f;
        if (ed->wallSegRadius > 5.0f) ed->wallSegRadius = 5.0f;
      }
      if (CheckCollisionPointRec(mouse, bPlayBtn))
        ed->wallSegBlockPlayer = !ed->wallSegBlockPlayer;
      if (CheckCollisionPointRec(mouse, bProjBtn))
        ed->wallSegBlockProjectiles = !ed->wallSegBlockProjectiles;
    }
    if (IsKeyPressed(KEY_ENTER)) {
      if (ed->wallSegEditExisting && ed->selectedIndex >= 0 &&
          ed->selectedIndex < ed->wallSegCount) {
        EditorPlacedWallSeg *ws = &ed->placedWallSegs[ed->selectedIndex];
        ws->yTop             = ws->yBottom + ed->wallSegHeight;
        ws->radius           = ed->wallSegRadius;
        ws->blockPlayer      = ed->wallSegBlockPlayer;
        ws->blockProjectiles = ed->wallSegBlockProjectiles;
      } else {
        int histCap = EDITOR_MAX_BOXES + EDITOR_MAX_SPAWNERS + EDITOR_MAX_PROPS +
                      EDITOR_MAX_INFOBOXES + EDITOR_MAX_WALLSEGS;
        if (ed->wallSegCount < EDITOR_MAX_WALLSEGS && ed->historyTop < histCap) {
          float terrYA = HeightMap_GetHeightCatmullRom(&gw->terrainHeightMap,
                                                       ed->wallSegPendingA.x,
                                                       ed->wallSegPendingA.z);
          float terrYB = HeightMap_GetHeightCatmullRom(&gw->terrainHeightMap,
                                                       ed->wallSegPendingB.x,
                                                       ed->wallSegPendingB.z);
          float yBottom = fminf(terrYA, terrYB);
          EditorPlacedWallSeg *ws = &ed->placedWallSegs[ed->wallSegCount++];
          ws->ax               = ed->wallSegPendingA.x;
          ws->az               = ed->wallSegPendingA.z;
          ws->bx               = ed->wallSegPendingB.x;
          ws->bz               = ed->wallSegPendingB.z;
          ws->yBottom          = yBottom;
          ws->yTop             = yBottom + ed->wallSegHeight;
          ws->radius           = ed->wallSegRadius;
          ws->blockPlayer      = ed->wallSegBlockPlayer;
          ws->blockProjectiles = ed->wallSegBlockProjectiles;
          ed->history[ed->historyTop++] = 5;
        }
      }
      ed->wallSegEditExisting = false;
      ed->wallSegDialogOpen   = false;
      ed->wallSegStep         = 0;
      DisableCursor();
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
      ed->wallSegEditExisting = false;
      ed->wallSegDialogOpen   = false;
      ed->wallSegStep         = 0;
      DisableCursor();
    }
    return;
  }

  // --- Box array dialog (blocks all other input) ---
  if (ed->arrayDialogOpen) {
    int sw2 = GetScreenWidth(), sh2 = GetScreenHeight();
    int pw2 = 420, ph2 = 290;
    int px2 = sw2 / 2 - pw2 / 2, py2 = sh2 / 2 - ph2 / 2;
    Vector2 mouse = GetMousePosition();
    Rectangle cntMinus  = {(float)(px2+165), (float)(py2+50), 44, 24};
    Rectangle cntPlus   = {(float)(px2+215), (float)(py2+50), 44, 24};
    Rectangle spcMinus  = {(float)(px2+165), (float)(py2+90), 44, 24};
    Rectangle spcPlus   = {(float)(px2+215), (float)(py2+90), 44, 24};
    static const char *dirLabels[6] = {"+X", "-X", "+Z", "-Z", "+Y", "-Y"};
    Rectangle dirBtns[6];
    dirBtns[0] = (Rectangle){(float)(px2+16),  (float)(py2+155), 80, 28};
    dirBtns[1] = (Rectangle){(float)(px2+104), (float)(py2+155), 80, 28};
    dirBtns[2] = (Rectangle){(float)(px2+192), (float)(py2+155), 80, 28};
    dirBtns[3] = (Rectangle){(float)(px2+280), (float)(py2+155), 80, 28};
    dirBtns[4] = (Rectangle){(float)(px2+16),  (float)(py2+193), 80, 28};
    dirBtns[5] = (Rectangle){(float)(px2+104), (float)(py2+193), 80, 28};
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      if (CheckCollisionPointRec(mouse, cntMinus)) {
        ed->arrayCount--; if (ed->arrayCount < 1) ed->arrayCount = 1;
      }
      if (CheckCollisionPointRec(mouse, cntPlus)) {
        ed->arrayCount++; if (ed->arrayCount > 100) ed->arrayCount = 100;
      }
      if (CheckCollisionPointRec(mouse, spcMinus)) {
        ed->arraySpacing -= 0.5f; if (ed->arraySpacing < 0.5f) ed->arraySpacing = 0.5f;
      }
      if (CheckCollisionPointRec(mouse, spcPlus)) {
        ed->arraySpacing += 0.5f; if (ed->arraySpacing > 100.0f) ed->arraySpacing = 100.0f;
      }
      for (int d = 0; d < 6; d++) {
        if (CheckCollisionPointRec(mouse, dirBtns[d]))
          ed->arrayDir = d;
      }
    }
    // Scroll adjusts count
    {
      float sc = GetMouseWheelMove();
      if (sc != 0.0f) {
        ed->arrayCount += (int)sc;
        if (ed->arrayCount < 1)   ed->arrayCount = 1;
        if (ed->arrayCount > 100) ed->arrayCount = 100;
      }
    }
    if (IsKeyPressed(KEY_ENTER)) {
      static const Vector3 dirs[6] = {{1,0,0},{-1,0,0},{0,0,1},{0,0,-1},{0,1,0},{0,-1,0}};
      Vector3 d = dirs[ed->arrayDir];
      int histCap = EDITOR_MAX_BOXES + EDITOR_MAX_SPAWNERS + EDITOR_MAX_PROPS +
                    EDITOR_MAX_INFOBOXES + EDITOR_MAX_WALLSEGS;
      Vector3 scale = {ed->boxScale, ed->boxScale, ed->boxScale};
      for (int i = 0; i < ed->arrayCount && ed->placedCount < EDITOR_MAX_BOXES; i++) {
        Vector3 p = {
          ed->arrayOrigin.x + d.x * ed->arraySpacing * i,
          ed->arrayOrigin.y + d.y * ed->arraySpacing * i,
          ed->arrayOrigin.z + d.z * ed->arraySpacing * i,
        };
        entity_t e = SpawnBoxModel(world, gw, p, scale);
        ed->placed[ed->placedCount++] = (EditorPlacedBox){.entity=e, .position=p, .scale=scale};
        if (ed->historyTop < histCap) ed->history[ed->historyTop++] = 0;
      }
      ed->arrayDialogOpen = false;
      DisableCursor();
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
      ed->arrayDialogOpen = false;
      DisableCursor();
    }
    return;
  }

  // --- Target placement dialog ---
  if (ed->targetDialogOpen) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int ph = ed->targetIsPatrol ? 285 : 250;
    int pw = 380, px = sw / 2 - pw / 2, py = sh / 2 - ph / 2;
    Vector2 mouse = GetMousePosition();

    // TAB cycles active field
    if (IsKeyPressed(KEY_TAB)) {
      int maxField = ed->targetIsPatrol ? 4 : 3;
      ed->targetDlgField = (ed->targetDlgField + 1) % (maxField + 1);
    }

    // Speed row offset: base rows at 52/88, then speed at 124 (patrol), then drops
    int speedY = ed->targetIsPatrol ? 124 : -1;
    int hdropY = ed->targetIsPatrol ? 160 : 124;
    int cdropY = ed->targetIsPatrol ? 196 : 160;

    Rectangle hpMinus  = {(float)(px+180), (float)(py+52), 40, 22};
    Rectangle hpPlus   = {(float)(px+226), (float)(py+52), 40, 22};
    Rectangle shMinus  = {(float)(px+180), (float)(py+88), 40, 22};
    Rectangle shPlus   = {(float)(px+226), (float)(py+88), 40, 22};
    Rectangle spMinus  = {(float)(px+180), (float)(py+speedY), 40, 22};
    Rectangle spPlus   = {(float)(px+226), (float)(py+speedY), 40, 22};
    Rectangle hdMinus  = {(float)(px+180), (float)(py+hdropY), 40, 22};
    Rectangle hdPlus   = {(float)(px+226), (float)(py+hdropY), 40, 22};
    Rectangle cdMinus  = {(float)(px+180), (float)(py+cdropY), 40, 22};
    Rectangle cdPlus   = {(float)(px+226), (float)(py+cdropY), 40, 22};

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      if (CheckCollisionPointRec(mouse, hpMinus)) { ed->targetDlgHealth -= 10.0f; if (ed->targetDlgHealth < 10.0f)  ed->targetDlgHealth = 10.0f; }
      if (CheckCollisionPointRec(mouse, hpPlus))  { ed->targetDlgHealth += 10.0f; if (ed->targetDlgHealth > 9990.0f) ed->targetDlgHealth = 9990.0f; }
      if (CheckCollisionPointRec(mouse, shMinus)) { ed->targetDlgShield -= 10.0f; if (ed->targetDlgShield < 0.0f)   ed->targetDlgShield = 0.0f; }
      if (CheckCollisionPointRec(mouse, shPlus))  { ed->targetDlgShield += 10.0f; if (ed->targetDlgShield > 9990.0f) ed->targetDlgShield = 9990.0f; }
      if (ed->targetIsPatrol && speedY >= 0) {
        if (CheckCollisionPointRec(mouse, spMinus)) { ed->targetDlgSpeed -= 0.5f; if (ed->targetDlgSpeed < 0.5f) ed->targetDlgSpeed = 0.5f; }
        if (CheckCollisionPointRec(mouse, spPlus))  { ed->targetDlgSpeed += 0.5f; if (ed->targetDlgSpeed > 50.0f) ed->targetDlgSpeed = 50.0f; }
      }
      if (CheckCollisionPointRec(mouse, hdMinus)) { if (ed->targetDlgHealthCount  > 0)  ed->targetDlgHealthCount--; }
      if (CheckCollisionPointRec(mouse, hdPlus))  { if (ed->targetDlgHealthCount  < 10) ed->targetDlgHealthCount++; }
      if (CheckCollisionPointRec(mouse, cdMinus)) { if (ed->targetDlgCoolantCount > 0)  ed->targetDlgCoolantCount--; }
      if (CheckCollisionPointRec(mouse, cdPlus))  { if (ed->targetDlgCoolantCount < 10) ed->targetDlgCoolantCount++; }
    }

    if (IsKeyPressed(KEY_ENTER)) {
      if (ed->targetEditExisting) {
        // Update an already-placed target in-place
        if (ed->targetIsPatrol && ed->targetEditIndex < ed->targetPatrolCount) {
          EditorPlacedTargetPatrol *tp = &ed->placedTargetsPatrol[ed->targetEditIndex];
          tp->yaw = ed->targetDlgYaw; tp->health = ed->targetDlgHealth;
          tp->shield = ed->targetDlgShield; tp->speed = ed->targetDlgSpeed;
          tp->healthDropCount = ed->targetDlgHealthCount;
          tp->coolantDropCount = ed->targetDlgCoolantCount;
          if (tp->entity.id) {
            Health *hp = ECS_GET(world, tp->entity, Health, COMP_HEALTH);
            if (hp) { hp->max = tp->health; hp->current = tp->health; }
            Shield *sh = ECS_GET(world, tp->entity, Shield, COMP_SHIELD);
            if (sh) { sh->max = tp->shield; sh->current = tp->shield; }
            TargetDummy *td = ECS_GET(world, tp->entity, TargetDummy, COMP_TARGET_DUMMY);
            if (td) { td->maxHealth = tp->health; td->maxShield = tp->shield;
                      td->healthDropCount = tp->healthDropCount;
                      td->coolantDropCount = tp->coolantDropCount; }
            TargetPatrol *tpp = ECS_GET(world, tp->entity, TargetPatrol, COMP_TARGET_PATROL);
            if (tpp) tpp->speed = tp->speed;
            SyncTargetPosition(world, tp->entity, tp->pointA, tp->yaw);
          }
        } else if (!ed->targetIsPatrol && ed->targetEditIndex < ed->targetStaticCount) {
          EditorPlacedTargetStatic *ts = &ed->placedTargetsStatic[ed->targetEditIndex];
          ts->yaw = ed->targetDlgYaw; ts->health = ed->targetDlgHealth;
          ts->shield = ed->targetDlgShield;
          ts->healthDropCount = ed->targetDlgHealthCount;
          ts->coolantDropCount = ed->targetDlgCoolantCount;
          if (ts->entity.id) {
            Health *hp = ECS_GET(world, ts->entity, Health, COMP_HEALTH);
            if (hp) { hp->max = ts->health; hp->current = ts->health; }
            Shield *sh = ECS_GET(world, ts->entity, Shield, COMP_SHIELD);
            if (sh) { sh->max = ts->shield; sh->current = ts->shield; }
            TargetDummy *td = ECS_GET(world, ts->entity, TargetDummy, COMP_TARGET_DUMMY);
            if (td) { td->maxHealth = ts->health; td->maxShield = ts->shield;
                      td->healthDropCount = ts->healthDropCount;
                      td->coolantDropCount = ts->coolantDropCount; }
            SyncTargetPosition(world, ts->entity, ts->position, ts->yaw);
          }
        }
        ed->targetEditExisting = false;
      } else if (ed->targetIsPatrol && ed->targetPatrolCount < EDITOR_MAX_TARGETS) {
        entity_t e = SpawnTargetPatrol(world, gw, ed->targetPendingA, ed->targetPendingB,
                          ed->targetDlgHealth, ed->targetDlgShield, ed->targetDlgSpeed,
                          ed->targetDlgYaw, ed->targetDlgHealthCount, ed->targetDlgCoolantCount);
        ed->placedTargetsPatrol[ed->targetPatrolCount++] = (EditorPlacedTargetPatrol){
          .entity          = e,
          .pointA          = ed->targetPendingA,
          .pointB          = ed->targetPendingB,
          .yaw             = ed->targetDlgYaw,
          .health          = ed->targetDlgHealth,
          .shield          = ed->targetDlgShield,
          .speed           = ed->targetDlgSpeed,
          .healthDropCount= ed->targetDlgHealthCount,
          .coolantDropCount= ed->targetDlgCoolantCount,
        };
      } else if (!ed->targetIsPatrol && ed->targetStaticCount < EDITOR_MAX_TARGETS) {
        entity_t e = SpawnTargetStatic(world, gw, ed->targetPendingA, ed->targetDlgHealth,
                          ed->targetDlgShield, ed->targetDlgYaw,
                          ed->targetDlgHealthCount, ed->targetDlgCoolantCount);
        ed->placedTargetsStatic[ed->targetStaticCount++] = (EditorPlacedTargetStatic){
          .entity          = e,
          .position        = ed->targetPendingA,
          .yaw             = ed->targetDlgYaw,
          .health          = ed->targetDlgHealth,
          .shield          = ed->targetDlgShield,
          .healthDropCount= ed->targetDlgHealthCount,
          .coolantDropCount= ed->targetDlgCoolantCount,
        };
      }
      ed->targetDialogOpen = false;
      ed->targetPatrolStep = 0;
      DisableCursor();
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
      ed->targetEditExisting = false;
      ed->targetDialogOpen = false;
      ed->targetPatrolStep = 0;
      DisableCursor();
    }
    return;
  }

  // --- ESC toggles pause menu ---
  if (IsKeyPressed(KEY_ESCAPE)) {
    if (ed->wallSegStep > 0) {
      ed->wallSegStep = 0;
    } else if (ed->navPaletteOpen) {
      ed->navPaletteOpen = false;
    } else if (ed->waveEditorOpen) {
      ed->waveEditorOpen = false;
      DisableCursor();
    } else if (ed->objectPanelOpen) {
      ed->objectPanelOpen = false;
    } else if (ed->modelPickerOpen) {
      ed->modelPickerOpen = false;
      DisableCursor();
    } else if (ed->terrainPickerOpen) {
      ed->terrainPickerOpen = false;
      DisableCursor();
    } else if (ed->isSelectingLevel) {
      ed->isSelectingLevel = false;
    } else {
      ed->isPaused = !ed->isPaused;
      if (ed->isPaused) EnableCursor();
      else              DisableCursor();
    }
  }

  // --- Pause menu input (blocks everything else) ---
  if (ed->isPaused) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    if (ed->isSelectingLevel) {
      if (IsKeyPressed(KEY_UP) && ed->edLevelCount > 0)
        ed->edLevelSelected = (ed->edLevelSelected - 1 + ed->edLevelCount) % ed->edLevelCount;
      if (IsKeyPressed(KEY_DOWN) && ed->edLevelCount > 0)
        ed->edLevelSelected = (ed->edLevelSelected + 1) % ed->edLevelCount;
      float scroll = GetMouseWheelMove();
      if (scroll > 0.0f && ed->edLevelCount > 0)
        ed->edLevelSelected = (ed->edLevelSelected - 1 + ed->edLevelCount) % ed->edLevelCount;
      if (scroll < 0.0f && ed->edLevelCount > 0)
        ed->edLevelSelected = (ed->edLevelSelected + 1) % ed->edLevelCount;
      if (IsKeyPressed(KEY_ENTER) && ed->edLevelCount > 0) {
        EditorLoad(ed, gw, world, ed->edLevelPaths[ed->edLevelSelected]);
        ed->isSelectingLevel = false;
        ed->isPaused = false;
        DisableCursor();
      }
      int itemW = 400, itemH = 50;
      int lsx = sw / 2 - itemW / 2, lsy = 150;
      Vector2 mp = GetMousePosition();
      for (int i = 0; i < ed->edLevelCount; i++) {
        Rectangle r = {(float)lsx, (float)(lsy + i * (itemH + 8)),
                       (float)itemW, (float)itemH};
        if (CheckCollisionPointRec(mp, r)) {
          ed->edLevelSelected = i;
          if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            EditorLoad(ed, gw, world, ed->edLevelPaths[i]);
            ed->isSelectingLevel = false;
            ed->isPaused = false;
            DisableCursor();
          }
        }
      }
    } else {
      int bx = sw / 2 - 120;
      Vector2 mp = GetMousePosition();
      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (CheckCollisionPointRec(mp, (Rectangle){(float)bx, (float)(sh/2 - 90), 240, 50})) {
          ed->isPaused = false;
          DisableCursor();
        }
        if (CheckCollisionPointRec(mp, (Rectangle){(float)bx, (float)(sh/2 - 10), 240, 50})) {
          ScanLevels(ed);
          ed->edLevelSelected = 0;
          ed->isSelectingLevel = true;
        }
        if (CheckCollisionPointRec(mp, (Rectangle){(float)bx, (float)(sh/2 + 70), 240, 50}))
          ed->requestQuit = true;
      }
    }
    return;
  }

  // --- Mode toggle ---
  if (IsKeyPressed(KEY_TAB)) {
    ed->navPaintMode      = false;
    ed->transformMode     = !ed->transformMode;
    ed->selectedIndex     = -1;
    ed->objectPanelOpen   = false;
    ed->modelPickerOpen   = false;
    ed->terrainPickerOpen = false;
  }

  // --- Nav paint toggle ---
  if (IsKeyPressed(KEY_N)) {
    ed->navPaintMode      = !ed->navPaintMode;
    if (ed->navPaintMode) ed->transformMode = false;
    ed->navPaletteOpen    = false;
    ed->objectPanelOpen   = false;
    ed->modelPickerOpen   = false;
    ed->terrainPickerOpen = false;
  }

  // --- Model picker: M (prop model), T (terrain model) ---
  // Available in place mode (prop type) or transform mode (prop selected)
  bool canPickPropModel = (!ed->navPaintMode) &&
      ((!ed->transformMode && ed->placeType == 3) ||
       (ed->transformMode && ed->selectedIndex >= 0 && ed->selectedType == 1));
  if (IsKeyPressed(KEY_M) && canPickPropModel) {
    ed->modelPickerOpen = !ed->modelPickerOpen;
    if (ed->modelPickerOpen) { ScanModelFiles(ed); ed->modelPickerScroll = 0; EnableCursor(); }
    else                     DisableCursor();
    ed->terrainPickerOpen = false;
  }
  if (IsKeyPressed(KEY_T) && !ed->navPaintMode) {
    ed->terrainPickerOpen = !ed->terrainPickerOpen;
    if (ed->terrainPickerOpen) { ScanTerrainFiles(ed); ed->modelPickerScroll = 0; EnableCursor(); }
    else                       DisableCursor();
    ed->modelPickerOpen = false;
  }
  if (IsKeyPressed(KEY_F) && !ed->navPaintMode) {
    ed->missionType = (ed->missionType == MISSION_WAVES) ? MISSION_EXPLORATION : MISSION_WAVES;
  }
  if (IsKeyPressed(KEY_H) && !ed->navPaintMode) {
    ed->healthBarFade = !ed->healthBarFade;
  }
  if (IsKeyPressed(KEY_G) && !ed->navPaintMode && ed->missionType == MISSION_WAVES) {
    ed->waveEditorOpen = !ed->waveEditorOpen;
    if (ed->waveEditorOpen) EnableCursor();
    else                    DisableCursor();
  }

  // Handle picker input (model or terrain) — scroll and click
  if (ed->modelPickerOpen || ed->terrainPickerOpen) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int vis   = (ed->edModelCount < PICKER_VIS) ? ed->edModelCount : PICKER_VIS;
    int maxSc = ed->edModelCount - vis;
    if (maxSc < 0) maxSc = 0;

    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
      ed->modelPickerScroll -= (int)wheel;
      if (ed->modelPickerScroll < 0)     ed->modelPickerScroll = 0;
      if (ed->modelPickerScroll > maxSc) ed->modelPickerScroll = maxSc;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      int pw = 340;
      int px = sw/2 - pw/2;
      int py = sh/2 - (PICKER_HDR + vis*(PICKER_IH+PICKER_GAP) + PICKER_FTR)/2;
      Vector2 mp = GetMousePosition();

      for (int j = 0; j < vis; j++) {
        int i  = ed->modelPickerScroll + j;
        if (i >= ed->edModelCount) break;
        Rectangle r = {(float)px,
                       (float)(py + PICKER_HDR + j*(PICKER_IH+PICKER_GAP)),
                       (float)pw, (float)PICKER_IH};
        if (!CheckCollisionPointRec(mp, r)) continue;

        if (ed->modelPickerOpen) {
          strncpy(ed->propPlaceModel, ed->edModelPaths[i], 255);
          if (ed->transformMode && ed->selectedIndex >= 0 && ed->selectedType == 1)
            strncpy(ed->placedProps[ed->selectedIndex].modelPath, ed->edModelPaths[i], 255);
          ed->modelPickerOpen = false;
        } else {
          const char *newPath = ed->edModelPaths[i];
          if (strcmp(newPath, gw->terrainModelPath) != 0) {
            UnloadModel(gw->terrainModel);
            gw->terrainModel = LoadModel(newPath);
            strncpy(gw->terrainModelPath, newPath, sizeof(gw->terrainModelPath)-1);
            HeightMap_Free(&gw->terrainHeightMap);
            gw->terrainHeightMap = HeightMap_FromMesh(gw->terrainModel.meshes[0], MatrixIdentity());
            s_navHeightCacheValid = false;
          }
          ed->terrainPickerOpen = false;
        }
        DisableCursor();
        break;
      }
    }
  }

  // --- Object picker panel: V toggles, cursor enabled while open ---
  if (!ed->transformMode && !ed->navPaintMode && IsKeyPressed(KEY_V)) {
    ed->objectPanelOpen = !ed->objectPanelOpen;
    if (ed->objectPanelOpen) EnableCursor();
    else                     DisableCursor();
  }

  if (ed->objectPanelOpen) {
    int sw = GetScreenWidth();
    int px = sw - 185, py = 50;
    int pw = 170, ih = 38;
    Vector2 mp = GetMousePosition();
    for (int i = 0; i < 9; i++) {
      Rectangle r = {(float)px, (float)(py + i * (ih + 4)), (float)pw, (float)ih};
      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, r)) {
        if (ed->placeType != i) { ed->wallSegStep = 0; ed->targetPatrolStep = 0; }
        ed->placeType       = i;
        ed->objectPanelOpen = false;
        DisableCursor();
      }
    }
  }

  // --- Wave editor input ---
  if (ed->waveEditorOpen) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int pw = 500, rowH = 28, hdr = 54;
    int ph = hdr + ed->edWaveCount * rowH + 38;
    if (ph < 140) ph = 140;
    int px = sw / 2 - pw / 2, py = sh / 2 - ph / 2;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      Vector2 mp = GetMousePosition();

      for (int wi = 0; wi < ed->edWaveCount; wi++) {
        int rowY = py + hdr + wi * rowH + 3;
        for (int ci = 0; ci < 4; ci++) {
          int bx = px + 54 + ci * 94;
          Rectangle btnMinus = {(float)bx,       (float)rowY, 24, 22};
          Rectangle btnPlus  = {(float)(bx + 54),(float)rowY, 24, 22};
          if (CheckCollisionPointRec(mp, btnMinus) && ed->edWaves[wi][ci] > 0)
            ed->edWaves[wi][ci]--;
          if (CheckCollisionPointRec(mp, btnPlus) && ed->edWaves[wi][ci] < 30)
            ed->edWaves[wi][ci]++;
        }
        Rectangle btnDel = {(float)(px + 435), (float)rowY, 50, 22};
        if (CheckCollisionPointRec(mp, btnDel)) {
          for (int j = wi; j < ed->edWaveCount - 1; j++)
            memcpy(ed->edWaves[j], ed->edWaves[j + 1], sizeof(ed->edWaves[0]));
          ed->edWaveCount--;
          break;
        }
      }

      if (ed->edWaveCount < MAX_WAVES) {
        int addY = py + hdr + ed->edWaveCount * rowH + 6;
        Rectangle btnAdd = {(float)(px + pw / 2 - 70), (float)addY, 140, 26};
        if (CheckCollisionPointRec(mp, btnAdd)) {
          int wi = ed->edWaveCount++;
          ed->edWaves[wi][0] = ed->edWaves[wi][1] = ed->edWaves[wi][2] = ed->edWaves[wi][3] = 0;
        }
      }
    }
    return;
  }

  // --- Camera (always active unless panel is open) ---
  float speed = IsKeyDown(KEY_LEFT_SHIFT) ? 80.0f : 30.0f;
  Vector2 mouse = ed->objectPanelOpen ? (Vector2){0,0} : GetMouseDelta();
  ed->yaw   -= mouse.x * 0.002f;
  ed->pitch -= mouse.y * 0.002f;
  if (ed->pitch >  PI / 2 - 0.01f) ed->pitch =  PI / 2 - 0.01f;
  if (ed->pitch < -PI / 2 + 0.01f) ed->pitch = -PI / 2 + 0.01f;

  Vector3 forward = {
      cosf(ed->pitch) * sinf(ed->yaw),
      sinf(ed->pitch),
      cosf(ed->pitch) * cosf(ed->yaw),
  };
  Vector3 right = {cosf(ed->yaw), 0.0f, -sinf(ed->yaw)};

  if (IsKeyDown(KEY_W))
    ed->camera.position = Vector3Add(ed->camera.position, Vector3Scale(forward, speed * dt));
  if (IsKeyDown(KEY_S))
    ed->camera.position = Vector3Subtract(ed->camera.position, Vector3Scale(forward, speed * dt));
  if (IsKeyDown(KEY_A))
    ed->camera.position = Vector3Add(ed->camera.position, Vector3Scale(right, speed * dt));
  if (IsKeyDown(KEY_D))
    ed->camera.position = Vector3Subtract(ed->camera.position, Vector3Scale(right, speed * dt));
  if (IsKeyDown(KEY_Q)) ed->camera.position.y -= speed * dt;
  if (IsKeyDown(KEY_E)) ed->camera.position.y += speed * dt;

  ed->camera.target = Vector3Add(ed->camera.position, forward);
  ed->camera.up     = (Vector3){0, 1, 0};

  // --- Mode-specific ---
  if (ed->navPaintMode) {

    if (ed->navPaletteOpen) {
      // Key selection
      if (IsKeyPressed(KEY_ONE))   { ed->navPaintType = 0; ed->navPaletteOpen = false; }
      if (IsKeyPressed(KEY_TWO))   { ed->navPaintType = 1; ed->navPaletteOpen = false; }
      if (IsKeyPressed(KEY_THREE)) { ed->navPaintType = 2; ed->navPaletteOpen = false; }
      if (IsKeyPressed(KEY_FOUR))  { ed->navPaintType = 3; ed->navPaletteOpen = false; }
      if (IsKeyPressed(KEY_FIVE))  { ed->navPaintType = 4; ed->navPaletteOpen = false; }
      if (IsKeyPressed(KEY_SIX))   { ed->navPaintType = 5; ed->navPaletteOpen = false; }
      if (IsKeyPressed(KEY_SEVEN)) { ed->navPaintType = 6; ed->navPaletteOpen = false; }
      if (IsKeyPressed(KEY_EIGHT)) { ed->navPaintType = 7; ed->navPaletteOpen = false; }
      // Mouse selection (coordinates match palette render below)
      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        int pw = 320, ph = 390;
        int ppx = sw/2 - pw/2, ppy = sh/2 - ph/2;
        Vector2 mp = GetMousePosition();
        for (int i = 0; i < 8; i++) {
          Rectangle r = {(float)(ppx+20), (float)(ppy+50+i*40), (float)(pw-40), 34};
          if (CheckCollisionPointRec(mp, r)) {
            ed->navPaintType = i;
            ed->navPaletteOpen = false;
            break;
          }
        }
      }
    } else {
      if (IsKeyPressed(KEY_P)) ed->navPaletteOpen = true;

      float scroll = GetMouseWheelMove();
      if (scroll > 0.0f && ed->navBrushSize < 5) ed->navBrushSize++;
      if (scroll < 0.0f && ed->navBrushSize > 1) ed->navBrushSize--;

      ed->hasHit = RaycastTerrain(ed, gw, &ed->hitPos);
      if (ed->hasHit) {
        bool doErase = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
        bool doPaint = IsMouseButtonDown(MOUSE_BUTTON_LEFT) || doErase;
        if (doPaint) {
          static const Color s_navColors[] = {
            {255,255,255,255}, // 0 EMPTY
            {0,  0,  0,  255}, // 1 WALL
            {0,  0,  255,255}, // 2 COVER_LOW
            {0,  255,0,  255}, // 3 COVER_HIGH
            {255,0,  0,  255}, // 4 BLOCKED
            {255,255,0,  255}, // 5 SNIPE
            {255,0,  255,255}, // 6 FLANK
            {0,  255,255,255}, // 7 FENCE
          };
          int   ci         = (doErase || ed->navPaintType < 0) ? 0 : ed->navPaintType;
          Color paintColor = s_navColors[ci < 8 ? ci : 0];
          int cgx = (int)((ed->hitPos.x + 180.0f) / 2.0f);
          int cgz = (int)((ed->hitPos.z + 180.0f) / 2.0f);
          int r   = ed->navBrushSize - 1;
          for (int dz = -r; dz <= r; dz++) {
            for (int dx = -r; dx <= r; dx++) {
              int nx = cgx + dx, nz = cgz + dz;
              if (nx < 0 || nx >= 180 || nz < 0 || nz >= 180) continue;
              ImageDrawPixel(&ed->navImage, nx, 179 - nz, paintColor);
            }
          }
        }
      }
    }

  } else if (ed->transformMode) {

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      int   bi = PickBox(ed), pi = PickProp(ed), ii = PickInfoBox(ed);
      int   si = PickSpawner(ed), wi = PickWallSeg(ed);
      int   tsi = PickTargetStatic(ed), tpi = PickTargetPatrol(ed);
      float bt = 1e9f, pt = 1e9f, it = 1e9f, st = 1e9f, spt = 1e9f, wt = 1e9f;
      float tsst = 1e9f, tppt = 1e9f;
      Vector2 c = {(float)GetScreenWidth()/2.0f, (float)GetScreenHeight()/2.0f};
      Ray ray = GetScreenToWorldRay(c, ed->camera);
      if (bi >= 0) {
        BoundingBox bb = {
            Vector3Subtract(ed->placed[bi].position, Vector3Scale(ed->placed[bi].scale, 0.5f)),
            Vector3Add(ed->placed[bi].position,      Vector3Scale(ed->placed[bi].scale, 0.5f)),
        };
        bt = GetRayCollisionBox(ray, bb).distance;
      }
      if (pi >= 0) {
        BoundingBox bb = {
            Vector3Subtract(ed->placedProps[pi].position, Vector3Scale(ed->placedProps[pi].scale, 0.5f)),
            Vector3Add(ed->placedProps[pi].position,      Vector3Scale(ed->placedProps[pi].scale, 0.5f)),
        };
        pt = GetRayCollisionBox(ray, bb).distance;
      }
      if (ii >= 0) {
        float he = ed->placedInfoBoxes[ii].halfExtent;
        Vector3 pos = ed->placedInfoBoxes[ii].position;
        BoundingBox bb = {{pos.x-he,pos.y-he,pos.z-he},{pos.x+he,pos.y+he,pos.z+he}};
        it = GetRayCollisionBox(ray, bb).distance;
      }
      if (si >= 0) {
        Vector3 p = ed->placedSpawners[si].position;
        BoundingBox bb = {{p.x-1.5f,p.y-1.5f,p.z-1.5f},{p.x+1.5f,p.y+1.5f,p.z+1.5f}};
        spt = GetRayCollisionBox(ray, bb).distance;
      }
      if (wi >= 0) {
        EditorPlacedWallSeg *ws = &ed->placedWallSegs[wi];
        BoundingBox bb = {
            {fminf(ws->ax,ws->bx)-ws->radius, ws->yBottom, fminf(ws->az,ws->bz)-ws->radius},
            {fmaxf(ws->ax,ws->bx)+ws->radius, ws->yTop,    fmaxf(ws->az,ws->bz)+ws->radius},
        };
        wt = GetRayCollisionBox(ray, bb).distance;
      }
      if (tsi >= 0) {
        Vector3 p = ed->placedTargetsStatic[tsi].position;
        BoundingBox bb = {{p.x-1.2f,p.y-0.5f,p.z-1.2f},{p.x+1.2f,p.y+3.0f,p.z+1.2f}};
        tsst = GetRayCollisionBox(ray, bb).distance;
      }
      if (tpi >= 0) {
        Vector3 pa = ed->placedTargetsPatrol[tpi].pointA;
        Vector3 pb = ed->placedTargetsPatrol[tpi].pointB;
        BoundingBox bbA = {{pa.x-1.2f,pa.y-0.5f,pa.z-1.2f},{pa.x+1.2f,pa.y+3.0f,pa.z+1.2f}};
        BoundingBox bbB = {{pb.x-1.2f,pb.y-0.5f,pb.z-1.2f},{pb.x+1.2f,pb.y+3.0f,pb.z+1.2f}};
        RayCollision ca = GetRayCollisionBox(ray, bbA);
        RayCollision cb = GetRayCollisionBox(ray, bbB);
        tppt = fminf(ca.hit ? ca.distance : 1e9f, cb.hit ? cb.distance : 1e9f);
      }
      PickSpawnPoint(ed, &st);

      float best = fminf(bt, fminf(pt, fminf(it, fminf(st, fminf(spt, fminf(wt, fminf(tsst, tppt)))))));
      if      (bt   == best && bi  >= 0)   { ed->selectedIndex = bi;  ed->selectedType = 0; }
      else if (pt   == best && pi  >= 0)   { ed->selectedIndex = pi;  ed->selectedType = 1; }
      else if (it   == best && ii  >= 0)   { ed->selectedIndex = ii;  ed->selectedType = 2; }
      else if (st   == best && st  < 1e9f) { ed->selectedIndex = 0;   ed->selectedType = 3; }
      else if (spt  == best && si  >= 0)   { ed->selectedIndex = si;  ed->selectedType = 4; }
      else if (wt   == best && wi  >= 0)   { ed->selectedIndex = wi;  ed->selectedType = 5; }
      else if (tsst == best && tsi >= 0)   { ed->selectedIndex = tsi; ed->selectedType = 6; }
      else if (tppt == best && tpi >= 0)   { ed->selectedIndex = tpi; ed->selectedType = 7; }
      else                                 { ed->selectedIndex = -1; }
    }

    if (ed->selectedIndex >= 0 && ed->selectedType == 0) {
      EditorPlacedBox *b = &ed->placed[ed->selectedIndex];
      bool ctrl  = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
      float step = ctrl ? 5.0f : 0.5f;
      bool changed = false;

      if (IsKeyPressed(KEY_RIGHT))     { b->position.x += step; changed = true; }
      if (IsKeyPressed(KEY_LEFT))      { b->position.x -= step; changed = true; }
      if (IsKeyPressed(KEY_UP))        { b->position.z -= step; changed = true; }
      if (IsKeyPressed(KEY_DOWN))      { b->position.z += step; changed = true; }
      if (IsKeyPressed(KEY_PAGE_UP))   { b->position.y += step; changed = true; }
      if (IsKeyPressed(KEY_PAGE_DOWN)) { b->position.y -= step; changed = true; }

      float scroll = GetMouseWheelMove();
      if (scroll != 0.0f) {
        float ss = 0.5f * scroll;
        if (IsKeyDown(KEY_X))      b->scale.x = fmaxf(0.5f, b->scale.x + ss);
        else if (IsKeyDown(KEY_Y)) b->scale.y = fmaxf(0.5f, b->scale.y + ss);
        else if (IsKeyDown(KEY_Z)) b->scale.z = fmaxf(0.5f, b->scale.z + ss);
        else { float ns = fmaxf(0.5f, b->scale.x + ss); b->scale = (Vector3){ns,ns,ns}; }
        changed = true;
      }
      if (changed) SyncBoxEntity(ed, world, ed->selectedIndex);

      if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) {
        ModelCollection_t *mc = ECS_GET(world, b->entity, ModelCollection_t, COMP_MODEL);
        if (mc) ModelCollectionFree(mc);
        WorldDestroyEntity(world, b->entity);
        for (int i = ed->selectedIndex; i < ed->placedCount - 1; i++)
          ed->placed[i] = ed->placed[i + 1];
        ed->placedCount--;
        ed->selectedIndex = -1;
      }
    }

    if (ed->selectedIndex >= 0 && ed->selectedType == 1) {
      EditorPlacedProp *p = &ed->placedProps[ed->selectedIndex];
      bool ctrl  = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
      float step = ctrl ? 5.0f : 0.5f;

      if (IsKeyPressed(KEY_RIGHT))     p->position.x += step;
      if (IsKeyPressed(KEY_LEFT))      p->position.x -= step;
      if (IsKeyPressed(KEY_UP))        p->position.z -= step;
      if (IsKeyPressed(KEY_DOWN))      p->position.z += step;
      if (IsKeyPressed(KEY_PAGE_UP))   p->position.y += step;
      if (IsKeyPressed(KEY_PAGE_DOWN)) p->position.y -= step;

      float scroll = GetMouseWheelMove();
      if (scroll != 0.0f) {
        if (IsKeyDown(KEY_R)) {
          p->yaw += scroll * 0.2f;
        } else {
          float ns = fmaxf(0.25f, p->scale.x + scroll * 0.25f);
          p->scale = (Vector3){ns, ns, ns};
        }
      }

      if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) {
        for (int i = ed->selectedIndex; i < ed->propCount - 1; i++)
          ed->placedProps[i] = ed->placedProps[i + 1];
        ed->propCount--;
        ed->selectedIndex = -1;
      }
    }

    if (ed->selectedIndex >= 0 && ed->selectedType == 2) {
      EditorPlacedInfoBox *ib = &ed->placedInfoBoxes[ed->selectedIndex];
      bool ctrl  = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
      float step = ctrl ? 5.0f : 0.5f;

      if (IsKeyPressed(KEY_RIGHT))     ib->position.x += step;
      if (IsKeyPressed(KEY_LEFT))      ib->position.x -= step;
      if (IsKeyPressed(KEY_UP))        ib->position.z -= step;
      if (IsKeyPressed(KEY_DOWN))      ib->position.z += step;
      if (IsKeyPressed(KEY_PAGE_UP))   ib->position.y += step;
      if (IsKeyPressed(KEY_PAGE_DOWN)) ib->position.y -= step;

      float scroll = GetMouseWheelMove();
      if (scroll != 0.0f) {
        if (IsKeyDown(KEY_R)) {
          ib->markerHeight += scroll * 0.25f;
          ib->markerHeight = fmaxf(-10.0f, fminf(10.0f, ib->markerHeight));
        } else {
          ib->halfExtent = fmaxf(0.5f, ib->halfExtent + scroll * 0.25f);
        }
      }

      if (IsKeyPressed(KEY_E) && !ed->infoBoxEditOpen) {
        ed->infoBoxHalfExtent   = ib->halfExtent;
        ed->infoBoxDuration     = ib->duration;
        ed->infoBoxMaxTriggers  = ib->triggerCount;
        ed->infoBoxMarkerHeight = ib->markerHeight;
        ed->infoBoxFontSize     = ib->fontSize;
        strncpy(ed->infoBoxTextBuf, ib->message, 255);
        ed->infoBoxTextBuf[255] = '\0';
        ed->infoBoxTextLen      = (int)strlen(ed->infoBoxTextBuf);
        ed->infoBoxEditExisting = true;
        ed->infoBoxEditOpen     = true;
        EnableCursor();
      }

      if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) {
        for (int i = ed->selectedIndex; i < ed->infoBoxCount - 1; i++)
          ed->placedInfoBoxes[i] = ed->placedInfoBoxes[i + 1];
        ed->infoBoxCount--;
        ed->selectedIndex = -1;
      }
    }

    if (ed->selectedIndex >= 0 && ed->selectedType == 3 && ed->edHasSpawnPoint) {
      bool ctrl  = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
      float step = ctrl ? 5.0f : 0.5f;

      if (IsKeyPressed(KEY_RIGHT))     ed->edSpawnPoint.x += step;
      if (IsKeyPressed(KEY_LEFT))      ed->edSpawnPoint.x -= step;
      if (IsKeyPressed(KEY_UP))        ed->edSpawnPoint.z -= step;
      if (IsKeyPressed(KEY_DOWN))      ed->edSpawnPoint.z += step;
      if (IsKeyPressed(KEY_PAGE_UP))   ed->edSpawnPoint.y += step;
      if (IsKeyPressed(KEY_PAGE_DOWN)) ed->edSpawnPoint.y -= step;

      if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) {
        ed->edHasSpawnPoint = false;
        ed->selectedIndex   = -1;
      }
    }

    if (ed->selectedIndex >= 0 && ed->selectedType == 4) {
      EditorPlacedSpawner *sp = &ed->placedSpawners[ed->selectedIndex];
      bool ctrl  = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
      float step = ctrl ? 5.0f : 0.5f;

      if (IsKeyPressed(KEY_RIGHT))     sp->position.x += step;
      if (IsKeyPressed(KEY_LEFT))      sp->position.x -= step;
      if (IsKeyPressed(KEY_UP))        sp->position.z -= step;
      if (IsKeyPressed(KEY_DOWN))      sp->position.z += step;
      if (IsKeyPressed(KEY_PAGE_UP))   sp->position.y += step;
      if (IsKeyPressed(KEY_PAGE_DOWN)) sp->position.y -= step;

      float scroll = GetMouseWheelMove();
      if (scroll != 0.0f)
        sp->enemyType = (sp->enemyType == 0) ? 1 : 0;

      if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) {
        for (int i = ed->selectedIndex; i < ed->spawnerCount - 1; i++)
          ed->placedSpawners[i] = ed->placedSpawners[i + 1];
        ed->spawnerCount--;
        ed->selectedIndex = -1;
      }
    }

    if (ed->selectedIndex >= 0 && ed->selectedType == 5) {
      EditorPlacedWallSeg *ws = &ed->placedWallSegs[ed->selectedIndex];
      bool ctrl  = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
      float step = ctrl ? 5.0f : 0.5f;

      if (IsKeyPressed(KEY_RIGHT))     { ws->ax += step; ws->bx += step; }
      if (IsKeyPressed(KEY_LEFT))      { ws->ax -= step; ws->bx -= step; }
      if (IsKeyPressed(KEY_UP))        { ws->az -= step; ws->bz -= step; }
      if (IsKeyPressed(KEY_DOWN))      { ws->az += step; ws->bz += step; }
      if (IsKeyPressed(KEY_PAGE_UP))   { ws->yBottom += step; ws->yTop += step; }
      if (IsKeyPressed(KEY_PAGE_DOWN)) { ws->yBottom -= step; ws->yTop -= step; }

      if (IsKeyPressed(KEY_E) && !ed->wallSegDialogOpen) {
        ed->wallSegHeight            = ws->yTop - ws->yBottom;
        ed->wallSegRadius            = ws->radius;
        ed->wallSegBlockPlayer       = ws->blockPlayer;
        ed->wallSegBlockProjectiles  = ws->blockProjectiles;
        ed->wallSegEditExisting      = true;
        ed->wallSegDialogOpen        = true;
        EnableCursor();
      }

      if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) {
        for (int i = ed->selectedIndex; i < ed->wallSegCount - 1; i++)
          ed->placedWallSegs[i] = ed->placedWallSegs[i + 1];
        ed->wallSegCount--;
        ed->selectedIndex = -1;
      }
    }

    if (ed->selectedIndex >= 0 && ed->selectedType == 6) {
      EditorPlacedTargetStatic *ts = &ed->placedTargetsStatic[ed->selectedIndex];
      bool ctrl  = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
      float step = ctrl ? 5.0f : 0.5f;
      bool changed = false;

      if (IsKeyPressed(KEY_RIGHT))     { ts->position.x += step; changed = true; }
      if (IsKeyPressed(KEY_LEFT))      { ts->position.x -= step; changed = true; }
      if (IsKeyPressed(KEY_UP))        { ts->position.z -= step; changed = true; }
      if (IsKeyPressed(KEY_DOWN))      { ts->position.z += step; changed = true; }
      if (IsKeyPressed(KEY_PAGE_UP))   { ts->position.y += step; changed = true; }
      if (IsKeyPressed(KEY_PAGE_DOWN)) { ts->position.y -= step; changed = true; }

      float scroll = GetMouseWheelMove();
      if (scroll != 0.0f) { ts->yaw += scroll * 0.2f; changed = true; }

      if (changed && ts->entity.id) SyncTargetPosition(world, ts->entity, ts->position, ts->yaw);

      if (IsKeyPressed(KEY_E) && !ed->targetDialogOpen) {
        ed->targetIsPatrol       = false;
        ed->targetEditExisting   = true;
        ed->targetEditIndex      = ed->selectedIndex;
        ed->targetDlgHealth      = ts->health;
        ed->targetDlgShield      = ts->shield;
        ed->targetDlgHealthCount  = ts->healthDropCount;
        ed->targetDlgCoolantCount = ts->coolantDropCount;
        ed->targetDlgYaw         = ts->yaw;
        ed->targetDlgField       = 0;
        ed->targetDialogOpen     = true;
        EnableCursor();
      }

      if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) {
        if (ts->entity.id) {
          ModelCollection_t *mc = ECS_GET(world, ts->entity, ModelCollection_t, COMP_MODEL);
          if (mc) ModelCollectionFree(mc);
          WorldDestroyEntity(world, ts->entity);
        }
        for (int i = ed->selectedIndex; i < ed->targetStaticCount - 1; i++)
          ed->placedTargetsStatic[i] = ed->placedTargetsStatic[i + 1];
        ed->targetStaticCount--;
        ed->selectedIndex = -1;
      }
    }

    if (ed->selectedIndex >= 0 && ed->selectedType == 7) {
      EditorPlacedTargetPatrol *tp = &ed->placedTargetsPatrol[ed->selectedIndex];
      bool ctrl  = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
      float step = ctrl ? 5.0f : 0.5f;
      bool changed = false;

      if (IsKeyPressed(KEY_RIGHT))     { tp->pointA.x += step; tp->pointB.x += step; changed = true; }
      if (IsKeyPressed(KEY_LEFT))      { tp->pointA.x -= step; tp->pointB.x -= step; changed = true; }
      if (IsKeyPressed(KEY_UP))        { tp->pointA.z -= step; tp->pointB.z -= step; changed = true; }
      if (IsKeyPressed(KEY_DOWN))      { tp->pointA.z += step; tp->pointB.z += step; changed = true; }
      if (IsKeyPressed(KEY_PAGE_UP))   { tp->pointA.y += step; tp->pointB.y += step; changed = true; }
      if (IsKeyPressed(KEY_PAGE_DOWN)) { tp->pointA.y -= step; tp->pointB.y -= step; changed = true; }

      float scroll = GetMouseWheelMove();
      if (scroll != 0.0f) { tp->yaw += scroll * 0.2f; changed = true; }

      if (changed && tp->entity.id) {
        SyncTargetPosition(world, tp->entity, tp->pointA, tp->yaw);
        TargetPatrol *tpp = ECS_GET(world, tp->entity, TargetPatrol, COMP_TARGET_PATROL);
        if (tpp) { tpp->pointA = tp->pointA; tpp->pointB = tp->pointB; }
      }

      if (IsKeyPressed(KEY_E) && !ed->targetDialogOpen) {
        ed->targetIsPatrol       = true;
        ed->targetEditExisting   = true;
        ed->targetEditIndex      = ed->selectedIndex;
        ed->targetDlgHealth      = tp->health;
        ed->targetDlgShield      = tp->shield;
        ed->targetDlgSpeed       = tp->speed;
        ed->targetDlgHealthCount  = tp->healthDropCount;
        ed->targetDlgCoolantCount = tp->coolantDropCount;
        ed->targetDlgYaw         = tp->yaw;
        ed->targetDlgField       = 0;
        ed->targetDialogOpen     = true;
        EnableCursor();
      }

      if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) {
        if (tp->entity.id) {
          ModelCollection_t *mc = ECS_GET(world, tp->entity, ModelCollection_t, COMP_MODEL);
          if (mc) ModelCollectionFree(mc);
          WorldDestroyEntity(world, tp->entity);
        }
        for (int i = ed->selectedIndex; i < ed->targetPatrolCount - 1; i++)
          ed->placedTargetsPatrol[i] = ed->placedTargetsPatrol[i + 1];
        ed->targetPatrolCount--;
        ed->selectedIndex = -1;
      }
    }

  } else {
    // --- Place mode ---

    // Scroll: resize box scale in box mode, rotate prop yaw in prop mode
    float scroll = GetMouseWheelMove();
    if (ed->placeType == 0 && scroll != 0.0f) {
      ed->boxScale += scroll * 0.5f;
      if (ed->boxScale < 0.5f) ed->boxScale = 0.5f;
    }
    if (ed->placeType == 3 && scroll != 0.0f) {
      if (IsKeyDown(KEY_LEFT_SHIFT)) {
        ed->propPlaceScale += scroll * 0.25f;
        if (ed->propPlaceScale < 0.25f) ed->propPlaceScale = 0.25f;
      } else {
        ed->propPlaceYaw += scroll * 0.2f;
      }
    }
    if (ed->placeType == 4 && scroll != 0.0f) {
      ed->infoBoxHalfExtent += scroll * 0.25f;
      if (ed->infoBoxHalfExtent < 0.5f)  ed->infoBoxHalfExtent = 0.5f;
      if (ed->infoBoxHalfExtent > 20.0f) ed->infoBoxHalfExtent = 20.0f;
    }
    if ((ed->placeType == 7 || ed->placeType == 8) && scroll != 0.0f) {
      ed->targetDlgYaw += scroll * 0.2f;
    }

    ed->hasHit = RaycastTerrain(ed, gw, &ed->hitPos);

    // Only place on LMB when the object panel is not intercepting clicks
    bool panelConsumedClick = false;
    if (ed->objectPanelOpen) panelConsumedClick = true;

    if (!panelConsumedClick && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && ed->hasHit) {
      int histCap = EDITOR_MAX_BOXES + EDITOR_MAX_SPAWNERS + EDITOR_MAX_PROPS +
                    EDITOR_MAX_INFOBOXES + EDITOR_MAX_WALLSEGS;
      if (ed->placeType == 0) {
        ed->arrayOrigin     = ed->hitPos;
        ed->arraySpacing    = ed->boxScale;
        ed->arrayDialogOpen = true;
        EnableCursor();
      } else if ((ed->placeType == 1 || ed->placeType == 2) &&
                 ed->spawnerCount < EDITOR_MAX_SPAWNERS) {
        int enemyType = ed->placeType - 1; // 0=grunt, 1=ranger
        ed->placedSpawners[ed->spawnerCount++] = (EditorPlacedSpawner){
            .position  = ed->hitPos,
            .enemyType = enemyType,
        };
        if (ed->historyTop < histCap) ed->history[ed->historyTop++] = 1;
      } else if (ed->placeType == 3 && ed->propCount < EDITOR_MAX_PROPS) {
        bool wantSkybox = (strstr(ed->propPlaceModel, "skybox") != NULL);
        bool hasSkybox  = false;
        if (wantSkybox) {
          for (int pi = 0; pi < ed->propCount; pi++)
            if (strstr(ed->placedProps[pi].modelPath, "skybox") != NULL) { hasSkybox = true; break; }
        }
        if (!wantSkybox || !hasSkybox) {
          float s = ed->propPlaceScale;
          EditorPlacedProp *pp = &ed->placedProps[ed->propCount++];
          pp->position = ed->hitPos;
          pp->yaw      = ed->propPlaceYaw;
          pp->scale    = (Vector3){s, s, s};
          strncpy(pp->modelPath, ed->propPlaceModel, 255);
          if (ed->historyTop < histCap) ed->history[ed->historyTop++] = 2;
        }
      } else if (ed->placeType == 4 && ed->infoBoxCount < EDITOR_MAX_INFOBOXES
                 && !ed->infoBoxEditOpen) {
        float hy = HeightMap_GetHeightCatmullRom(
            &gw->terrainHeightMap, ed->hitPos.x, ed->hitPos.z);
        ed->infoBoxPendingPos = (Vector3){
            ed->hitPos.x,
            hy + ed->infoBoxHalfExtent,
            ed->hitPos.z};
        ed->infoBoxTextLen    = 0;
        ed->infoBoxTextBuf[0] = '\0';
        ed->infoBoxEditOpen   = true;
        EnableCursor();
      } else if (ed->placeType == 5) {
        float hy = HeightMap_GetHeightCatmullRom(
            &gw->terrainHeightMap, ed->hitPos.x, ed->hitPos.z);
        ed->edSpawnPoint    = (Vector3){ed->hitPos.x, hy + 1.8f, ed->hitPos.z};
        ed->edHasSpawnPoint = true;
      } else if (ed->placeType == 6 && !ed->wallSegDialogOpen) {
        if (ed->wallSegStep == 0) {
          ed->wallSegPendingA = ed->hitPos;
          ed->wallSegStep     = 1;
        } else if (ed->wallSegStep == 1) {
          ed->wallSegPendingB   = ed->hitPos;
          ed->wallSegStep       = 2;
          ed->wallSegDialogOpen = true;
          EnableCursor();
        }
      } else if (ed->placeType == 7 && !ed->targetDialogOpen) {
        // Static target — single click opens config dialog
        ed->targetPendingA        = ed->hitPos;
        ed->targetIsPatrol        = false;
        ed->targetEditExisting    = false;
        ed->targetDlgHealth       = 100.0f;
        ed->targetDlgShield       = 0.0f;
        ed->targetDlgHealthCount   = 0.0f;
        ed->targetDlgCoolantCount  = 0.0f;
        ed->targetDlgField        = 0;
        ed->targetDialogOpen      = true;
        EnableCursor();
      } else if (ed->placeType == 8 && !ed->targetDialogOpen) {
        // Patrol target — two clicks then dialog
        if (ed->targetPatrolStep == 0) {
          ed->targetPendingA    = ed->hitPos;
          ed->targetPatrolStep  = 1;
        } else if (ed->targetPatrolStep == 1) {
          ed->targetPendingB        = ed->hitPos;
          ed->targetIsPatrol        = true;
          ed->targetEditExisting    = false;
          ed->targetDlgHealth       = 100.0f;
          ed->targetDlgShield       = 0.0f;
          ed->targetDlgSpeed        = 5.0f;
          ed->targetDlgHealthCount   = 0.0f;
          ed->targetDlgCoolantCount  = 0.0f;
          ed->targetDlgField        = 0;
          ed->targetDialogOpen      = true;
          EnableCursor();
        }
      }
    }

    if (IsKeyPressed(KEY_Z) && ed->historyTop > 0) {
      uint8_t t = ed->history[--ed->historyTop];
      if (t == 0 && ed->placedCount > 0) {
        EditorPlacedBox *last = &ed->placed[--ed->placedCount];
        ModelCollection_t *mc =
            ECS_GET(world, last->entity, ModelCollection_t, COMP_MODEL);
        if (mc) ModelCollectionFree(mc);
        WorldDestroyEntity(world, last->entity);
      } else if (t == 1 && ed->spawnerCount > 0) {
        ed->spawnerCount--;
      } else if (t == 2 && ed->propCount > 0) {
        ed->propCount--;
      } else if (t == 4 && ed->infoBoxCount > 0) {
        ed->infoBoxCount--;
      } else if (t == 5 && ed->wallSegCount > 0) {
        ed->wallSegCount--;
      }
    }
  }

  // --- Save (both modes) ---
  bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
  if (ctrl && IsKeyPressed(KEY_S)) {
    ed->nameLen       = 0;
    ed->nameBuffer[0] = '\0';
    ed->isNaming      = true;
  }
}

void EditorRender(EditorState *ed, GameWorld *gw) {
  BeginDrawing();
  ClearBackground((Color){20, 20, 30, 255});
  BeginMode3D(ed->camera);

  DrawModel(gw->terrainModel, (Vector3){0, 0, 0}, 1.0f, WHITE);

  gw->obstacleModel.transform = MatrixIdentity();
  for (int i = 0; i < ed->placedCount; i++) {
    EditorPlacedBox *b = &ed->placed[i];
    bool selected = (ed->transformMode && i == ed->selectedIndex);

    Color tint  = selected ? YELLOW   : WHITE;
    Color wires = selected ? SKYBLUE  : ORANGE;

    DrawModelEx(gw->obstacleModel, b->position, (Vector3){0, 1, 0}, 0.0f,
                b->scale, tint);
    DrawCubeWires(b->position, b->scale.x, b->scale.y, b->scale.z, wires);
  }

  // Placed props (visual-only)
  for (int i = 0; i < ed->propCount; i++) {
    EditorPlacedProp *p = &ed->placedProps[i];
    bool sel = (ed->transformMode && ed->selectedType == 1 && i == ed->selectedIndex);
    Model m = (p->modelPath[0] != '\0') ? EditorGetModel(p->modelPath) : gw->obstacleModel;
    DrawModelEx(m, p->position, (Vector3){0,1,0},
                p->yaw * RAD2DEG, p->scale, sel ? YELLOW : WHITE);
    DrawCubeWires(p->position, p->scale.x, p->scale.y, p->scale.z,
                  sel ? SKYBLUE : GREEN);
  }

  // Placed spawner wireframes
  for (int i = 0; i < ed->spawnerCount; i++) {
    EditorPlacedSpawner *s = &ed->placedSpawners[i];
    bool sel = (ed->transformMode && ed->selectedType == 4 && i == ed->selectedIndex);
    Color col = sel ? YELLOW : (s->enemyType == 0) ? RED : BLUE;
    if (sel) DrawSphere(s->position, 1.5f, (Color){255,255,0,50});
    DrawSphereWires(s->position, 1.5f, 8, 8, col);
    DrawLine3D(Vector3Add(s->position, (Vector3){-2,0,0}),
               Vector3Add(s->position, (Vector3){ 2,0,0}), col);
    DrawLine3D(Vector3Add(s->position, (Vector3){0,0,-2}),
               Vector3Add(s->position, (Vector3){0,0, 2}), col);
  }

  // Placed info box wireframes
  for (int i = 0; i < ed->infoBoxCount; i++) {
    EditorPlacedInfoBox *ib = &ed->placedInfoBoxes[i];
    bool sel = (ed->transformMode && ed->selectedType == 2 && i == ed->selectedIndex);
    float s = ib->halfExtent * 2.0f;
    DrawCube(ib->position, s, s, s, sel ? (Color){0, 200, 180, 55} : (Color){0, 200, 180, 18});
    DrawCubeWires(ib->position, s, s, s, sel ? YELLOW : (Color){0, 220, 200, 255});
  }

  // Placed wall segment wireframes
  for (int i = 0; i < ed->wallSegCount; i++) {
    EditorPlacedWallSeg *ws = &ed->placedWallSegs[i];
    bool sel = (ed->transformMode && ed->selectedType == 5 && i == ed->selectedIndex);
    float dx = ws->bx - ws->ax, dz = ws->bz - ws->az;
    float len = sqrtf(dx * dx + dz * dz);
    Vector3 perp = (len > 1e-6f)
        ? (Vector3){-dz / len * ws->radius, 0.0f, dx / len * ws->radius}
        : (Vector3){ws->radius, 0.0f, 0.0f};
    float yb = ws->yBottom, yt = ws->yTop;
    Vector3 c[8] = {
        {ws->ax + perp.x, yb, ws->az + perp.z},
        {ws->ax - perp.x, yb, ws->az - perp.z},
        {ws->bx - perp.x, yb, ws->bz - perp.z},
        {ws->bx + perp.x, yb, ws->bz + perp.z},
        {ws->ax + perp.x, yt, ws->az + perp.z},
        {ws->ax - perp.x, yt, ws->az - perp.z},
        {ws->bx - perp.x, yt, ws->bz - perp.z},
        {ws->bx + perp.x, yt, ws->bz + perp.z},
    };
    Color col = sel ? SKYBLUE : ORANGE;
    DrawLine3D(c[0], c[1], col); DrawLine3D(c[1], c[2], col);
    DrawLine3D(c[2], c[3], col); DrawLine3D(c[3], c[0], col);
    DrawLine3D(c[4], c[5], col); DrawLine3D(c[5], c[6], col);
    DrawLine3D(c[6], c[7], col); DrawLine3D(c[7], c[4], col);
    DrawLine3D(c[0], c[4], col); DrawLine3D(c[1], c[5], col);
    DrawLine3D(c[2], c[6], col); DrawLine3D(c[3], c[7], col);
    if (sel) {
      DrawSphere((Vector3){ws->ax, yb, ws->az}, 0.3f, (Color){100,220,255,180});
      DrawSphere((Vector3){ws->bx, yb, ws->bz}, 0.3f, (Color){100,220,255,180});
    }
  }

  // Placed static target wireframes
  for (int i = 0; i < ed->targetStaticCount; i++) {
    EditorPlacedTargetStatic *ts = &ed->placedTargetsStatic[i];
    bool sel = (ed->transformMode && ed->selectedType == 6 && i == ed->selectedIndex);
    Color col = sel ? YELLOW : ORANGE;
    DrawCylinderWires(ts->position, 1.0f, 1.0f, 2.5f, 8, col);
    if (sel) DrawCylinderWires(ts->position, 1.3f, 1.3f, 3.0f, 8, SKYBLUE);
  }

  // Placed patrol target wireframes
  for (int i = 0; i < ed->targetPatrolCount; i++) {
    EditorPlacedTargetPatrol *tp = &ed->placedTargetsPatrol[i];
    bool sel = (ed->transformMode && ed->selectedType == 7 && i == ed->selectedIndex);
    Color col = sel ? YELLOW : PURPLE;
    DrawCylinderWires(tp->pointA, 1.0f, 1.0f, 2.5f, 8, col);
    DrawCylinderWires(tp->pointB, 1.0f, 1.0f, 2.5f, 8, col);
    DrawLine3D(tp->pointA, tp->pointB, col);
    if (sel) {
      DrawCylinderWires(tp->pointA, 1.3f, 1.3f, 3.0f, 8, SKYBLUE);
      DrawCylinderWires(tp->pointB, 1.3f, 1.3f, 3.0f, 8, SKYBLUE);
    }
  }

  // Box array preview — ghost boxes while dialog is open
  if (ed->arrayDialogOpen) {
    static const Vector3 dirs[6] = {{1,0,0},{-1,0,0},{0,0,1},{0,0,-1},{0,1,0},{0,-1,0}};
    Vector3 d = dirs[ed->arrayDir];
    float s = ed->boxScale;
    for (int i = 0; i < ed->arrayCount; i++) {
      Vector3 p = {
        ed->arrayOrigin.x + d.x * ed->arraySpacing * i,
        ed->arrayOrigin.y + d.y * ed->arraySpacing * i,
        ed->arrayOrigin.z + d.z * ed->arraySpacing * i,
      };
      DrawCube(p, s, s, s, (Color){80, 190, 255, 50});
      DrawCubeWires(p, s, s, s, (Color){80, 210, 255, 200});
    }
  }

  // Place-mode preview ghost
  if (!ed->transformMode && !ed->navPaintMode && ed->hasHit && !ed->objectPanelOpen) {
    if (ed->placeType == 0) {
      float s = ed->boxScale;
      DrawCube(ed->hitPos, s, s, s, (Color){255, 255, 100, 60});
      DrawCubeWires(ed->hitPos, s, s, s, YELLOW);
    } else if (ed->placeType == 1 || ed->placeType == 2) {
      Color col = (ed->placeType == 1) ? RED : BLUE;
      DrawSphere(ed->hitPos, 1.5f, (Color){col.r, col.g, col.b, 80});
      DrawSphereWires(ed->hitPos, 1.5f, 8, 8, col);
    } else if (ed->placeType == 3) {
      float s = ed->propPlaceScale;
      Model m = EditorGetModel(ed->propPlaceModel);
      DrawModelEx(m, ed->hitPos, (Vector3){0,1,0},
                  ed->propPlaceYaw * RAD2DEG, (Vector3){s,s,s},
                  (Color){100, 255, 100, 80});
      DrawCubeWires(ed->hitPos, s, s, s, GREEN);
    } else if (ed->placeType == 4) {
      float s = ed->infoBoxHalfExtent * 2.0f;
      float hy = HeightMap_GetHeightCatmullRom(
          &gw->terrainHeightMap, ed->hitPos.x, ed->hitPos.z);
      Vector3 gp = {ed->hitPos.x, hy + ed->infoBoxHalfExtent, ed->hitPos.z};
      DrawCube(gp, s, s, s, (Color){0, 200, 180, 35});
      DrawCubeWires(gp, s, s, s, (Color){0, 220, 200, 200});
    } else if (ed->placeType == 6) {
      if (ed->wallSegStep == 0) {
        DrawSphereWires(ed->hitPos, 0.35f, 6, 6, ORANGE);
      } else if (ed->wallSegStep == 1) {
        DrawSphere(ed->wallSegPendingA, 0.35f, (Color){255, 165, 0, 180});
        DrawSphereWires(ed->wallSegPendingA, 0.35f, 6, 6, ORANGE);
        DrawLine3D(ed->wallSegPendingA, ed->hitPos, (Color){255, 165, 0, 200});
        DrawSphereWires(ed->hitPos, 0.35f, 6, 6, (Color){255, 165, 0, 120});
      }
    } else if (ed->placeType == 7) {
      DrawModelEx(gw->gruntLegs,  ed->hitPos, (Vector3){0,1,0}, ed->targetDlgYaw * RAD2DEG, (Vector3){1,1,1}, (Color){255,80,80,80});
      DrawModelEx(gw->gruntTorso, ed->hitPos, (Vector3){0,1,0}, ed->targetDlgYaw * RAD2DEG, (Vector3){1,1,1}, (Color){255,80,80,80});
    } else if (ed->placeType == 8) {
      if (ed->targetPatrolStep == 0) {
        DrawModelEx(gw->gruntLegs,  ed->hitPos, (Vector3){0,1,0}, ed->targetDlgYaw * RAD2DEG, (Vector3){1,1,1}, (Color){255,160,60,80});
        DrawModelEx(gw->gruntTorso, ed->hitPos, (Vector3){0,1,0}, ed->targetDlgYaw * RAD2DEG, (Vector3){1,1,1}, (Color){255,160,60,80});
      } else if (ed->targetPatrolStep == 1) {
        DrawModelEx(gw->gruntLegs,  ed->targetPendingA, (Vector3){0,1,0}, ed->targetDlgYaw * RAD2DEG, (Vector3){1,1,1}, (Color){255,140,40,120});
        DrawModelEx(gw->gruntTorso, ed->targetPendingA, (Vector3){0,1,0}, ed->targetDlgYaw * RAD2DEG, (Vector3){1,1,1}, (Color){255,140,40,120});
        DrawLine3D(ed->targetPendingA, ed->hitPos, (Color){255, 160, 60, 200});
        DrawModelEx(gw->gruntLegs,  ed->hitPos, (Vector3){0,1,0}, ed->targetDlgYaw * RAD2DEG, (Vector3){1,1,1}, (Color){255,160,60,60});
        DrawModelEx(gw->gruntTorso, ed->hitPos, (Vector3){0,1,0}, ed->targetDlgYaw * RAD2DEG, (Vector3){1,1,1}, (Color){255,160,60,60});
      }
    }
  }

  // Draw placed target wireframes
  for (int i = 0; i < ed->targetStaticCount; i++) {
    EditorPlacedTargetStatic *ts = &ed->placedTargetsStatic[i];
    DrawSphereWires(ts->position, 1.0f, 8, 8, (Color){255, 80, 80, 160});
  }
  for (int i = 0; i < ed->targetPatrolCount; i++) {
    EditorPlacedTargetPatrol *tp = &ed->placedTargetsPatrol[i];
    DrawSphereWires(tp->pointA, 1.0f, 8, 8, (Color){255, 160, 60, 160});
    DrawSphereWires(tp->pointB, 1.0f, 8, 8, (Color){255, 160, 60, 160});
    DrawLine3D(tp->pointA, tp->pointB, (Color){255, 160, 60, 140});
  }

  // Nav grid overlay
  if (ed->navImageLoaded) {
    if (!s_navHeightCacheValid) {
      for (int gz = 0; gz < 180; gz++)
        for (int gx = 0; gx < 180; gx++) {
          float wx = gx * 2.0f - 180.0f + 1.0f;
          float wz = gz * 2.0f - 180.0f + 1.0f;
          s_navHeightCache[gz][gx] =
              HeightMap_GetHeightCatmullRom(&gw->terrainHeightMap, wx, wz);
        }
      s_navHeightCacheValid = true;
    }
    unsigned char *px = (unsigned char *)ed->navImage.data;
    rlDisableDepthMask();
    rlBegin(RL_QUADS);
    for (int gz = 0; gz < 180; gz++) {
      for (int gx = 0; gx < 180; gx++) {
        int img_y = 179 - gz;
        int base  = (img_y * 180 + gx) * 4;
        uint8_t r = px[base], g = px[base+1], b = px[base+2];
        // Skip white (EMPTY) — everything else gets a colour
        if (r > 240 && g > 240 && b > 240) continue;
        uint8_t dr, dg, db;
        if      (r < 10  && g < 10  && b < 10)  { dr=220; dg=40;  db=40;  } // WALL
        else if (b > 200 && r < 50  && g < 50)  { dr=40;  dg=100; db=220; } // COVER_LOW
        else if (g > 200 && r < 50  && b < 50)  { dr=40;  dg=190; db=40;  } // COVER_HIGH
        else if (r > 200 && g < 50  && b < 50)  { dr=110; dg=15;  db=15;  } // BLOCKED
        else if (r > 200 && g > 200 && b < 50)  { dr=220; dg=210; db=40;  } // SNIPE
        else if (r > 200 && b > 200 && g < 50)  { dr=200; dg=40;  db=200; } // FLANK
        else if (g > 200 && b > 200 && r < 50)  { dr=40;  dg=200; db=200; } // FENCE
        else continue;
        float x0 = gx * 2.0f - 180.0f, x1 = x0 + 2.0f;
        float z0 = gz * 2.0f - 180.0f, z1 = z0 + 2.0f;
        float y  = s_navHeightCache[gz][gx] + 0.15f;
        rlColor4ub(dr, dg, db, 160);
        rlVertex3f(x0, y, z0);
        rlVertex3f(x0, y, z1);
        rlVertex3f(x1, y, z1);
        rlVertex3f(x1, y, z0);
      }
    }
    rlEnd();
    rlEnableDepthMask();
  }

  // Nav brush cursor
  if (ed->navPaintMode && ed->hasHit) {
    float dia = (float)((ed->navBrushSize * 2 - 1) * 2);
    static const Color s_brushCols[] = {
      RAYWHITE,
      {220,60, 60, 255},
      {60, 100,220,255},
      {60, 200,60, 255},
      {200,55, 55, 255},
      {220,210,40, 255},
      {200,40, 200,255},
      {40, 200,200,255},
    };
    int bci = (ed->navPaintType >= 0 && ed->navPaintType < 8) ? ed->navPaintType : 0;
    Color col = s_brushCols[bci];
    DrawCubeWires(ed->hitPos, dia, 0.3f, dia, col);
  }

  // Player spawn point marker
  if (ed->edHasSpawnPoint) {
    Vector3 sp   = ed->edSpawnPoint;
    bool    selSp = (ed->transformMode && ed->selectedType == 3 && ed->selectedIndex == 0);
    Color   ringC = selSp ? YELLOW : (Color){50, 255, 80, 255};
    // Vertical pole from ground to spawn height
    DrawLine3D((Vector3){sp.x, sp.y - 1.8f, sp.z}, (Vector3){sp.x, sp.y + 2.0f, sp.z}, ringC);
    // Sphere at the spawn position
    DrawSphere(sp, 0.25f, ringC);
    DrawSphereWires(sp, 0.25f, 6, 6, selSp ? YELLOW : GREEN);
    // Ground ring
    DrawCircle3D((Vector3){sp.x, sp.y - 1.8f, sp.z}, 0.8f, (Vector3){1,0,0}, 90.0f, ringC);
  }

  EndMode3D();

  int cx = GetScreenWidth() / 2;
  int cy = GetScreenHeight() / 2;
  DrawLine(cx - 10, cy, cx + 10, cy, WHITE);
  DrawLine(cx, cy - 10, cx, cy + 10, WHITE);

  // Header
  DrawText("LEVEL EDITOR", 20, 20, 20, RAYWHITE);

  if (ed->navPaintMode) {
    static const char *typeNames[]  = {"EMPTY","WALL","LOW COVER","HIGH COVER","BLOCKED","SNIPE","FLANK","FENCE"};
    static Color       typeColors[] = {
      RAYWHITE,
      {180,60,60,255},
      {80,130,255,255},
      {60,200,60,255},
      {110,15,15,255},
      {230,220,60,255},
      {210,60,210,255},
      {60,210,210,255},
    };
    int pt = ed->navPaintType < 8 ? ed->navPaintType : 0;
    DrawText(TextFormat("[ NAV PAINT ]  Type: %s  Brush: %d",
                        typeNames[pt], ed->navBrushSize),
             20, 48, 18, typeColors[pt]);
    DrawText("[LMB] Paint  [RMB] Erase  [Scroll] Brush size  [P] Palette  "
             "[N] Exit  [Ctrl+S] Save  [ESC] Menu",
             20, GetScreenHeight() - 28, 14, WHITE);

    if (ed->navPaletteOpen) {
      int sw = GetScreenWidth(), sh = GetScreenHeight();
      int pw = 320, ph = 390;
      int ppx = sw/2 - pw/2, ppy = sh/2 - ph/2;
      DrawRectangle(ppx, ppy, pw, ph, (Color){20, 20, 35, 230});
      DrawRectangleLines(ppx, ppy, pw, ph, RAYWHITE);
      DrawText("SELECT NAV TYPE", ppx + 60, ppy + 14, 18, RAYWHITE);
      static const char *labels[] = {
        "1: EMPTY",
        "2: WALL",
        "3: LOW COVER",
        "4: HIGH COVER",
        "5: BLOCKED",
        "6: SNIPE",
        "7: FLANK",
        "8: FENCE",
      };
      static Color swatchColors[] = {
        {200,200,200,255},
        {40, 40, 40, 255},
        {40, 100,220,255},
        {40, 180,40, 255},
        {200,40, 40, 255},
        {220,210,40, 255},
        {200,40, 200,255},
        {40, 200,200,255},
      };
      Vector2 mp = GetMousePosition();
      for (int i = 0; i < 8; i++) {
        int sy  = ppy + 50 + i * 40;
        bool sel = (i == pt);
        bool hov = CheckCollisionPointRec(mp, (Rectangle){(float)(ppx+20),(float)sy,(float)(pw-40),34});
        DrawRectangle(ppx+20, sy, pw-40, 34, sel ? (Color){50,80,130,255} : (Color){30,30,50,255});
        DrawRectangleLines(ppx+20, sy, pw-40, 34, (hov||sel) ? SKYBLUE : (Color){55,55,75,255});
        DrawRectangle(ppx+28, sy+5, 22, 22, swatchColors[i]);
        DrawRectangleLines(ppx+28, sy+5, 22, 22, LIGHTGRAY);
        DrawText(labels[i], ppx+58, sy+8, 16, sel ? YELLOW : LIGHTGRAY);
      }
      DrawText("[1-8] Select  [P/ESC] Close", ppx+20, ppy+ph-28, 13, GRAY);
    }
  } else if (ed->transformMode) {
    DrawText("[ TRANSFORM MODE ]", 20, 48, 18, SKYBLUE);

    if (ed->selectedIndex >= 0 && ed->selectedType == 0) {
      EditorPlacedBox *b = &ed->placed[ed->selectedIndex];
      DrawText(TextFormat("BOX  Pos X:%.2f Y:%.2f Z:%.2f",
                          b->position.x, b->position.y, b->position.z),
               20, 74, 16, YELLOW);
      DrawText(TextFormat("Scale X:%.2f Y:%.2f Z:%.2f",
                          b->scale.x, b->scale.y, b->scale.z),
               20, 94, 16, YELLOW);
      DrawText("[Arrows] Move XZ  [PgUp/Dn] Move Y  [Ctrl] x10\n"
               "[Scroll] Scale uniform  [X/Y/Z]+Scroll Scale axis  [Del] Delete",
               20, GetScreenHeight() - 42, 14, WHITE);
    } else if (ed->selectedIndex >= 0 && ed->selectedType == 1) {
      EditorPlacedProp *p = &ed->placedProps[ed->selectedIndex];
      DrawText(TextFormat("PROP  Pos X:%.2f Y:%.2f Z:%.2f",
                          p->position.x, p->position.y, p->position.z),
               20, 74, 16, GREEN);
      DrawText(TextFormat("Scale: %.2f  Yaw: %.1f°", p->scale.x, p->yaw * RAD2DEG),
               20, 94, 16, GREEN);
      DrawText("[Arrows] Move XZ  [PgUp/Dn] Move Y  [Ctrl] x10\n"
               "[R]+Scroll Rotate  [Scroll] Scale  [Del] Delete",
               20, GetScreenHeight() - 42, 14, WHITE);
    } else if (ed->selectedIndex >= 0 && ed->selectedType == 2) {
      EditorPlacedInfoBox *ib = &ed->placedInfoBoxes[ed->selectedIndex];
      const char *trigStr = (ib->triggerCount == 0) ? "INF" : TextFormat("%d", ib->triggerCount);
      DrawText(TextFormat("INFO BOX  Pos X:%.2f Y:%.2f Z:%.2f",
                          ib->position.x, ib->position.y, ib->position.z),
               20, 74, 16, (Color){0, 220, 200, 255});
      DrawText(TextFormat("Size: %.1fm  Dur: %.0fs  Triggers: %s  MarkerY: %.2f",
                          ib->halfExtent * 2.0f, ib->duration, trigStr, ib->markerHeight),
               20, 94, 16, (Color){0, 220, 200, 255});
      DrawText(TextFormat("Msg: %.48s", ib->message), 20, 114, 14, LIGHTGRAY);
      DrawText("[Arrows] Move XZ  [PgUp/Dn] Move Y  [Ctrl] x10\n"
               "[Scroll] Resize  [R]+Scroll MarkerY  [E] Edit attrs  [Del] Delete",
               20, GetScreenHeight() - 42, 14, WHITE);
    } else if (ed->selectedIndex >= 0 && ed->selectedType == 3) {
      DrawText(TextFormat("SPAWN POINT  X:%.2f Y:%.2f Z:%.2f",
                          ed->edSpawnPoint.x, ed->edSpawnPoint.y, ed->edSpawnPoint.z),
               20, 74, 16, GREEN);
      DrawText("[Arrows] Move XZ  [PgUp/Dn] Move Y  [Ctrl] x10  [Del] Remove",
               20, GetScreenHeight() - 28, 14, WHITE);
    } else if (ed->selectedIndex >= 0 && ed->selectedType == 4) {
      EditorPlacedSpawner *sp = &ed->placedSpawners[ed->selectedIndex];
      const char *typeName = (sp->enemyType == 0) ? "GRUNT" : "RANGER";
      Color typeCol = (sp->enemyType == 0) ? RED : BLUE;
      DrawText(TextFormat("SPAWNER [%s]  X:%.2f Y:%.2f Z:%.2f",
                          typeName, sp->position.x, sp->position.y, sp->position.z),
               20, 74, 16, typeCol);
      DrawText("[Arrows] Move XZ  [PgUp/Dn] Move Y  [Ctrl] x10  [Scroll] Toggle type  [Del] Delete",
               20, GetScreenHeight() - 28, 14, WHITE);
    } else if (ed->selectedIndex >= 0 && ed->selectedType == 5) {
      EditorPlacedWallSeg *ws = &ed->placedWallSegs[ed->selectedIndex];
      DrawText(TextFormat("WALL SEG  A:(%.1f,%.1f)  B:(%.1f,%.1f)",
                          ws->ax, ws->az, ws->bx, ws->bz),
               20, 74, 16, ORANGE);
      DrawText(TextFormat("H:%.1fm  R:%.1fm  Y:%.1f..%.1f  Player:%s  Proj:%s",
                          ws->yTop - ws->yBottom, ws->radius, ws->yBottom, ws->yTop,
                          ws->blockPlayer ? "YES" : "NO",
                          ws->blockProjectiles ? "YES" : "NO"),
               20, 94, 16, ORANGE);
      DrawText("[Arrows] Move XZ  [PgUp/Dn] Move Y  [Ctrl] x10  [E] Edit attrs  [Del] Delete",
               20, GetScreenHeight() - 28, 14, WHITE);
    } else {
      DrawText(TextFormat("Boxes: %d  Props: %d  InfoBoxes: %d  Walls: %d  Spawn: %s",
                          ed->placedCount, ed->propCount, ed->infoBoxCount,
                          ed->wallSegCount, ed->edHasSpawnPoint ? "SET" : "NONE"),
               20, 74, 16, LIGHTGRAY);
      DrawText("[LMB] Select  [TAB] Place mode   [Ctrl+S] Save   [ESC] Menu",
               20, GetScreenHeight() - 28, 14, WHITE);
    }
  } else {
    static const char *placeTypeNames[] = {"BOX", "GRUNT SPAWNER", "RANGER SPAWNER", "PROP (VISUAL)", "INFO BOX", "SPAWN POINT", "WALL SEGMENT", "TARGET (STATIC)", "TARGET (PATROL)"};
    static Color placeTypeColors[]      = {LIGHTGRAY, RED, BLUE, GREEN, {0,220,200,255}, {50,255,80,255}, ORANGE, {255,80,80,255}, {255,160,60,255}};
    int pt = (ed->placeType < 9) ? ed->placeType : 0;

    const char *extraInfo = "";
    char extraBuf[80] = "";
    if (ed->placeType == 0)
      snprintf(extraBuf, sizeof(extraBuf), "  Scale: %.1f", ed->boxScale);
    else if (ed->placeType == 3)
      snprintf(extraBuf, sizeof(extraBuf), "  Scale: %.2f  Yaw: %.1f°",
               ed->propPlaceScale, ed->propPlaceYaw * RAD2DEG);
    else if (ed->placeType == 4)
      snprintf(extraBuf, sizeof(extraBuf), "  Size: %.1f  [Scroll] Adjust",
               ed->infoBoxHalfExtent * 2.0f);
    else if (ed->placeType == 6)
      snprintf(extraBuf, sizeof(extraBuf), "  H:%.1f R:%.1f  Step:%d",
               ed->wallSegHeight, ed->wallSegRadius, ed->wallSegStep);
    else if (ed->placeType == 7 || ed->placeType == 8)
      snprintf(extraBuf, sizeof(extraBuf), "  Yaw:%.0f°  [Scroll] Rotate",
               ed->targetDlgYaw * RAD2DEG);
    extraInfo = extraBuf;

    DrawText(TextFormat("[ %s ]  Boxes:%d Spawners:%d Props:%d InfoBoxes:%d Walls:%d%s",
                        placeTypeNames[pt], ed->placedCount,
                        ed->spawnerCount, ed->propCount, ed->infoBoxCount,
                        ed->wallSegCount, extraInfo),
             20, 48, 18, placeTypeColors[pt]);

    // Show active terrain model name (strip directory prefix for brevity)
    const char *terrainFull = gw->terrainModelPath;
    const char *terrainSlash = strrchr(terrainFull, '/');
    const char *terrainName = terrainSlash ? terrainSlash + 1 : terrainFull;
    DrawText(TextFormat("TERRAIN: %s  [T] Change", terrainName),
             20, 72, 14, (Color){120, 200, 255, 220});
    {
      const char *mLabel = (ed->missionType == MISSION_EXPLORATION) ? "EXPLORATION" : "WAVES";
      Color mCol = (ed->missionType == MISSION_EXPLORATION) ? (Color){80, 220, 120, 220}
                                                             : (Color){255, 180, 60, 220};
      DrawText(TextFormat("MISSION: %s  [F] Toggle", mLabel), 20, 90, 14, mCol);
    }
    {
      const char *hpLabel = ed->healthBarFade ? "ON" : "OFF";
      Color hpCol = ed->healthBarFade ? (Color){100, 220, 100, 220} : (Color){200, 80, 80, 220};
      DrawText(TextFormat("HP BAR FADE: %s  [H] Toggle", hpLabel), 20, 108, 14, hpCol);
    }

    if (ed->objectPanelOpen) {
      static const char *labels[]     = {"BOX", "GRUNT SPAWNER", "RANGER SPAWNER", "PROP (VISUAL)", "INFO BOX", "SPAWN POINT", "WALL SEGMENT", "TARGET (STATIC)", "TARGET (PATROL)"};
      static Color       itemColors[] = {LIGHTGRAY, RED, BLUE, GREEN, {0,220,200,255}, {50,255,80,255}, ORANGE, {255,80,80,255}, {255,160,60,255}};
      int sw = GetScreenWidth();
      int px = sw - 185, py = 50;
      int pw = 170, ih = 38;
      Vector2 mp = GetMousePosition();
      DrawRectangle(px - 4, py - 4, pw + 8, 9 * (ih + 4) + 8, (Color){10,18,10,240});
      DrawRectangleLines(px - 4, py - 4, pw + 8, 9 * (ih + 4) + 8, SKYBLUE);
      DrawText("[V] Close", px + 4, py - 20, 14, GRAY);
      for (int i = 0; i < 9; i++) {
        bool sel = (ed->placeType == i);
        bool hov = CheckCollisionPointRec(mp,
            (Rectangle){(float)px, (float)(py + i*(ih+4)), (float)pw, (float)ih});
        DrawRectangle(px, py + i*(ih+4), pw, ih,
                      sel ? (Color){40,80,40,255} : (hov ? (Color){30,55,30,255} : (Color){20,35,20,255}));
        DrawRectangleLines(px, py + i*(ih+4), pw, ih,
                           (sel || hov) ? itemColors[i] : (Color){40,55,40,255});
        DrawText(labels[i], px + 12, py + i*(ih+4) + 11, 16,
                 sel ? YELLOW : itemColors[i]);
      }
    }

    const char *hint = (ed->placeType == 3)
        ? "[LMB] Place  [Scroll] Rotate  [Shift+Scroll] Scale  [Z] Undo  "
          "[V] Objects  [TAB] Transform  [N] Nav Paint  [Ctrl+S] Save  [ESC] Menu"
        : (ed->placeType == 4)
        ? "[LMB] Place  [Scroll] Size  [Z] Undo  "
          "[V] Objects  [TAB] Transform  [N] Nav Paint  [Ctrl+S] Save  [ESC] Menu"
        : (ed->placeType == 6 && ed->wallSegStep == 0)
        ? "[LMB] Set Point A  [Z] Undo  [V] Objects  [Ctrl+S] Save  [ESC] Cancel"
        : (ed->placeType == 6 && ed->wallSegStep == 1)
        ? "[LMB] Set Point B  [ESC] Cancel"
        : (ed->placeType == 7)
        ? "[LMB] Place Static Target  [Scroll] Rotate  [V] Objects  [Ctrl+S] Save  [ESC] Menu"
        : (ed->placeType == 8 && ed->targetPatrolStep == 0)
        ? "[LMB] Set Point A  [Scroll] Rotate  [V] Objects  [Ctrl+S] Save  [ESC] Cancel"
        : (ed->placeType == 8 && ed->targetPatrolStep == 1)
        ? "[LMB] Set Point B  [Scroll] Rotate  [ESC] Cancel"
        : (ed->placeType == 0)
        ? "[LMB] Array  [Scroll] Resize  [Z] Undo  "
          "[V] Objects  [TAB] Transform  [N] Nav Paint  [Ctrl+S] Save  [ESC] Menu"
        : "[LMB] Place  [Scroll] Resize  [Z] Undo  "
          "[V] Objects  [TAB] Transform  [N] Nav Paint  [Ctrl+S] Save  [ESC] Menu";
    DrawText(hint, 20, GetScreenHeight() - 28, 14, WHITE);
  }

  // --- Pause overlay ---
  if (ed->isPaused) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 180});

    if (ed->isSelectingLevel) {
      DrawText("OPEN LEVEL", sw / 2 - 90, 80, 30, RAYWHITE);
      if (ed->edLevelCount == 0) {
        DrawText("No levels found in assets/levels/",
                 sw/2 - 220, sh/2 - 10, 18, GRAY);
      } else {
        int itemW = 400, itemH = 50;
        int lsx = sw / 2 - itemW / 2, lsy = 150;
        Vector2 mp = GetMousePosition();
        for (int i = 0; i < ed->edLevelCount; i++) {
          int iy = lsy + i * (itemH + 8);
          bool sel = (i == ed->edLevelSelected);
          bool hov = CheckCollisionPointRec(mp, (Rectangle){(float)lsx, (float)iy, (float)itemW, (float)itemH});
          DrawRectangle(lsx, iy, itemW, itemH, sel ? (Color){50,80,180,255} : (Color){30,30,45,255});
          DrawRectangleLines(lsx, iy, itemW, itemH, (hov || sel) ? SKYBLUE : (Color){60,60,75,255});
          DrawText(ed->edLevelNames[i], lsx + 16, iy + 14, 20, sel ? YELLOW : LIGHTGRAY);
        }
      }
      DrawText("[UP/DOWN/SCROLL] Navigate   [ENTER/LMB] Load   [ESC] Back",
               sw/2 - 230, sh - 36, 14, GRAY);
    } else {
      DrawText("PAUSED", sw/2 - 55, sh/2 - 140, 36, RAYWHITE);
      Vector2 mp = GetMousePosition();
      int bx = sw / 2 - 120;
      // Resume
      {
        Rectangle r = {(float)bx, (float)(sh/2 - 90), 240, 50};
        bool hov = CheckCollisionPointRec(mp, r);
        DrawRectangleRec(r, hov ? (Color){60,90,60,255} : (Color){35,50,35,255});
        DrawRectangleLinesEx(r, 2, hov ? GREEN : (Color){55,75,55,255});
        DrawText("RESUME", bx + 72, sh/2 - 76, 20, hov ? YELLOW : RAYWHITE);
      }
      // Open Level
      {
        Rectangle r = {(float)bx, (float)(sh/2 - 10), 240, 50};
        bool hov = CheckCollisionPointRec(mp, r);
        DrawRectangleRec(r, hov ? (Color){40,60,90,255} : (Color){30,40,55,255});
        DrawRectangleLinesEx(r, 2, hov ? SKYBLUE : (Color){50,60,80,255});
        DrawText("OPEN LEVEL", bx + 44, sh/2 + 4, 20, hov ? YELLOW : RAYWHITE);
      }
      // Main Menu
      {
        Rectangle r = {(float)bx, (float)(sh/2 + 70), 240, 50};
        bool hov = CheckCollisionPointRec(mp, r);
        DrawRectangleRec(r, hov ? (Color){90,35,35,255} : (Color){55,25,25,255});
        DrawRectangleLinesEx(r, 2, hov ? RED : (Color){80,50,50,255});
        DrawText("MAIN MENU", bx + 48, sh/2 + 84, 20, hov ? YELLOW : RAYWHITE);
      }
      DrawText("[ESC] Resume", sw/2 - 60, sh - 36, 14, GRAY);
    }
  }

  // --- Model / terrain picker overlay ---
  if (ed->modelPickerOpen || ed->terrainPickerOpen) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 160});

    int vis    = (ed->edModelCount < PICKER_VIS) ? ed->edModelCount : PICKER_VIS;
    int panelH = PICKER_HDR + vis * (PICKER_IH + PICKER_GAP) + PICKER_FTR;
    int pw     = 360;
    int px     = sw / 2 - pw / 2;
    int py     = sh / 2 - panelH / 2;

    DrawRectangle(px - 4, py - 4, pw + 8, panelH + 8, (Color){15, 20, 15, 240});
    DrawRectangleLines(px - 4, py - 4, pw + 8, panelH + 8, SKYBLUE);
    const char *title = ed->terrainPickerOpen ? "SELECT TERRAIN MODEL" : "SELECT PROP MODEL";
    DrawText(title, px + 10, py + 10, 18, RAYWHITE);

    const char *currentPath = ed->terrainPickerOpen
        ? gw->terrainModelPath : ed->propPlaceModel;
    if (ed->transformMode && ed->selectedIndex >= 0 && ed->selectedType == 1 && !ed->terrainPickerOpen)
      currentPath = ed->placedProps[ed->selectedIndex].modelPath;

    Vector2 mp = GetMousePosition();
    for (int j = 0; j < vis; j++) {
      int i  = ed->modelPickerScroll + j;
      if (i >= ed->edModelCount) break;
      int iy = py + PICKER_HDR + j * (PICKER_IH + PICKER_GAP);
      bool sel = (strcmp(ed->edModelPaths[i], currentPath) == 0);
      bool hov = CheckCollisionPointRec(mp,
          (Rectangle){(float)px, (float)iy, (float)pw, (float)PICKER_IH});
      DrawRectangle(px, iy, pw, PICKER_IH,
                    sel ? (Color){30,70,30,255} : (hov ? (Color){25,45,25,255} : (Color){18,28,18,255}));
      DrawRectangleLines(px, iy, pw, PICKER_IH,
                         (sel||hov) ? SKYBLUE : (Color){40,55,40,255});
      DrawText(ed->edModelNames[i], px + 10, iy + 9, 16,
               sel ? YELLOW : LIGHTGRAY);
    }

    // Scrollbar when list overflows
    if (ed->edModelCount > PICKER_VIS) {
      int maxSc   = ed->edModelCount - PICKER_VIS;
      int trackH  = vis * (PICKER_IH + PICKER_GAP);
      int trackY  = py + PICKER_HDR;
      int thumbH  = trackH * PICKER_VIS / ed->edModelCount;
      if (thumbH < 12) thumbH = 12;
      int thumbY  = trackY + (trackH - thumbH) * ed->modelPickerScroll / maxSc;
      DrawRectangle(px + pw + 6, trackY, 6, trackH, (Color){30,40,30,255});
      DrawRectangle(px + pw + 6, thumbY, 6, thumbH, SKYBLUE);
    }

    DrawText("[Scroll] Scroll  [LMB] Select  [M/T/ESC] Close",
             px + 10, py + panelH - PICKER_FTR + 6, 12, GRAY);
  }

  // --- Info box text input dialog ---
  if (ed->infoBoxEditOpen) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int pw = 520, ph = 380;
    int px = sw / 2 - pw / 2, py = sh / 2 - ph / 2;
    DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 140});
    DrawRectangle(px, py, pw, ph, (Color){8, 14, 28, 245});
    DrawRectangleLines(px, py, pw, ph, (Color){0, 210, 190, 255});
    // Corner accents
    DrawRectangle(px, py, 12, 2, (Color){0, 255, 230, 255});
    DrawRectangle(px, py, 2, 12, (Color){0, 255, 230, 255});
    DrawRectangle(px + pw - 12, py, 12, 2, (Color){0, 255, 230, 255});
    DrawRectangle(px + pw - 2, py, 2, 12, (Color){0, 255, 230, 255});
    DrawRectangle(px, py + ph - 2, 12, 2, (Color){0, 255, 230, 255});
    DrawRectangle(px, py + ph - 12, 2, 12, (Color){0, 255, 230, 255});
    DrawRectangle(px + pw - 12, py + ph - 2, 12, 2, (Color){0, 255, 230, 255});
    DrawRectangle(px + pw - 2, py + ph - 12, 2, 12, (Color){0, 255, 230, 255});

    DrawText("INFO BOX EDITOR", px + 16, py + 14, 18, (Color){0, 230, 210, 255});
    DrawLine(px + 12, py + 38, px + pw - 12, py + 38, (Color){0, 80, 70, 200});

    // Message field — multi-line text box (80px tall)
    DrawText("MESSAGE:", px + 16, py + 50, 14, LIGHTGRAY);
    DrawRectangle(px + 16, py + 68, pw - 32, 80, (Color){4, 10, 22, 220});
    DrawRectangleLines(px + 16, py + 68, pw - 32, 80, (Color){0, 150, 140, 255});
    {
      // Split on \n and render lines, cursor on last char
      const char *buf = ed->infoBoxTextBuf;
      int lineY = py + 72;
      int lineH = 18;
      int col = (((int)(GetTime() * 4)) % 2 == 0);
      // walk through lines
      const char *lineStart = buf;
      bool drewCursor = false;
      while (*lineStart) {
        const char *nl = lineStart;
        while (*nl && *nl != '\n') nl++;
        int len = (int)(nl - lineStart);
        char lineText[256];
        if (len > 254) len = 254;
        memcpy(lineText, lineStart, len);
        lineText[len] = '\0';
        // Is this the last segment (cursor here)?
        bool isLast = (*nl == '\0');
        if (isLast && col) {
          DrawText(TextFormat("%s_", lineText), px + 22, lineY, 14, (Color){100, 255, 170, 255});
          drewCursor = true;
        } else {
          DrawText(lineText, px + 22, lineY, 14, (Color){100, 255, 170, 255});
        }
        lineY += lineH;
        if (*nl == '\0') break;
        lineStart = nl + 1;
        if (lineY > py + 68 + 80 - lineH) break; // clip to box
      }
      if (!drewCursor && col)
        DrawText("_", px + 22, lineY, 14, (Color){100, 255, 170, 255});
    }

    // Duration row (+46 from old)
    DrawText("DURATION:", px + 16, py + 166, 14, LIGHTGRAY);
    DrawText(TextFormat("%.0f s", ed->infoBoxDuration), px + 110, py + 164, 18, YELLOW);
    {
      Vector2 mouse      = GetMousePosition();
      Rectangle btnMinus = {(float)(px + 170), (float)(py + 161), 44, 24};
      Rectangle btnPlus  = {(float)(px + 224), (float)(py + 161), 44, 24};
      bool hovM = CheckCollisionPointRec(mouse, btnMinus);
      bool hovP = CheckCollisionPointRec(mouse, btnPlus);
      DrawRectangleRec(btnMinus, hovM ? (Color){0,160,140,255} : (Color){0,50,45,255});
      DrawRectangleLinesEx(btnMinus, 1.0f, (Color){0,210,190,255});
      DrawText("-5", px + 184, py + 165, 13, hovM ? WHITE : (Color){0,230,210,255});
      DrawRectangleRec(btnPlus,  hovP ? (Color){0,160,140,255} : (Color){0,50,45,255});
      DrawRectangleLinesEx(btnPlus,  1.0f, (Color){0,210,190,255});
      DrawText("+5", px + 236, py + 165, 13, hovP ? WHITE : (Color){0,230,210,255});
    }

    // Trigger size row
    DrawText("TRIGGER SIZE:", px + 16, py + 198, 14, LIGHTGRAY);
    DrawText(TextFormat("%.1f m", ed->infoBoxHalfExtent * 2.0f), px + 130, py + 196, 18, YELLOW);
    DrawText("scroll to adjust", px + 200, py + 200, 13, GRAY);

    // Trigger count row
    DrawText("TRIGGERS:", px + 16, py + 238, 14, LIGHTGRAY);
    const char *trigVal = (ed->infoBoxMaxTriggers == 0)
        ? "INF" : TextFormat("%d", ed->infoBoxMaxTriggers);
    DrawText(trigVal, px + 110, py + 236, 18, YELLOW);
    {
      Vector2 mouse      = GetMousePosition();
      Rectangle tMinus   = {(float)(px + 170), (float)(py + 233), 30, 24};
      Rectangle tPlus    = {(float)(px + 210), (float)(py + 233), 30, 24};
      bool hovM = CheckCollisionPointRec(mouse, tMinus);
      bool hovP = CheckCollisionPointRec(mouse, tPlus);
      DrawRectangleRec(tMinus, hovM ? (Color){0,160,140,255} : (Color){0,50,45,255});
      DrawRectangleLinesEx(tMinus, 1.0f, (Color){0,210,190,255});
      DrawText("-",  px + 181, py + 237, 13, hovM ? WHITE : (Color){0,230,210,255});
      DrawRectangleRec(tPlus,  hovP ? (Color){0,160,140,255} : (Color){0,50,45,255});
      DrawRectangleLinesEx(tPlus,  1.0f, (Color){0,210,190,255});
      DrawText("+",  px + 221, py + 237, 13, hovP ? WHITE : (Color){0,230,210,255});
    }
    DrawText("0=INF", px + 252, py + 240, 12, GRAY);

    // Marker height row
    DrawText("MARKER Y:", px + 16, py + 278, 14, LIGHTGRAY);
    DrawText(TextFormat("%.1f", ed->infoBoxMarkerHeight), px + 110, py + 276, 18, YELLOW);
    {
      Vector2 mouse     = GetMousePosition();
      Rectangle mhMinus = {(float)(px + 170), (float)(py + 273), 44, 24};
      Rectangle mhPlus  = {(float)(px + 224), (float)(py + 273), 44, 24};
      bool hovM = CheckCollisionPointRec(mouse, mhMinus);
      bool hovP = CheckCollisionPointRec(mouse, mhPlus);
      DrawRectangleRec(mhMinus, hovM ? (Color){0,160,140,255} : (Color){0,50,45,255});
      DrawRectangleLinesEx(mhMinus, 1.0f, (Color){0,210,190,255});
      DrawText("-0.5", px + 175, py + 277, 13, hovM ? WHITE : (Color){0,230,210,255});
      DrawRectangleRec(mhPlus,  hovP ? (Color){0,160,140,255} : (Color){0,50,45,255});
      DrawRectangleLinesEx(mhPlus,  1.0f, (Color){0,210,190,255});
      DrawText("+0.5", px + 228, py + 277, 13, hovP ? WHITE : (Color){0,230,210,255});
    }

    // Font size row
    DrawText("FONT SIZE:", px + 16, py + 318, 14, LIGHTGRAY);
    const char *fsVal = (ed->infoBoxFontSize == 0) ? "AUTO" : TextFormat("%d", ed->infoBoxFontSize);
    DrawText(fsVal, px + 110, py + 316, 18, YELLOW);
    {
      Vector2 mouse     = GetMousePosition();
      Rectangle fsMinus = {(float)(px + 170), (float)(py + 313), 44, 24};
      Rectangle fsPlus  = {(float)(px + 224), (float)(py + 313), 44, 24};
      bool hovM = CheckCollisionPointRec(mouse, fsMinus);
      bool hovP = CheckCollisionPointRec(mouse, fsPlus);
      DrawRectangleRec(fsMinus, hovM ? (Color){0,160,140,255} : (Color){0,50,45,255});
      DrawRectangleLinesEx(fsMinus, 1.0f, (Color){0,210,190,255});
      DrawText("-2", px + 184, py + 317, 13, hovM ? WHITE : (Color){0,230,210,255});
      DrawRectangleRec(fsPlus,  hovP ? (Color){0,160,140,255} : (Color){0,50,45,255});
      DrawRectangleLinesEx(fsPlus,  1.0f, (Color){0,210,190,255});
      DrawText("+2", px + 236, py + 317, 13, hovP ? WHITE : (Color){0,230,210,255});
    }
    DrawText("0=AUTO", px + 280, py + 320, 12, GRAY);

    DrawText("[Ctrl+ENTER] Confirm", px + 16, py + ph - 36, 14, (Color){80, 230, 120, 255});
    DrawText("[ESC] Cancel", px + pw - 130, py + ph - 36, 14, (Color){230, 80, 80, 255});
  }

  // --- Wall segment param dialog ---
  if (ed->wallSegDialogOpen) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int pw = 380, ph = 260;
    int px = sw / 2 - pw / 2, py = sh / 2 - ph / 2;
    DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 140});
    DrawRectangle(px, py, pw, ph, (Color){8, 14, 28, 245});
    DrawRectangleLines(px, py, pw, ph, ORANGE);
    DrawRectangle(px, py, 12, 2, (Color){255, 200, 80, 255});
    DrawRectangle(px, py, 2, 12, (Color){255, 200, 80, 255});
    DrawRectangle(px + pw - 12, py, 12, 2, (Color){255, 200, 80, 255});
    DrawRectangle(px + pw - 2,  py, 2, 12, (Color){255, 200, 80, 255});
    DrawRectangle(px, py + ph - 2,  12, 2, (Color){255, 200, 80, 255});
    DrawRectangle(px, py + ph - 12,  2, 12, (Color){255, 200, 80, 255});
    DrawRectangle(px + pw - 12, py + ph - 2, 12, 2, (Color){255, 200, 80, 255});
    DrawRectangle(px + pw - 2,  py + ph - 12, 2, 12, (Color){255, 200, 80, 255});

    DrawText("WALL SEGMENT", px + 16, py + 14, 18, ORANGE);
    DrawLine(px + 12, py + 38, px + pw - 12, py + 38, (Color){80, 55, 0, 200});

    Vector2 mouse = GetMousePosition();

    // Height row
    DrawText("HEIGHT:", px + 16, py + 57, 14, LIGHTGRAY);
    DrawText(TextFormat("%.1f m", ed->wallSegHeight), px + 110, py + 54, 18, YELLOW);
    {
      Rectangle btnM = {(float)(px + 170), (float)(py + 55), 44, 24};
      Rectangle btnP = {(float)(px + 224), (float)(py + 55), 44, 24};
      bool hovM = CheckCollisionPointRec(mouse, btnM);
      bool hovP = CheckCollisionPointRec(mouse, btnP);
      DrawRectangleRec(btnM, hovM ? (Color){160,100,0,255} : (Color){50,32,0,255});
      DrawRectangleLinesEx(btnM, 1.0f, ORANGE);
      DrawText("-0.5", px + 174, py + 59, 13, hovM ? WHITE : ORANGE);
      DrawRectangleRec(btnP, hovP ? (Color){160,100,0,255} : (Color){50,32,0,255});
      DrawRectangleLinesEx(btnP, 1.0f, ORANGE);
      DrawText("+0.5", px + 228, py + 59, 13, hovP ? WHITE : ORANGE);
    }

    // Radius row
    DrawText("RADIUS:", px + 16, py + 107, 14, LIGHTGRAY);
    DrawText(TextFormat("%.1f m", ed->wallSegRadius), px + 110, py + 104, 18, YELLOW);
    {
      Rectangle btnM = {(float)(px + 170), (float)(py + 105), 44, 24};
      Rectangle btnP = {(float)(px + 224), (float)(py + 105), 44, 24};
      bool hovM = CheckCollisionPointRec(mouse, btnM);
      bool hovP = CheckCollisionPointRec(mouse, btnP);
      DrawRectangleRec(btnM, hovM ? (Color){160,100,0,255} : (Color){50,32,0,255});
      DrawRectangleLinesEx(btnM, 1.0f, ORANGE);
      DrawText("-0.1", px + 174, py + 109, 13, hovM ? WHITE : ORANGE);
      DrawRectangleRec(btnP, hovP ? (Color){160,100,0,255} : (Color){50,32,0,255});
      DrawRectangleLinesEx(btnP, 1.0f, ORANGE);
      DrawText("+0.1", px + 228, py + 109, 13, hovP ? WHITE : ORANGE);
    }

    // Block player row
    DrawText("BLOCK PLAYER:", px + 16, py + 157, 14, LIGHTGRAY);
    {
      Rectangle btn = {(float)(px + 170), (float)(py + 155), 80, 24};
      bool on = ed->wallSegBlockPlayer;
      bool hov = CheckCollisionPointRec(GetMousePosition(), btn);
      DrawRectangleRec(btn, on ? (Color){0,100,0,255} : (Color){100,0,0,255});
      if (hov) DrawRectangleRec(btn, (Color){255,255,255,30});
      DrawRectangleLinesEx(btn, 1.0f, ORANGE);
      DrawText(on ? "YES" : "NO", px + 195, py + 159, 13, WHITE);
    }

    // Block projectiles row
    DrawText("BLOCK PROJ:", px + 16, py + 197, 14, LIGHTGRAY);
    {
      Rectangle btn = {(float)(px + 170), (float)(py + 195), 80, 24};
      bool on = ed->wallSegBlockProjectiles;
      bool hov = CheckCollisionPointRec(GetMousePosition(), btn);
      DrawRectangleRec(btn, on ? (Color){0,100,0,255} : (Color){100,0,0,255});
      if (hov) DrawRectangleRec(btn, (Color){255,255,255,30});
      DrawRectangleLinesEx(btn, 1.0f, ORANGE);
      DrawText(on ? "YES" : "NO", px + 195, py + 199, 13, WHITE);
    }

    DrawText("[ENTER] Confirm", px + 16, py + ph - 30, 14, (Color){80, 230, 120, 255});
    DrawText("[ESC] Cancel", px + pw - 130, py + ph - 30, 14, (Color){230, 80, 80, 255});
  }

  // --- Box array dialog ---
  if (ed->arrayDialogOpen) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int pw = 420, ph = 290;
    int px = sw / 2 - pw / 2, py = sh / 2 - ph / 2;

    DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 140});
    DrawRectangle(px, py, pw, ph, (Color){8, 16, 30, 245});
    DrawRectangleLines(px, py, pw, ph, (Color){80, 190, 255, 255});
    // corner accents
    Color ac = (Color){80, 210, 255, 255};
    DrawRectangle(px,      py,      12, 2, ac); DrawRectangle(px,      py,      2, 12, ac);
    DrawRectangle(px+pw-12,py,      12, 2, ac); DrawRectangle(px+pw-2, py,      2, 12, ac);
    DrawRectangle(px,      py+ph-2, 12, 2, ac); DrawRectangle(px,      py+ph-12,2, 12, ac);
    DrawRectangle(px+pw-12,py+ph-2, 12, 2, ac); DrawRectangle(px+pw-2, py+ph-12,2, 12, ac);

    DrawText("BOX ARRAY", px + 16, py + 12, 18, (Color){80, 210, 255, 255});
    DrawLine(px + 12, py + 36, px + pw - 12, py + 36, (Color){30, 70, 110, 200});

    Vector2 mouse = GetMousePosition();

    // Count row
    DrawText("COUNT:", px + 16, py + 55, 14, LIGHTGRAY);
    DrawText(TextFormat("%d", ed->arrayCount), px + 120, py + 52, 18, YELLOW);
    {
      Rectangle btnM = {(float)(px+165), (float)(py+50), 44, 24};
      Rectangle btnP = {(float)(px+215), (float)(py+50), 44, 24};
      bool hovM = CheckCollisionPointRec(mouse, btnM);
      bool hovP = CheckCollisionPointRec(mouse, btnP);
      DrawRectangleRec(btnM, hovM ? (Color){0,100,160,255} : (Color){0,32,55,255});
      DrawRectangleLinesEx(btnM, 1.0f, ac);
      DrawText("-1", px+177, py+54, 13, hovM ? WHITE : ac);
      DrawRectangleRec(btnP, hovP ? (Color){0,100,160,255} : (Color){0,32,55,255});
      DrawRectangleLinesEx(btnP, 1.0f, ac);
      DrawText("+1", px+225, py+54, 13, hovP ? WHITE : ac);
    }

    // Spacing row
    DrawText("SPACING:", px + 16, py + 95, 14, LIGHTGRAY);
    DrawText(TextFormat("%.1f m", ed->arraySpacing), px + 120, py + 92, 18, YELLOW);
    {
      Rectangle btnM = {(float)(px+165), (float)(py+90), 44, 24};
      Rectangle btnP = {(float)(px+215), (float)(py+90), 44, 24};
      bool hovM = CheckCollisionPointRec(mouse, btnM);
      bool hovP = CheckCollisionPointRec(mouse, btnP);
      DrawRectangleRec(btnM, hovM ? (Color){0,100,160,255} : (Color){0,32,55,255});
      DrawRectangleLinesEx(btnM, 1.0f, ac);
      DrawText("-0.5", px+169, py+94, 13, hovM ? WHITE : ac);
      DrawRectangleRec(btnP, hovP ? (Color){0,100,160,255} : (Color){0,32,55,255});
      DrawRectangleLinesEx(btnP, 1.0f, ac);
      DrawText("+0.5", px+219, py+94, 13, hovP ? WHITE : ac);
    }

    // Direction row
    DrawText("DIRECTION:", px + 16, py + 136, 14, LIGHTGRAY);
    static const char *dirLabels[6] = {"+X", "-X", "+Z", "-Z", "+Y", "-Y"};
    Rectangle dirBtns[6];
    dirBtns[0] = (Rectangle){(float)(px+16),  (float)(py+155), 80, 28};
    dirBtns[1] = (Rectangle){(float)(px+104), (float)(py+155), 80, 28};
    dirBtns[2] = (Rectangle){(float)(px+192), (float)(py+155), 80, 28};
    dirBtns[3] = (Rectangle){(float)(px+280), (float)(py+155), 80, 28};
    dirBtns[4] = (Rectangle){(float)(px+16),  (float)(py+193), 80, 28};
    dirBtns[5] = (Rectangle){(float)(px+104), (float)(py+193), 80, 28};
    for (int d = 0; d < 6; d++) {
      bool sel = (ed->arrayDir == d);
      bool hov = CheckCollisionPointRec(mouse, dirBtns[d]);
      Color bg  = sel  ? (Color){0,100,160,255}
                : hov  ? (Color){0,60,100,255}
                       : (Color){0,28,50,255};
      DrawRectangleRec(dirBtns[d], bg);
      DrawRectangleLinesEx(dirBtns[d], 1.0f, sel ? WHITE : ac);
      int tw = MeasureText(dirLabels[d], 16);
      DrawText(dirLabels[d],
               (int)(dirBtns[d].x + dirBtns[d].width/2 - tw/2),
               (int)(dirBtns[d].y + 6), 16,
               sel ? WHITE : ac);
    }

    DrawText(TextFormat("[ENTER] Place %d boxes", ed->arrayCount),
             px + 16, py + ph - 30, 14, (Color){80, 230, 120, 255});
    DrawText("[ESC] Cancel", px + pw - 130, py + ph - 30, 14, (Color){230, 80, 80, 255});
  }

  // --- Target config dialog ---
  if (ed->targetDialogOpen) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int ph = ed->targetIsPatrol ? 285 : 250;
    int pw = 380, px = sw / 2 - pw / 2, py = sh / 2 - ph / 2;
    Vector2 mouse = GetMousePosition();
    Color ac = (Color){255, 100, 80, 255};
    Color bgc = (Color){30, 12, 10, 240};

    DrawRectangle(px - 4, py - 4, pw + 8, ph + 8, bgc);
    DrawRectangleLines(px - 4, py - 4, pw + 8, ph + 8, ac);

    const char *title = ed->targetIsPatrol ? "PATROL TARGET" : "STATIC TARGET";
    DrawText(title, px + 16, py + 14, 18, ac);

    int speedY = ed->targetIsPatrol ? 124 : -1;
    int hdropY = ed->targetIsPatrol ? 160 : 124;
    int cdropY = ed->targetIsPatrol ? 196 : 160;

    // HP row
    bool hpSel = (ed->targetDlgField == 0);
    DrawText("HEALTH:", px + 16, py + 56, 14, hpSel ? YELLOW : LIGHTGRAY);
    DrawText(TextFormat("%.0f", ed->targetDlgHealth), px + 110, py + 54, 18, hpSel ? YELLOW : WHITE);
    { Rectangle hpMinus = {(float)(px+180),(float)(py+52),40,22}; Rectangle hpPlus = {(float)(px+226),(float)(py+52),40,22};
      bool hovM = CheckCollisionPointRec(mouse, hpMinus), hovP = CheckCollisionPointRec(mouse, hpPlus);
      DrawRectangleRec(hpMinus, hovM ? (Color){180,50,50,255} : (Color){60,20,20,255}); DrawRectangleLinesEx(hpMinus, 1, ac);
      DrawText("-10", px+186, py+56, 13, hovM ? WHITE : (Color){255,160,160,255});
      DrawRectangleRec(hpPlus,  hovP ? (Color){180,50,50,255} : (Color){60,20,20,255}); DrawRectangleLinesEx(hpPlus, 1, ac);
      DrawText("+10", px+231, py+56, 13, hovP ? WHITE : (Color){255,160,160,255}); }

    // Shield row
    bool shSel = (ed->targetDlgField == 1);
    DrawText("SHIELD:", px + 16, py + 92, 14, shSel ? YELLOW : LIGHTGRAY);
    DrawText(TextFormat("%.0f", ed->targetDlgShield), px + 110, py + 90, 18, shSel ? YELLOW : WHITE);
    { Rectangle shMinus = {(float)(px+180),(float)(py+88),40,22}; Rectangle shPlus = {(float)(px+226),(float)(py+88),40,22};
      bool hovM = CheckCollisionPointRec(mouse, shMinus), hovP = CheckCollisionPointRec(mouse, shPlus);
      DrawRectangleRec(shMinus, hovM ? (Color){180,50,50,255} : (Color){60,20,20,255}); DrawRectangleLinesEx(shMinus, 1, ac);
      DrawText("-10", px+186, py+92, 13, hovM ? WHITE : (Color){255,160,160,255});
      DrawRectangleRec(shPlus,  hovP ? (Color){180,50,50,255} : (Color){60,20,20,255}); DrawRectangleLinesEx(shPlus, 1, ac);
      DrawText("+10", px+231, py+92, 13, hovP ? WHITE : (Color){255,160,160,255}); }

    // Speed row (patrol only)
    if (ed->targetIsPatrol && speedY >= 0) {
      bool spSel = (ed->targetDlgField == 2);
      DrawText("SPEED:", px + 16, py + speedY + 4, 14, spSel ? YELLOW : LIGHTGRAY);
      DrawText(TextFormat("%.1f u/s", ed->targetDlgSpeed), px + 110, py + speedY + 2, 18, spSel ? YELLOW : WHITE);
      Rectangle spMinus = {(float)(px+180), (float)(py+speedY), 40, 22};
      Rectangle spPlus  = {(float)(px+226), (float)(py+speedY), 40, 22};
      bool hovM = CheckCollisionPointRec(mouse, spMinus), hovP = CheckCollisionPointRec(mouse, spPlus);
      DrawRectangleRec(spMinus, hovM ? (Color){180,50,50,255} : (Color){60,20,20,255}); DrawRectangleLinesEx(spMinus, 1, ac);
      DrawText("-0.5", px+183, py+speedY+4, 13, hovM ? WHITE : (Color){255,160,160,255});
      DrawRectangleRec(spPlus,  hovP ? (Color){180,50,50,255} : (Color){60,20,20,255}); DrawRectangleLinesEx(spPlus, 1, ac);
      DrawText("+0.5", px+229, py+speedY+4, 13, hovP ? WHITE : (Color){255,160,160,255});
    }

    // Health drop row
    int hdField = ed->targetIsPatrol ? 3 : 2;
    bool hdSel = (ed->targetDlgField == hdField);
    DrawText("HP DROPS:", px + 16, py + hdropY + 4, 14, hdSel ? YELLOW : LIGHTGRAY);
    DrawText(TextFormat("%d", ed->targetDlgHealthCount), px + 120, py + hdropY + 2, 18, hdSel ? YELLOW : WHITE);
    { Rectangle hdMinus = {(float)(px+180),(float)(py+hdropY),40,22}; Rectangle hdPlus = {(float)(px+226),(float)(py+hdropY),40,22};
      bool hovM = CheckCollisionPointRec(mouse, hdMinus), hovP = CheckCollisionPointRec(mouse, hdPlus);
      DrawRectangleRec(hdMinus, hovM ? (Color){180,50,50,255} : (Color){60,20,20,255}); DrawRectangleLinesEx(hdMinus, 1, ac);
      DrawText("-1", px+190, py+hdropY+4, 13, hovM ? WHITE : (Color){255,160,160,255});
      DrawRectangleRec(hdPlus,  hovP ? (Color){180,50,50,255} : (Color){60,20,20,255}); DrawRectangleLinesEx(hdPlus, 1, ac);
      DrawText("+1", px+236, py+hdropY+4, 13, hovP ? WHITE : (Color){255,160,160,255}); }

    // Coolant drop row
    int cdField = ed->targetIsPatrol ? 4 : 3;
    bool cdSel = (ed->targetDlgField == cdField);
    DrawText("CL DROPS:", px + 16, py + cdropY + 4, 14, cdSel ? YELLOW : LIGHTGRAY);
    DrawText(TextFormat("%d", ed->targetDlgCoolantCount), px + 120, py + cdropY + 2, 18, cdSel ? YELLOW : WHITE);
    { Rectangle cdMinus = {(float)(px+180),(float)(py+cdropY),40,22}; Rectangle cdPlus = {(float)(px+226),(float)(py+cdropY),40, 22};
      bool hovM = CheckCollisionPointRec(mouse, cdMinus), hovP = CheckCollisionPointRec(mouse, cdPlus);
      DrawRectangleRec(cdMinus, hovM ? (Color){180,50,50,255} : (Color){60,20,20,255}); DrawRectangleLinesEx(cdMinus, 1, ac);
      DrawText("-1", px+190, py+cdropY+4, 13, hovM ? WHITE : (Color){255,160,160,255});
      DrawRectangleRec(cdPlus,  hovP ? (Color){180,50,50,255} : (Color){60,20,20,255}); DrawRectangleLinesEx(cdPlus, 1, ac);
      DrawText("+1", px+236, py+cdropY+4, 13, hovP ? WHITE : (Color){255,160,160,255}); }

    int confirmY = py + ph - 30;
    DrawText("[ENTER] Place", px + 16, confirmY, 14, (Color){80, 230, 120, 255});
    DrawText("[ESC] Cancel",  px + pw - 130, confirmY, 14, (Color){230, 80, 80, 255});
    DrawText("[TAB] Next field", px + 16, confirmY - 20, 12, GRAY);
  }

  // --- Wave editor panel ---
  if (ed->waveEditorOpen) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    int pw = 500, rowH = 28, hdr = 54;
    int ph = hdr + ed->edWaveCount * rowH + 38;
    if (ph < 140) ph = 140;
    int px = sw / 2 - pw / 2, py = sh / 2 - ph / 2;
    Color ac = (Color){0, 200, 180, 255};

    DrawRectangle(px, py, pw, ph, (Color){8, 14, 24, 245});
    DrawRectangleLines(px, py, pw, ph, ac);
    DrawRectangle(px,        py,        12, 2, ac); DrawRectangle(px,        py,        2, 12, ac);
    DrawRectangle(px+pw-12,  py,        12, 2, ac); DrawRectangle(px+pw-2,   py,        2, 12, ac);
    DrawRectangle(px,        py+ph-2,   12, 2, ac); DrawRectangle(px,        py+ph-12,  2, 12, ac);
    DrawRectangle(px+pw-12,  py+ph-2,   12, 2, ac); DrawRectangle(px+pw-2,   py+ph-12,  2, 12, ac);

    DrawText("WAVE COMPOSITION", px + 14, py + 10, 16, ac);
    DrawLine(px + 10, py + 32, px + pw - 10, py + 32, (Color){0, 80, 70, 200});

    DrawText("#",       px + 14,  py + 36, 12, GRAY);
    DrawText("GRUNTS",  px + 60,  py + 36, 12, (Color){255, 140, 100, 255});
    DrawText("RANGERS", px + 154, py + 36, 12, (Color){100, 160, 255, 255});
    DrawText("MELEE",   px + 248, py + 36, 12, (Color){255, 200,  80, 255});
    DrawText("DRONES",  px + 342, py + 36, 12, (Color){180, 100, 255, 255});

    static Color s_waveColColors[4] = {
        {255, 140, 100, 255}, {100, 160, 255, 255}, {255, 200, 80, 255}, {180, 100, 255, 255}
    };
    Vector2 mp = GetMousePosition();

    for (int wi = 0; wi < ed->edWaveCount; wi++) {
      int rowY = py + hdr + wi * rowH + 3;
      if (CheckCollisionPointRec(mp, (Rectangle){(float)(px+6),(float)(rowY-2),(float)(pw-12),24.0f}))
        DrawRectangle(px + 6, rowY - 2, pw - 12, 24, (Color){0, 40, 35, 120});

      DrawText(TextFormat("%d", wi + 1), px + 14, rowY + 3, 13, LIGHTGRAY);

      for (int ci = 0; ci < 4; ci++) {
        int bx = px + 54 + ci * 94;
        Color cc = s_waveColColors[ci];
        Rectangle btnMinus = {(float)bx,        (float)rowY, 24, 22};
        Rectangle btnPlus  = {(float)(bx + 54), (float)rowY, 24, 22};
        bool hovM = CheckCollisionPointRec(mp, btnMinus);
        bool hovP = CheckCollisionPointRec(mp, btnPlus);

        DrawRectangleRec(btnMinus, hovM ? ColorAlpha(cc, 0.4f) : (Color){20, 20, 30, 255});
        DrawRectangleLinesEx(btnMinus, 1, ColorAlpha(cc, hovM ? 1.0f : 0.5f));
        DrawText("-", bx + 8, rowY + 4, 13, hovM ? WHITE : ColorAlpha(cc, 0.8f));

        DrawText(TextFormat("%d", ed->edWaves[wi][ci]), bx + 28, rowY + 3, 13, WHITE);

        DrawRectangleRec(btnPlus, hovP ? ColorAlpha(cc, 0.4f) : (Color){20, 20, 30, 255});
        DrawRectangleLinesEx(btnPlus, 1, ColorAlpha(cc, hovP ? 1.0f : 0.5f));
        DrawText("+", bx + 58, rowY + 4, 13, hovP ? WHITE : ColorAlpha(cc, 0.8f));
      }

      Rectangle btnDel = {(float)(px + 435), (float)rowY, 50, 22};
      bool hovDel = CheckCollisionPointRec(mp, btnDel);
      DrawRectangleRec(btnDel, hovDel ? (Color){180, 40, 40, 255} : (Color){40, 15, 15, 255});
      DrawRectangleLinesEx(btnDel, 1.0f, (Color){255, 80, 80, 255});
      DrawText("DEL", px + 443, rowY + 4, 13, hovDel ? WHITE : (Color){255, 120, 120, 255});
    }

    if (ed->edWaveCount < MAX_WAVES) {
      int addY = py + hdr + ed->edWaveCount * rowH + 6;
      Rectangle btnAdd = {(float)(px + pw / 2 - 70), (float)addY, 140, 26};
      bool hovAdd = CheckCollisionPointRec(mp, btnAdd);
      DrawRectangleRec(btnAdd, hovAdd ? (Color){0, 120, 90, 255} : (Color){0, 40, 30, 255});
      DrawRectangleLinesEx(btnAdd, 1.0f, ac);
      DrawText("+ ADD WAVE", px + pw / 2 - 46, addY + 6, 13, hovAdd ? WHITE : ac);
    }

    DrawText("[G] Close  [Ctrl+S] Save", px + pw - 190, py + ph - 18, 12, GRAY);
  }

  // --- Naming overlay ---
  if (ed->isNaming) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, (Color){0, 0, 0, 160});

    int bw = 480, bh = 130;
    int bx = sw / 2 - bw / 2, by = sh / 2 - bh / 2;
    DrawRectangle(bx, by, bw, bh, (Color){30, 30, 40, 240});
    DrawRectangleLines(bx, by, bw, bh, SKYBLUE);
    DrawText("Save level as:", bx + 20, by + 18, 20, RAYWHITE);
    DrawRectangle(bx + 20, by + 52, bw - 40, 36, (Color){15, 15, 25, 255});
    DrawRectangleLines(bx + 20, by + 52, bw - 40, 36, WHITE);

    char display[140];
    snprintf(display, sizeof(display), "%s_", ed->nameBuffer);
    DrawText(display, bx + 28, by + 60, 20, YELLOW);
    DrawText("[ENTER] Confirm   [ESC] Cancel", bx + 20, by + 100, 14, GRAY);
  }

  EndDrawing();
}

void EditorSave(EditorState *ed, GameWorld *gw, const char *path) {
  system("mkdir -p assets/levels");
  FILE *f = fopen(path, "w");
  if (!f)
    return;

  char navPath[256];
  NavmapPathFromLevel(path, navPath, sizeof(navPath));
  const char *missionStr = (ed->missionType == MISSION_EXPLORATION) ? "exploration" : "waves";
  fprintf(f, "{\n  \"terrain\": \"%s\",\n  \"navmap\": \"%s\",\n  \"mission\": \"%s\",\n"
             "  \"hpfade\": %d,\n  \"boxes\": [\n",
          gw->terrainModelPath, navPath, missionStr, ed->healthBarFade ? 1 : 0);
  for (int i = 0; i < ed->placedCount; i++) {
    EditorPlacedBox *b = &ed->placed[i];
    const char *comma = (i < ed->placedCount - 1) ? "," : "";
    fprintf(f,
            "    {\"x\": %.3f, \"y\": %.3f, \"z\": %.3f"
            ", \"sx\": %.3f, \"sy\": %.3f, \"sz\": %.3f}%s\n",
            b->position.x, b->position.y, b->position.z,
            b->scale.x, b->scale.y, b->scale.z, comma);
  }
  fprintf(f, "  ],\n  \"spawners\": [\n");
  for (int i = 0; i < ed->spawnerCount; i++) {
    EditorPlacedSpawner *s = &ed->placedSpawners[i];
    const char *comma = (i < ed->spawnerCount - 1) ? "," : "";
    fprintf(f, "    {\"x\": %.3f, \"y\": %.3f, \"z\": %.3f, \"type\": %d}%s\n",
            s->position.x, s->position.y, s->position.z, s->enemyType, comma);
  }
  fprintf(f, "  ],\n  \"props\": [\n");
  for (int i = 0; i < ed->propCount; i++) {
    EditorPlacedProp *p = &ed->placedProps[i];
    const char *comma = (i < ed->propCount - 1) ? "," : "";
    fprintf(f,
            "    {\"x\": %.3f, \"y\": %.3f, \"z\": %.3f"
            ", \"yaw\": %.4f, \"sx\": %.3f, \"sy\": %.3f, \"sz\": %.3f"
            ", \"model\": \"%s\"}%s\n",
            p->position.x, p->position.y, p->position.z,
            p->yaw, p->scale.x, p->scale.y, p->scale.z,
            p->modelPath, comma);
  }
  fprintf(f, "  ],\n  \"infoboxes\": [\n");
  for (int i = 0; i < ed->infoBoxCount; i++) {
    EditorPlacedInfoBox *ib = &ed->placedInfoBoxes[i];
    const char *comma = (i < ed->infoBoxCount - 1) ? "," : "";
    // Sanitize message: convert \n → | (preserved across save/load), strip JSON-breaking chars
    char safemsg[256];
    strncpy(safemsg, ib->message, 255);
    safemsg[255] = '\0';
    for (int j = 0; safemsg[j]; j++) {
      if (safemsg[j] == '\n') { safemsg[j] = '|'; continue; }
      if (safemsg[j] == '"' || safemsg[j] == '{' || safemsg[j] == '}' || safemsg[j] == '\\')
        safemsg[j] = '\'';
    }
    fprintf(f,
            "    {\"x\": %.3f, \"y\": %.3f, \"z\": %.3f"
            ", \"ext\": %.2f, \"dur\": %.1f, \"trig\": %d"
            ", \"mh\": %.2f, \"fs\": %d, \"msg\": \"%s\"}%s\n",
            ib->position.x, ib->position.y, ib->position.z,
            ib->halfExtent, ib->duration, ib->triggerCount,
            ib->markerHeight, ib->fontSize, safemsg, comma);
  }
  fprintf(f, "  ],\n  \"wallsegs\": [\n");
  for (int i = 0; i < ed->wallSegCount; i++) {
    EditorPlacedWallSeg *ws = &ed->placedWallSegs[i];
    const char *comma = (i < ed->wallSegCount - 1) ? "," : "";
    fprintf(f,
            "    {\"ax\": %.3f, \"az\": %.3f, \"bx\": %.3f, \"bz\": %.3f"
            ", \"yb\": %.3f, \"yt\": %.3f, \"r\": %.3f"
            ", \"bplay\": %d, \"bproj\": %d}%s\n",
            ws->ax, ws->az, ws->bx, ws->bz,
            ws->yBottom, ws->yTop, ws->radius,
            ws->blockPlayer ? 1 : 0, ws->blockProjectiles ? 1 : 0, comma);
  }
  // Targets — static then patrol in a single array
  int totalTargets = ed->targetStaticCount + ed->targetPatrolCount;
  fprintf(f, "  ],\n  \"targets\": [\n");
  for (int i = 0; i < ed->targetStaticCount; i++) {
    EditorPlacedTargetStatic *ts = &ed->placedTargetsStatic[i];
    bool last = (i == totalTargets - 1);
    fprintf(f, "    {\"x\": %.3f, \"y\": %.3f, \"z\": %.3f"
               ", \"yaw\": %.4f, \"hp\": %.1f, \"shield\": %.1f"
               ", \"hdrop\": %d, \"cdrop\": %d}%s\n",
            ts->position.x, ts->position.y, ts->position.z,
            ts->yaw, ts->health, ts->shield,
            ts->healthDropCount, ts->coolantDropCount, last ? "" : ",");
  }
  for (int i = 0; i < ed->targetPatrolCount; i++) {
    EditorPlacedTargetPatrol *tp = &ed->placedTargetsPatrol[i];
    bool last = (i == ed->targetPatrolCount - 1);
    fprintf(f, "    {\"x\": %.3f, \"y\": %.3f, \"z\": %.3f"
               ", \"yaw\": %.4f, \"hp\": %.1f, \"shield\": %.1f"
               ", \"patrol\": 1"
               ", \"x2\": %.3f, \"y2\": %.3f, \"z2\": %.3f"
               ", \"speed\": %.2f"
               ", \"hdrop\": %d, \"cdrop\": %d}%s\n",
            tp->pointA.x, tp->pointA.y, tp->pointA.z,
            tp->yaw, tp->health, tp->shield,
            tp->pointB.x, tp->pointB.y, tp->pointB.z,
            tp->speed, tp->healthDropCount, tp->coolantDropCount, last ? "" : ",");
  }
  fprintf(f, "  ]");

  if (ed->edWaveCount > 0) {
    fprintf(f, ",\n  \"waves\": [\n");
    for (int i = 0; i < ed->edWaveCount; i++) {
      const char *comma = (i < ed->edWaveCount - 1) ? "," : "";
      fprintf(f, "    {\"g\": %d, \"r\": %d, \"m\": %d, \"d\": %d}%s\n",
              ed->edWaves[i][0], ed->edWaves[i][1],
              ed->edWaves[i][2], ed->edWaves[i][3], comma);
    }
    fprintf(f, "  ]");
  }

  if (ed->edHasSpawnPoint)
    fprintf(f, ",\n  \"spawn\": {\"x\": %.3f, \"y\": %.3f, \"z\": %.3f}\n}\n",
            ed->edSpawnPoint.x, ed->edSpawnPoint.y, ed->edSpawnPoint.z);
  else
    fprintf(f, "\n}\n");
  fclose(f);

  ExportImage(ed->navImage, navPath);
}
