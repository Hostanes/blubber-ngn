#include "editor.h"
#include "../engine/util/json_reader.h"
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
  ed->infoBoxCount      = 0;
  ed->infoBoxHalfExtent = 2.5f;
  ed->infoBoxEditOpen   = false;
  ed->infoBoxTextLen    = 0;
  ed->infoBoxTextBuf[0] = '\0';
  ed->infoBoxDuration   = 5.0f;

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
          ib->halfExtent = 2.5f; ib->duration = 5.0f;
          JsonReadFloat(buf, "x",   &ib->position.x);
          JsonReadFloat(buf, "y",   &ib->position.y);
          JsonReadFloat(buf, "z",   &ib->position.z);
          JsonReadFloat(buf, "ext", &ib->halfExtent);
          JsonReadFloat(buf, "dur", &ib->duration);
          JsonReadString(buf, "msg", ib->message, sizeof(ib->message));
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
      if (ch == '"' || ch == '{' || ch == '}' || ch == '\\') continue;
      if (ed->infoBoxTextLen < 255) {
        ed->infoBoxTextBuf[ed->infoBoxTextLen++] = (char)ch;
        ed->infoBoxTextBuf[ed->infoBoxTextLen]   = '\0';
      }
    }
    if (IsKeyPressed(KEY_BACKSPACE) && ed->infoBoxTextLen > 0)
      ed->infoBoxTextBuf[--ed->infoBoxTextLen] = '\0';
    {
      int sw2 = GetScreenWidth(), sh2 = GetScreenHeight();
      int pw2 = 520, px2 = sw2 / 2 - pw2 / 2, py2 = sh2 / 2 - 125;
      Vector2 mouse      = GetMousePosition();
      Rectangle btnMinus = {(float)(px2 + 170), (float)(py2 + 115), 44, 24};
      Rectangle btnPlus  = {(float)(px2 + 224), (float)(py2 + 115), 44, 24};
      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (CheckCollisionPointRec(mouse, btnMinus)) {
          ed->infoBoxDuration -= 5.0f;
          if (ed->infoBoxDuration < 1.0f) ed->infoBoxDuration = 1.0f;
        }
        if (CheckCollisionPointRec(mouse, btnPlus)) {
          ed->infoBoxDuration += 5.0f;
          if (ed->infoBoxDuration > 120.0f) ed->infoBoxDuration = 120.0f;
        }
      }
    }
    if (IsKeyPressed(KEY_ENTER) && ed->infoBoxTextLen > 0) {
      int histCap = EDITOR_MAX_BOXES + EDITOR_MAX_SPAWNERS + EDITOR_MAX_PROPS + EDITOR_MAX_INFOBOXES;
      if (ed->infoBoxCount < EDITOR_MAX_INFOBOXES && ed->historyTop < histCap) {
        EditorPlacedInfoBox *ib = &ed->placedInfoBoxes[ed->infoBoxCount++];
        ib->position   = ed->infoBoxPendingPos;
        ib->halfExtent = ed->infoBoxHalfExtent;
        ib->duration   = ed->infoBoxDuration;
        strncpy(ib->message, ed->infoBoxTextBuf, 255);
        ib->message[255] = '\0';
        ed->history[ed->historyTop++] = 4;
      }
      ed->infoBoxEditOpen = false;
      DisableCursor();
    }
    if (IsKeyPressed(KEY_ESCAPE)) {
      ed->infoBoxEditOpen = false;
      DisableCursor();
    }
    return;
  }

  // --- ESC toggles pause menu ---
  if (IsKeyPressed(KEY_ESCAPE)) {
    if (ed->navPaletteOpen) {
      ed->navPaletteOpen = false;
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
    for (int i = 0; i < 5; i++) {
      Rectangle r = {(float)px, (float)(py + i * (ih + 4)), (float)pw, (float)ih};
      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(mp, r)) {
        ed->placeType       = i;
        ed->objectPanelOpen = false;
        DisableCursor();
      }
    }
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
      // Mouse selection (coordinates match palette render below)
      if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        int pw = 300, ph = 210;
        int ppx = sw/2 - pw/2, ppy = sh/2 - ph/2;
        Vector2 mp = GetMousePosition();
        for (int i = 0; i < 3; i++) {
          Rectangle r = {(float)(ppx+20), (float)(ppy+50+i*48), (float)(pw-40), 38};
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
          Color paintColor;
          if (doErase || ed->navPaintType == 0) {
            paintColor = WHITE;
          } else if (ed->navPaintType == 1) {
            paintColor = BLACK;
          } else {
            paintColor = (Color){0, 0, 255, 255};
          }
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
      // Pick nearest of box or prop; prefer whichever is closer
      int   bi = PickBox(ed),  pi = PickProp(ed);
      float bt = 1e9f,         pt = 1e9f;
      if (bi >= 0) {
        Vector2 c = {(float)GetScreenWidth()/2.0f, (float)GetScreenHeight()/2.0f};
        Ray ray = GetScreenToWorldRay(c, ed->camera);
        BoundingBox bb = {
            Vector3Subtract(ed->placed[bi].position, Vector3Scale(ed->placed[bi].scale, 0.5f)),
            Vector3Add(ed->placed[bi].position,      Vector3Scale(ed->placed[bi].scale, 0.5f)),
        };
        bt = GetRayCollisionBox(ray, bb).distance;
      }
      if (pi >= 0) {
        Vector2 c = {(float)GetScreenWidth()/2.0f, (float)GetScreenHeight()/2.0f};
        Ray ray = GetScreenToWorldRay(c, ed->camera);
        BoundingBox bb = {
            Vector3Subtract(ed->placedProps[pi].position, Vector3Scale(ed->placedProps[pi].scale, 0.5f)),
            Vector3Add(ed->placedProps[pi].position,      Vector3Scale(ed->placedProps[pi].scale, 0.5f)),
        };
        pt = GetRayCollisionBox(ray, bb).distance;
      }
      if (bi >= 0 && bt <= pt) { ed->selectedIndex = bi; ed->selectedType = 0; }
      else if (pi >= 0)        { ed->selectedIndex = pi; ed->selectedType = 1; }
      else                     { ed->selectedIndex = -1; }
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

    ed->hasHit = RaycastTerrain(ed, gw, &ed->hitPos);

    // Only place on LMB when the object panel is not intercepting clicks
    bool panelConsumedClick = false;
    if (ed->objectPanelOpen) panelConsumedClick = true;

    if (!panelConsumedClick && IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && ed->hasHit) {
      int histCap = EDITOR_MAX_BOXES + EDITOR_MAX_SPAWNERS + EDITOR_MAX_PROPS + EDITOR_MAX_INFOBOXES;
      if (ed->placeType == 0 && ed->placedCount < EDITOR_MAX_BOXES) {
        Vector3 scale = {ed->boxScale, ed->boxScale, ed->boxScale};
        entity_t e = SpawnBoxModel(world, gw, ed->hitPos, scale);
        ed->placed[ed->placedCount++] = (EditorPlacedBox){
            .entity   = e,
            .position = ed->hitPos,
            .scale    = scale,
        };
        if (ed->historyTop < histCap) ed->history[ed->historyTop++] = 0;
      } else if ((ed->placeType == 1 || ed->placeType == 2) &&
                 ed->spawnerCount < EDITOR_MAX_SPAWNERS) {
        int enemyType = ed->placeType - 1; // 0=grunt, 1=ranger
        ed->placedSpawners[ed->spawnerCount++] = (EditorPlacedSpawner){
            .position  = ed->hitPos,
            .enemyType = enemyType,
        };
        if (ed->historyTop < histCap) ed->history[ed->historyTop++] = 1;
      } else if (ed->placeType == 3 && ed->propCount < EDITOR_MAX_PROPS) {
        float s = ed->propPlaceScale;
        EditorPlacedProp *pp = &ed->placedProps[ed->propCount++];
        pp->position = ed->hitPos;
        pp->yaw      = ed->propPlaceYaw;
        pp->scale    = (Vector3){s, s, s};
        strncpy(pp->modelPath, ed->propPlaceModel, 255);
        if (ed->historyTop < histCap) ed->history[ed->historyTop++] = 2;
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

  DrawModel(gw->terrainModel,   (Vector3){0, 0, 0}, 1.0f, WHITE);
  DrawModel(gw->ArenaModel175,  (Vector3){0, 0, 0}, 1.0f, WHITE);

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
    Color col = (s->enemyType == 0) ? RED : BLUE;
    DrawSphereWires(s->position, 1.5f, 8, 8, col);
    DrawLine3D(Vector3Add(s->position, (Vector3){-2,0,0}),
               Vector3Add(s->position, (Vector3){ 2,0,0}), col);
    DrawLine3D(Vector3Add(s->position, (Vector3){0,0,-2}),
               Vector3Add(s->position, (Vector3){0,0, 2}), col);
  }

  // Placed info box wireframes
  for (int i = 0; i < ed->infoBoxCount; i++) {
    EditorPlacedInfoBox *ib = &ed->placedInfoBoxes[i];
    float s = ib->halfExtent * 2.0f;
    DrawCube(ib->position, s, s, s, (Color){0, 200, 180, 18});
    DrawCubeWires(ib->position, s, s, s, (Color){0, 220, 200, 255});
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
    }
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
        bool isWall  = (r < 10 && g < 10 && b < 10);
        bool isCover = (b > 200 && r < 50 && g < 50);
        if (!isWall && !isCover) continue;
        float x0 = gx * 2.0f - 180.0f, x1 = x0 + 2.0f;
        float z0 = gz * 2.0f - 180.0f, z1 = z0 + 2.0f;
        float y  = s_navHeightCache[gz][gx] + 0.15f;
        if (isWall) rlColor4ub(220, 40, 40, 160);
        else        rlColor4ub(40, 100, 220, 160);
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
    Color col = (ed->navPaintType == 1) ? RED :
                (ed->navPaintType == 2) ? BLUE : RAYWHITE;
    DrawCubeWires(ed->hitPos, dia, 0.3f, dia, col);
  }

  EndMode3D();

  int cx = GetScreenWidth() / 2;
  int cy = GetScreenHeight() / 2;
  DrawLine(cx - 10, cy, cx + 10, cy, WHITE);
  DrawLine(cx, cy - 10, cx, cy + 10, WHITE);

  // Header
  DrawText("LEVEL EDITOR", 20, 20, 20, RAYWHITE);

  if (ed->navPaintMode) {
    static const char *typeNames[]  = {"EMPTY", "WALL", "LOW COVER"};
    static Color       typeColors[] = {RAYWHITE, RED, BLUE};
    int pt = ed->navPaintType;
    DrawText(TextFormat("[ NAV PAINT ]  Type: %s  Brush: %d",
                        typeNames[pt], ed->navBrushSize),
             20, 48, 18, typeColors[pt]);
    DrawText("[LMB] Paint  [RMB] Erase  [Scroll] Brush size  [P] Palette  "
             "[N] Exit  [Ctrl+S] Save  [ESC] Menu",
             20, GetScreenHeight() - 28, 14, WHITE);

    if (ed->navPaletteOpen) {
      int sw = GetScreenWidth(), sh = GetScreenHeight();
      int pw = 300, ph = 210;
      int ppx = sw/2 - pw/2, ppy = sh/2 - ph/2;
      DrawRectangle(ppx, ppy, pw, ph, (Color){20, 20, 35, 230});
      DrawRectangleLines(ppx, ppy, pw, ph, RAYWHITE);
      DrawText("SELECT TYPE", ppx + 76, ppy + 14, 18, RAYWHITE);
      static const char *labels[]     = {"1: EMPTY", "2: WALL", "3: LOW COVER"};
      static Color swatchColors[]     = {{200,200,200,255}, {50,50,50,255}, {40,100,220,255}};
      Vector2 mp = GetMousePosition();
      for (int i = 0; i < 3; i++) {
        int sy  = ppy + 50 + i * 48;
        bool sel = (i == pt);
        bool hov = CheckCollisionPointRec(mp, (Rectangle){(float)(ppx+20),(float)sy,(float)(pw-40),38});
        DrawRectangle(ppx+20, sy, pw-40, 38, sel ? (Color){50,80,130,255} : (Color){30,30,50,255});
        DrawRectangleLines(ppx+20, sy, pw-40, 38, (hov||sel) ? SKYBLUE : (Color){55,55,75,255});
        DrawRectangle(ppx+30, sy+7, 24, 24, swatchColors[i]);
        DrawRectangleLines(ppx+30, sy+7, 24, 24, LIGHTGRAY);
        DrawText(labels[i], ppx+62, sy+10, 18, sel ? YELLOW : LIGHTGRAY);
      }
      DrawText("[1/2/3] Select  [P/ESC] Close", ppx+20, ppy+ph-28, 13, GRAY);
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
    } else {
      DrawText(TextFormat("Boxes: %d  Props: %d", ed->placedCount, ed->propCount),
               20, 74, 16, LIGHTGRAY);
      DrawText("[LMB] Select  [TAB] Place mode   [Ctrl+S] Save   [ESC] Menu",
               20, GetScreenHeight() - 28, 14, WHITE);
    }
  } else {
    static const char *placeTypeNames[] = {"BOX", "GRUNT SPAWNER", "RANGER SPAWNER", "PROP (VISUAL)", "INFO BOX"};
    static Color placeTypeColors[]      = {LIGHTGRAY, RED, BLUE, GREEN, {0,220,200,255}};
    int pt = (ed->placeType < 5) ? ed->placeType : 0;

    const char *extraInfo = "";
    char extraBuf[64] = "";
    if (ed->placeType == 0)
      snprintf(extraBuf, sizeof(extraBuf), "  Scale: %.1f", ed->boxScale);
    else if (ed->placeType == 3)
      snprintf(extraBuf, sizeof(extraBuf), "  Scale: %.2f  Yaw: %.1f°",
               ed->propPlaceScale, ed->propPlaceYaw * RAD2DEG);
    else if (ed->placeType == 4)
      snprintf(extraBuf, sizeof(extraBuf), "  Size: %.1f  [Scroll] Adjust",
               ed->infoBoxHalfExtent * 2.0f);
    extraInfo = extraBuf;

    DrawText(TextFormat("[ %s ]  Boxes: %d  Spawners: %d  Props: %d  InfoBoxes: %d%s",
                        placeTypeNames[pt], ed->placedCount,
                        ed->spawnerCount, ed->propCount, ed->infoBoxCount, extraInfo),
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

    if (ed->objectPanelOpen) {
      static const char *labels[]     = {"BOX", "GRUNT SPAWNER", "RANGER SPAWNER", "PROP (VISUAL)", "INFO BOX"};
      static Color       itemColors[] = {LIGHTGRAY, RED, BLUE, GREEN, {0,220,200,255}};
      int sw = GetScreenWidth();
      int px = sw - 185, py = 50;
      int pw = 170, ih = 38;
      Vector2 mp = GetMousePosition();
      DrawRectangle(px - 4, py - 4, pw + 8, 5 * (ih + 4) + 8, (Color){10,18,10,240});
      DrawRectangleLines(px - 4, py - 4, pw + 8, 5 * (ih + 4) + 8, SKYBLUE);
      DrawText("[V] Close", px + 4, py - 20, 14, GRAY);
      for (int i = 0; i < 5; i++) {
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
    int pw = 520, ph = 250;
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

    // Message field
    DrawText("MESSAGE:", px + 16, py + 50, 14, LIGHTGRAY);
    DrawRectangle(px + 16, py + 68, pw - 32, 34, (Color){4, 10, 22, 220});
    DrawRectangleLines(px + 16, py + 68, pw - 32, 34, (Color){0, 150, 140, 255});
    char disp[260];
    snprintf(disp, sizeof(disp), "%s%s", ed->infoBoxTextBuf,
             (((int)(GetTime() * 4)) % 2 == 0) ? "_" : " ");
    DrawText(disp, px + 22, py + 76, 14, (Color){100, 255, 170, 255});

    // Duration row
    DrawText("DURATION:", px + 16, py + 120, 14, LIGHTGRAY);
    DrawText(TextFormat("%.0f s", ed->infoBoxDuration), px + 110, py + 118, 18, YELLOW);
    {
      Vector2 mouse      = GetMousePosition();
      Rectangle btnMinus = {(float)(px + 170), (float)(py + 115), 44, 24};
      Rectangle btnPlus  = {(float)(px + 224), (float)(py + 115), 44, 24};
      bool hovM = CheckCollisionPointRec(mouse, btnMinus);
      bool hovP = CheckCollisionPointRec(mouse, btnPlus);
      DrawRectangleRec(btnMinus, hovM ? (Color){0,160,140,255} : (Color){0,50,45,255});
      DrawRectangleLinesEx(btnMinus, 1.0f, (Color){0,210,190,255});
      DrawText("-5", px + 184, py + 119, 13, hovM ? WHITE : (Color){0,230,210,255});
      DrawRectangleRec(btnPlus,  hovP ? (Color){0,160,140,255} : (Color){0,50,45,255});
      DrawRectangleLinesEx(btnPlus,  1.0f, (Color){0,210,190,255});
      DrawText("+5", px + 236, py + 119, 13, hovP ? WHITE : (Color){0,230,210,255});
    }

    // Trigger size row
    DrawText("TRIGGER SIZE:", px + 16, py + 152, 14, LIGHTGRAY);
    DrawText(TextFormat("%.1f m", ed->infoBoxHalfExtent * 2.0f), px + 130, py + 150, 18, YELLOW);
    DrawText("scroll to adjust", px + 200, py + 154, 13, GRAY);

    // Hints
    if (ed->infoBoxTextLen == 0)
      DrawText("(type message above)", px + 16, py + 185, 13, (Color){120, 120, 120, 255});
    DrawText("[ENTER] Confirm", px + 16, py + ph - 36, 14, (Color){80, 230, 120, 255});
    DrawText("[ESC] Cancel", px + pw - 130, py + ph - 36, 14, (Color){230, 80, 80, 255});
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
  fprintf(f, "{\n  \"terrain\": \"%s\",\n  \"navmap\": \"%s\",\n  \"mission\": \"%s\",\n  \"boxes\": [\n",
          gw->terrainModelPath, navPath, missionStr);
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
    // Sanitize message: replace chars that break minimal JSON parser
    char safemsg[256];
    strncpy(safemsg, ib->message, 255);
    safemsg[255] = '\0';
    for (int j = 0; safemsg[j]; j++) {
      if (safemsg[j] == '"' || safemsg[j] == '{' || safemsg[j] == '}' || safemsg[j] == '\\')
        safemsg[j] = '\'';
    }
    fprintf(f,
            "    {\"x\": %.3f, \"y\": %.3f, \"z\": %.3f"
            ", \"ext\": %.2f, \"dur\": %.1f, \"msg\": \"%s\"}%s\n",
            ib->position.x, ib->position.y, ib->position.z,
            ib->halfExtent, ib->duration, safemsg, comma);
  }
  fprintf(f, "  ]\n}\n");
  fclose(f);

  ExportImage(ed->navImage, navPath);
}
