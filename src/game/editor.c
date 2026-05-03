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

  ed->spawnerCount = 0;
  ed->placeType    = 0;
  ed->historyTop   = 0;

  HeightMap_Free(&gw->terrainHeightMap);
  gw->terrainHeightMap =
      HeightMap_FromMesh(gw->terrainModel.meshes[0], MatrixIdentity());

  ed->navPaintMode   = false;
  ed->navPaintType   = 0;
  ed->navPaletteOpen = false;
  ed->navBrushSize   = 1;
  if (ed->navImageLoaded) UnloadImage(ed->navImage);
  ed->navImage = LoadImage("assets/navmap.png");
  if (ed->navImage.width != 180 || ed->navImage.height != 180) {
    if (ed->navImage.data) UnloadImage(ed->navImage);
    ed->navImage = GenImageColor(180, 180, WHITE);
  }
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
  ed->historyTop    = 0;

  char *text = LoadFileText(path);
  if (!text) return;

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
      EditorSave(ed, path);
      ed->isNaming = false;
    }
    if (IsKeyPressed(KEY_ESCAPE))
      ed->isNaming = false;
    return;
  }

  // --- ESC toggles pause menu ---
  if (IsKeyPressed(KEY_ESCAPE)) {
    if (ed->navPaletteOpen) {
      ed->navPaletteOpen = false;
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
    ed->navPaintMode  = false;
    ed->transformMode = !ed->transformMode;
    ed->selectedIndex = -1;
  }

  // --- Nav paint toggle ---
  if (IsKeyPressed(KEY_N)) {
    ed->navPaintMode  = !ed->navPaintMode;
    if (ed->navPaintMode) ed->transformMode = false;
    ed->navPaletteOpen = false;
  }

  // --- Camera (always active) ---
  float speed = IsKeyDown(KEY_LEFT_SHIFT) ? 80.0f : 30.0f;
  Vector2 mouse = GetMouseDelta();
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

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
      ed->selectedIndex = PickBox(ed);

    if (ed->selectedIndex >= 0) {
      EditorPlacedBox *b = &ed->placed[ed->selectedIndex];
      bool ctrl  = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
      float step = ctrl ? 5.0f : 0.5f;
      bool changed = false;

      // Move on world axes
      if (IsKeyPressed(KEY_RIGHT))     { b->position.x += step; changed = true; }
      if (IsKeyPressed(KEY_LEFT))      { b->position.x -= step; changed = true; }
      if (IsKeyPressed(KEY_UP))        { b->position.z -= step; changed = true; }
      if (IsKeyPressed(KEY_DOWN))      { b->position.z += step; changed = true; }
      if (IsKeyPressed(KEY_PAGE_UP))   { b->position.y += step; changed = true; }
      if (IsKeyPressed(KEY_PAGE_DOWN)) { b->position.y -= step; changed = true; }

      // Scale via scroll; hold X/Y/Z to constrain axis
      float scroll = GetMouseWheelMove();
      if (scroll != 0.0f) {
        float ss = 0.5f * scroll;
        if (IsKeyDown(KEY_X)) {
          b->scale.x = fmaxf(0.5f, b->scale.x + ss);
        } else if (IsKeyDown(KEY_Y)) {
          b->scale.y = fmaxf(0.5f, b->scale.y + ss);
        } else if (IsKeyDown(KEY_Z)) {
          b->scale.z = fmaxf(0.5f, b->scale.z + ss);
        } else {
          float ns = fmaxf(0.5f, b->scale.x + ss);
          b->scale = (Vector3){ns, ns, ns};
        }
        changed = true;
      }

      if (changed)
        SyncBoxEntity(ed, world, ed->selectedIndex);

      // Delete selected box
      if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)) {
        ModelCollection_t *mc =
            ECS_GET(world, b->entity, ModelCollection_t, COMP_MODEL);
        if (mc) ModelCollectionFree(mc);
        WorldDestroyEntity(world, b->entity);
        for (int i = ed->selectedIndex; i < ed->placedCount - 1; i++)
          ed->placed[i] = ed->placed[i + 1];
        ed->placedCount--;
        ed->selectedIndex = -1;
      }
    }

  } else {
    // --- Place mode ---

    // Sub-type switching: B=box, G=grunt spawner, R=ranger spawner
    if (IsKeyPressed(KEY_B)) ed->placeType = 0;
    if (IsKeyPressed(KEY_G)) ed->placeType = 1;
    if (IsKeyPressed(KEY_R)) ed->placeType = 2;

    // Scroll resizes box scale (only relevant in box mode)
    if (ed->placeType == 0) {
      float scroll = GetMouseWheelMove();
      if (scroll != 0.0f) {
        ed->boxScale += scroll * 0.5f;
        if (ed->boxScale < 0.5f) ed->boxScale = 0.5f;
      }
    }

    ed->hasHit = RaycastTerrain(ed, gw, &ed->hitPos);

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && ed->hasHit) {
      if (ed->placeType == 0 && ed->placedCount < EDITOR_MAX_BOXES) {
        Vector3 scale = {ed->boxScale, ed->boxScale, ed->boxScale};
        entity_t e = SpawnBoxModel(world, gw, ed->hitPos, scale);
        ed->placed[ed->placedCount++] = (EditorPlacedBox){
            .entity   = e,
            .position = ed->hitPos,
            .scale    = scale,
        };
        int top = ed->historyTop;
        if (top < EDITOR_MAX_BOXES + EDITOR_MAX_SPAWNERS)
          ed->history[ed->historyTop++] = 0;
      } else if (ed->placeType > 0 && ed->spawnerCount < EDITOR_MAX_SPAWNERS) {
        int enemyType = ed->placeType - 1; // 0=grunt, 1=ranger
        ed->placedSpawners[ed->spawnerCount++] = (EditorPlacedSpawner){
            .position  = ed->hitPos,
            .enemyType = enemyType,
        };
        int top = ed->historyTop;
        if (top < EDITOR_MAX_BOXES + EDITOR_MAX_SPAWNERS)
          ed->history[ed->historyTop++] = 1;
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

  // Place-mode preview ghost
  if (!ed->transformMode && !ed->navPaintMode && ed->hasHit) {
    if (ed->placeType == 0) {
      float s = ed->boxScale;
      DrawCube(ed->hitPos, s, s, s, (Color){255, 255, 100, 60});
      DrawCubeWires(ed->hitPos, s, s, s, YELLOW);
    } else {
      Color col = (ed->placeType == 1) ? RED : BLUE;
      DrawSphere(ed->hitPos, 1.5f, (Color){col.r, col.g, col.b, 80});
      DrawSphereWires(ed->hitPos, 1.5f, 8, 8, col);
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

    if (ed->selectedIndex >= 0) {
      EditorPlacedBox *b = &ed->placed[ed->selectedIndex];
      DrawText(TextFormat("Pos  X:%.2f  Y:%.2f  Z:%.2f",
                          b->position.x, b->position.y, b->position.z),
               20, 74, 16, YELLOW);
      DrawText(TextFormat("Scale  X:%.2f  Y:%.2f  Z:%.2f",
                          b->scale.x, b->scale.y, b->scale.z),
               20, 94, 16, YELLOW);
      DrawText("[Arrows] Move XZ   [PgUp/Dn] Move Y   [Ctrl] x10 step\n"
               "[Scroll] Scale uniform   [X/Y/Z]+Scroll Scale axis   [Del] Delete",
               20, GetScreenHeight() - 42, 14, WHITE);
    } else {
      DrawText(TextFormat("Boxes: %d", ed->placedCount), 20, 74, 16, LIGHTGRAY);
      DrawText("[LMB] Select box   [TAB] Place mode   [Ctrl+S] Save   [ESC] Exit",
               20, GetScreenHeight() - 28, 14, WHITE);
    }
  } else {
    static const char *placeTypeNames[] = {"BOX", "GRUNT SPAWNER", "RANGER SPAWNER"};
    static Color placeTypeColors[]      = {LIGHTGRAY, RED, BLUE};
    int pt = ed->placeType < 3 ? ed->placeType : 0;
    DrawText(TextFormat("[ %s ]  Boxes: %d  Spawners: %d  Scale: %.1f",
                        placeTypeNames[pt], ed->placedCount,
                        ed->spawnerCount, ed->boxScale),
             20, 48, 18, placeTypeColors[pt]);
    DrawText("[LMB] Place  [Z] Undo  [B] Box  [G] Grunt Spwn  [R] Ranger Spwn  "
             "[Scroll] Resize  [TAB] Transform  [N] Nav Paint  [Ctrl+S] Save  [ESC] Menu",
             20, GetScreenHeight() - 28, 14, WHITE);
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

void EditorSave(EditorState *ed, const char *path) {
  system("mkdir -p assets/levels");
  FILE *f = fopen(path, "w");
  if (!f)
    return;

  fprintf(f, "{\n  \"boxes\": [\n");
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
  fprintf(f, "  ]\n}\n");
  fclose(f);

  ExportImage(ed->navImage, "assets/navmap.png");
}
