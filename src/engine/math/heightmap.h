
#pragma once
#include "raylib.h"
#include "raymath.h"
#include "stdlib.h"
#include <float.h>
#include <stdint.h>

typedef struct {
  uint32_t width;  // grid width (X)
  uint32_t height; // grid height (Z)

  float cellSize; // world units per cell
  Vector3 origin; // world-space origin (min corner)

  float *samples; // size = width * height
} HeightMap;

HeightMap HeightMap_FromMesh(Mesh mesh, Matrix transform);
float HeightMap_GetHeightSmooth(const HeightMap *hm, float x, float z);
float HeightMap_GetHeightCatmullRom(const HeightMap *hm, float x, float z);
