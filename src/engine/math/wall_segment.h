#pragma once
#include "raylib.h"
#include "raymath.h"
#include <math.h>

typedef struct {
    Vector3 localA;
    Vector3 localB;
    float   localYBottom;
    float   localYTop;

    Vector3 worldA;   // XZ endpoints in world space (y = 0)
    Vector3 worldB;
    float   yBottom;
    float   yTop;

    float   radius;   // half-thickness
} WallSegmentCollider;

static inline void WallSegment_UpdateWorld(WallSegmentCollider *w, Vector3 position) {
    w->worldA  = (Vector3){position.x + w->localA.x, 0.0f, position.z + w->localA.z};
    w->worldB  = (Vector3){position.x + w->localB.x, 0.0f, position.z + w->localB.z};
    w->yBottom = position.y + w->localYBottom;
    w->yTop    = position.y + w->localYTop;
}

static inline BoundingBox WallSegment_ComputeAABB(const WallSegmentCollider *w) {
    return (BoundingBox){
        .min = {
            fminf(w->worldA.x, w->worldB.x) - w->radius,
            w->yBottom,
            fminf(w->worldA.z, w->worldB.z) - w->radius,
        },
        .max = {
            fmaxf(w->worldA.x, w->worldB.x) + w->radius,
            w->yTop,
            fmaxf(w->worldA.z, w->worldB.z) + w->radius,
        },
    };
}
