
// map_maker.c
#include "raylib.h"
#include <stdint.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define TILE_SIZE 48
#define MAX_TYPES 4 // example: 0=flat, 1=ramp, 2=corner, 3=obstacle

typedef struct {
  uint8_t type;     // 0 = flat, 1 = ramp, etc.
  int8_t height;    // e.g. -3..+3 levels
  uint8_t rotation; // 0,1,2,3 (each = 90°)
} MapChunk;

typedef struct {
  int width;
  int height;
  MapChunk *chunks;
} MapGrid;

// ---------------- File IO ----------------

void SaveMapGrid(const char *path, MapGrid *map) {
  FILE *f = fopen(path, "wb");
  if (!f)
    return;
  fwrite(&map->width, sizeof(int), 1, f);
  fwrite(&map->height, sizeof(int), 1, f);
  fwrite(map->chunks, sizeof(MapChunk), map->width * map->height, f);
  fclose(f);
  printf("Saved map: %s (%dx%d)\n", path, map->width, map->height);
}

MapGrid *LoadMapGrid(const char *path) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;
  MapGrid *map = malloc(sizeof(MapGrid));
  fread(&map->width, sizeof(int), 1, f);
  fread(&map->height, sizeof(int), 1, f);
  map->chunks = malloc(sizeof(MapChunk) * map->width * map->height);
  fread(map->chunks, sizeof(MapChunk), map->width * map->height, f);
  fclose(f);
  printf("Loaded map: %s (%dx%d)\n", path, map->width, map->height);
  return map;
}

// ---------------- Helpers ----------------

Color GetColorForType(uint8_t type) {
  switch (type) {
  case 0:
    return (Color){100, 200, 100, 255}; // flat
  case 1:
    return (Color){200, 150, 100, 255}; // ramp
  case 2:
    return (Color){100, 100, 200, 255}; // corner
  case 3:
    return (Color){200, 100, 150, 255}; // obstacle
  default:
    return GRAY;
  }
}

void DrawCenteredText(const char *text, Rectangle rect, int fontSize, Color c) {
  int w = MeasureText(text, fontSize);
  DrawText(text, rect.x + rect.width / 2 - w / 2,
           rect.y + rect.height / 2 - fontSize / 2, fontSize, c);
}

// ---------------- Main ----------------

int main(void) {
  const int mapW = 12;
  const int mapH = 10;
  const int screenW = mapW * TILE_SIZE + 200;
  const int screenH = mapH * TILE_SIZE + 100;
  InitWindow(screenW, screenH, "Map Maker");

  MapGrid map = {0};
  map.width = mapW;
  map.height = mapH;
  map.chunks = calloc(mapW * mapH, sizeof(MapChunk));

  int currentType = 0;
  int currentHeight = 0;
  int currentRot = 0;

  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    Vector2 mouse = GetMousePosition();
    int gx = (int)(mouse.x / TILE_SIZE);
    int gz = (int)(mouse.y / TILE_SIZE);
    bool inside = (gx >= 0 && gx < map.width && gz >= 0 && gz < map.height);

    // --- Input ---
    if (inside && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
      MapChunk *c = &map.chunks[gz * map.width + gx];
      c->type = currentType;
      c->height = currentHeight;
      c->rotation = currentRot;
    }

    // Change paint parameters
    if (IsKeyPressed(KEY_ONE))
      currentType = (currentType + 1) % MAX_TYPES;
    if (IsKeyPressed(KEY_TWO))
      currentHeight++;
    if (IsKeyPressed(KEY_THREE))
      currentHeight--;
    if (IsKeyPressed(KEY_FOUR))
      currentRot = (currentRot + 1) % 4;

    // Clamp
    if (currentHeight > 5)
      currentHeight = 5;
    if (currentHeight < -5)
      currentHeight = -5;

    // Save/load
    if (IsKeyPressed(KEY_S))
      SaveMapGrid("map.bin", &map);
    if (IsKeyPressed(KEY_L)) {
      MapGrid *loaded = LoadMapGrid("map.bin");
      if (loaded) {
        free(map.chunks);
        map = *loaded;
        free(loaded);
      }
    }

    // --- Draw ---
    BeginDrawing();
    ClearBackground((Color){25, 25, 35, 255});

    for (int z = 0; z < map.height; z++) {
      for (int x = 0; x < map.width; x++) {
        MapChunk *chunk = &map.chunks[z * map.width + x];
        Rectangle rect = {x * TILE_SIZE, z * TILE_SIZE, TILE_SIZE, TILE_SIZE};
        DrawRectangleRec(rect, GetColorForType(chunk->type));
        DrawRectangleLinesEx(rect, 1, BLACK);

        char buf[32];
        snprintf(buf, sizeof(buf), "%d", chunk->height);
        DrawCenteredText(buf, rect, 14, WHITE);

        // draw small arrow for rotation
        Vector2 center = {rect.x + TILE_SIZE / 2, rect.y + TILE_SIZE / 2};
        float len = 10;
        float ang = chunk->rotation * 90.0f * DEG2RAD;
        Vector2 tip = {center.x + cosf(ang) * len, center.y + sinf(ang) * len};
        DrawLineV(center, tip, BLACK);
        DrawCircleV(tip, 2, RED);
      }
    }

    // UI panel
    int panelX = map.width * TILE_SIZE + 20;
    DrawText("Controls:", panelX, 40, 16, RAYWHITE);
    DrawText("[1] Change Type", panelX, 70, 14, GRAY);
    DrawText("[2] Height +", panelX, 90, 14, GRAY);
    DrawText("[3] Height -", panelX, 110, 14, GRAY);
    DrawText("[4] Rotate", panelX, 130, 14, GRAY);
    DrawText("[S] Save map.bin", panelX, 160, 14, GRAY);
    DrawText("[L] Load map.bin", panelX, 180, 14, GRAY);

    DrawText(TextFormat("Current Type: %d", currentType), panelX, 210, 16,
             GREEN);
    DrawText(TextFormat("Current Height: %d", currentHeight), panelX, 230, 16,
             GREEN);
    DrawText(TextFormat("Rotation: %d", currentRot * 90), panelX, 250, 16,
             GREEN);

    EndDrawing();
  }

  free(map.chunks);
  CloseWindow();
  return 0;
}
