#pragma once
#include "raylib.h"

typedef struct {
    float   respawnTimer;   // > 0 = counting down to respawn, <= 0 = alive
    float   maxHealth;
    float   maxShield;
    Vector3 spawnPos;
    float   spawnYaw;
    int     healthDropCount;
    int     coolantDropCount;
} TargetDummy;

typedef struct {
    Vector3 pointA;
    Vector3 pointB;
    float   speed;          // world units per second
    float   t;              // lerp parameter 0..1
    int     dir;            // +1 = A→B, -1 = B→A
} TargetPatrol;
