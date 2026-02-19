#include "heightmap.h"

HeightMap HeightMap_FromMesh(Mesh mesh, Matrix transform) {
  HeightMap hm = {0};

  // ---- 1. Transform vertices ----
  Vector3 *verts = (Vector3 *)mesh.vertices;
  unsigned short *indices = (unsigned short *)mesh.indices;

  int vertexCount = mesh.vertexCount;
  int triCount = mesh.triangleCount;

  Vector3 *tverts = malloc(sizeof(Vector3) * vertexCount);

  for (int i = 0; i < vertexCount; i++) {
    tverts[i] = Vector3Transform(verts[i], transform);
  }

  // ---- 2. Compute bounds in XZ ----
  Vector3 min = tverts[0];
  Vector3 max = tverts[0];

  for (int i = 1; i < vertexCount; i++) {
    min.x = fminf(min.x, tverts[i].x);
    min.y = fminf(min.y, tverts[i].y);
    min.z = fminf(min.z, tverts[i].z);

    max.x = fmaxf(max.x, tverts[i].x);
    max.y = fmaxf(max.y, tverts[i].y);
    max.z = fmaxf(max.z, tverts[i].z);
  }

  // ---- 3. Grid resolution ----

  hm.cellSize = 0.25f; // you can tweak this later
  hm.origin = (Vector3){min.x, 0.0f, min.z};

  hm.width = (uint32_t)ceilf((max.x - min.x) / hm.cellSize) + 1;
  hm.height = (uint32_t)ceilf((max.z - min.z) / hm.cellSize) + 1;

  hm.samples = malloc(sizeof(float) * hm.width * hm.height);

  // Clear heightmap
  for (uint32_t i = 0; i < hm.width * hm.height; i++) {
    hm.samples[i] = -FLT_MAX;
  }

  // ---- 4. Rasterize triangles ----
  for (int t = 0; t < triCount; t++) {
    int i0 = indices[t * 3 + 0];
    int i1 = indices[t * 3 + 1];
    int i2 = indices[t * 3 + 2];

    Vector3 v0 = tverts[i0];
    Vector3 v1 = tverts[i1];
    Vector3 v2 = tverts[i2];

    float minTx = fminf(v0.x, fminf(v1.x, v2.x));
    float maxTx = fmaxf(v0.x, fmaxf(v1.x, v2.x));
    float minTz = fminf(v0.z, fminf(v1.z, v2.z));
    float maxTz = fmaxf(v0.z, fmaxf(v1.z, v2.z));

    int ix0 = (int)((minTx - hm.origin.x) / hm.cellSize);
    int ix1 = (int)((maxTx - hm.origin.x) / hm.cellSize);
    int iz0 = (int)((minTz - hm.origin.z) / hm.cellSize);
    int iz1 = (int)((maxTz - hm.origin.z) / hm.cellSize);

    ix0 = Clamp(ix0, 0, hm.width - 1);
    ix1 = Clamp(ix1, 0, hm.width - 1);
    iz0 = Clamp(iz0, 0, hm.height - 1);
    iz1 = Clamp(iz1, 0, hm.height - 1);

    Vector2 a = {v0.x, v0.z};
    Vector2 b = {v1.x, v1.z};
    Vector2 c = {v2.x, v2.z};

    float denom = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);

    if (fabsf(denom) < 1e-6f)
      continue;

    for (int z = iz0; z <= iz1; z++) {
      for (int x = ix0; x <= ix1; x++) {
        float wx = hm.origin.x + x * hm.cellSize;
        float wz = hm.origin.z + z * hm.cellSize;

        Vector2 p = {wx, wz};

        float u =
            ((b.y - c.y) * (p.x - c.x) + (c.x - b.x) * (p.y - c.y)) / denom;
        float v =
            ((c.y - a.y) * (p.x - c.x) + (a.x - c.x) * (p.y - c.y)) / denom;
        float w = 1.0f - u - v;

        if (u < 0 || v < 0 || w < 0)
          continue;

        float h = u * v0.y + v * v1.y + w * v2.y;

        float *cell = &hm.samples[z * hm.width + x];
        if (h > *cell)
          *cell = h;
      }
    }
  }

  free(tverts);
  return hm;
}

float HeightMap_GetHeightSmooth(const HeightMap *hm, float x, float z) {
  float fx = (x - hm->origin.x) / hm->cellSize;
  float fz = (z - hm->origin.z) / hm->cellSize;

  int x0 = (int)floorf(fx);
  int z0 = (int)floorf(fz);
  int x1 = x0 + 1;
  int z1 = z0 + 1;

  if (x0 < 0 || z0 < 0 || x1 >= (int)hm->width || z1 >= (int)hm->height)
    return 0.0f;

  float h00 = hm->samples[z0 * hm->width + x0];
  float h10 = hm->samples[z0 * hm->width + x1];
  float h01 = hm->samples[z1 * hm->width + x0];
  float h11 = hm->samples[z1 * hm->width + x1];

  float tx = fx - x0;
  float tz = fz - z0;

  float h0 = Lerp(h00, h10, tx);
  float h1 = Lerp(h01, h11, tx);

  return Lerp(h0, h1, tz);
}

static float catmullRomInterpolate(float p0, float p1, float p2, float p3,
                                   float t) {
  return 0.5f *
         ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t +
          (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t);
}

float HeightMap_GetHeightCatmullRom(const HeightMap *hm, float x, float z) {
  float fx = (x - hm->origin.x) / hm->cellSize;
  float fz = (z - hm->origin.z) / hm->cellSize;

  int ix = (int)floorf(fx);
  int iz = (int)floorf(fz);

  if (ix < 1 || iz < 1 || ix >= (int)hm->width - 2 || iz >= (int)hm->height - 2)
    return HeightMap_GetHeightSmooth(hm, x, z);

  float tx = fx - ix;
  float tz = fz - iz;

  float p[4][4];
  for (int j = 0; j < 4; j++) {
    for (int i = 0; i < 4; i++) {
      p[j][i] = hm->samples[(iz - 1 + j) * hm->width + (ix - 1 + i)];
    }
  }

  float col[4];
  for (int j = 0; j < 4; j++) {
    col[j] = catmullRomInterpolate(p[j][0], p[j][1], p[j][2], p[j][3], tx);
  }

  return catmullRomInterpolate(col[0], col[1], col[2], col[3], tz);
}
